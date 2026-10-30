// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "K2Node_RunReactiveAction.h"

#include "EdGraphSchema_K2.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "ReactiveAction.h"
#include "ReactiveActorsLibrary.h"

#define LOCTEXT_NAMESPACE "K2Node_RunReactiveAction"

namespace RunReactiveActionPins
{
	// Unusual names on purpose: Construct Object from Class matches pins to the action's variables by name,
	// so these must never clash with a variable called Target or Instigator on an action Blueprint
	const FName Target(TEXT("RunAction_Target"));
	const FName Instigator(TEXT("RunAction_Instigator"));
	const FName Started(TEXT("RunAction_Started"));
}

void UK2Node_RunReactiveAction::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	UEdGraphPin* TargetPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, AActor::StaticClass(), RunReactiveActionPins::Target);
	TargetPin->PinFriendlyName = LOCTEXT("TargetPin", "Target");
	TargetPin->PinToolTip = LOCTEXT("TargetTooltip", "The actor the action acts on.").ToString();

	UEdGraphPin* InstigatorPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, AActor::StaticClass(), RunReactiveActionPins::Instigator);
	InstigatorPin->PinFriendlyName = LOCTEXT("InstigatorPin", "Instigator");
	InstigatorPin->PinToolTip = LOCTEXT("InstigatorTooltip", "Who caused it (optional): what Get Instigator returns inside the action.").ToString();

	UEdGraphPin* StartedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, RunReactiveActionPins::Started);
	StartedPin->PinFriendlyName = LOCTEXT("StartedPin", "Started");
	StartedPin->PinToolTip = LOCTEXT("StartedTooltip",
		"False if it didn't run: no class, no target, or a Class Requirement failed on the target.").ToString();

	if (UEdGraphPin* ResultPin = GetResultPin())
	{
		ResultPin->PinFriendlyName = LOCTEXT("ActionPin", "Action");
	}
}

FText UK2Node_RunReactiveAction::GetTooltipText() const
{
	return LOCTEXT("Tooltip",
		"Makes an action of the picked class and runs it once on Target, without a Reaction Component.\n"
		"Its variables marked Instance Editable and Expose on Spawn show as pins.\n"
		"Actions that take time keep running until they call Finish Action or are cancelled.");
}

FText UK2Node_RunReactiveAction::GetKeywords() const
{
	return LOCTEXT("Keywords", "run reactive action construct create class");
}

FText UK2Node_RunReactiveAction::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "Reactive Actors");
}

void UK2Node_RunReactiveAction::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// Hidden for now: not offered in the node menu. To bring it back, call Super::GetMenuActions(ActionRegistrar).
}

FText UK2Node_RunReactiveAction::GetBaseNodeTitle() const
{
	return LOCTEXT("BaseTitle", "Run Action");
}

FText UK2Node_RunReactiveAction::GetNodeTitleFormat() const
{
	return LOCTEXT("TitleFormat", "Run {ClassName}");
}

UClass* UK2Node_RunReactiveAction::GetClassPinBaseClass() const
{
	return UReactiveAction::StaticClass();
}

bool UK2Node_RunReactiveAction::IsSpawnVarPin(UEdGraphPin* Pin) const
{
	// Our own pins stay when the class changes; only the action's variables come and go
	return Super::IsSpawnVarPin(Pin)
		&& Pin->PinName != RunReactiveActionPins::Target
		&& Pin->PinName != RunReactiveActionPins::Instigator
		&& Pin->PinName != RunReactiveActionPins::Started;
}

UEdGraphPin* UK2Node_RunReactiveAction::GetTargetPin() const
{
	return FindPinChecked(RunReactiveActionPins::Target);
}

UEdGraphPin* UK2Node_RunReactiveAction::GetInstigatorPin() const
{
	return FindPinChecked(RunReactiveActionPins::Instigator);
}

UEdGraphPin* UK2Node_RunReactiveAction::GetStartedPin() const
{
	return FindPinChecked(RunReactiveActionPins::Started);
}

void UK2Node_RunReactiveAction::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	UEdGraphPin* ClassPin = GetClassPin();
	UClass* ClassToSpawn = GetClassToSpawn();

	if (!ClassPin || (ClassPin->LinkedTo.Num() == 0 && !ClassToSpawn))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("NoClass", "@@ needs an action class.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	// Actions that only work inside a reaction (Available in Blueprint Run Action off in their Class Defaults)
	if (ClassToSpawn)
	{
		const UReactiveAction* Defaults = ClassToSpawn->GetDefaultObject<UReactiveAction>();
		if (Defaults && !Defaults->IsAvailableInBlueprintRunAction())
		{
			CompilerContext.MessageLog.Error(*FText::Format(
				LOCTEXT("NotAllowed", "@@: {0} can only be used inside a reaction, not with Run Action (Available in Blueprint Run Action is off)."),
				ClassToSpawn->GetDisplayNameText()).ToString(), this);
			BreakAllNodeLinks();
			return;
		}
	}

	bool bSucceeded = true;

	// 1. Make the action
	UK2Node_CallFunction* CreateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CreateNode->FunctionReference.SetExternalMember(
		GET_FUNCTION_NAME_CHECKED(UReactiveActorsLibrary, CreateActionForRun), UReactiveActorsLibrary::StaticClass());
	CreateNode->AllocateDefaultPins();

	UEdGraphPin* CreateClassPin = CreateNode->FindPinChecked(TEXT("ActionClass"));
	UEdGraphPin* CreateResultPin = CreateNode->GetReturnValuePin();

	bSucceeded &= CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CreateNode->GetExecPin()).CanSafeConnect();
	bSucceeded &= CompilerContext.MovePinLinksToIntermediate(*ClassPin, *CreateClassPin).CanSafeConnect();

	// Typed as the picked class (as Construct Object from Class does), so the variables can be set on it
	UEdGraphPin* ResultPin = GetResultPin();
	CreateResultPin->PinType = ResultPin->PinType;
	bSucceeded &= CompilerContext.MovePinLinksToIntermediate(*ResultPin, *CreateResultPin).CanSafeConnect();

	// 2. Set its Expose on Spawn variables from the node's pins
	UEdGraphPin* LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes(
		CompilerContext, SourceGraph, CreateNode, this, CreateResultPin, ClassToSpawn);

	// 3. Run it
	UK2Node_CallFunction* RunNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	RunNode->FunctionReference.SetExternalMember(
		GET_FUNCTION_NAME_CHECKED(UReactiveActorsLibrary, RunAction), UReactiveActorsLibrary::StaticClass());
	RunNode->AllocateDefaultPins();

	bSucceeded &= LastThen && Schema->TryCreateConnection(LastThen, RunNode->GetExecPin());
	bSucceeded &= Schema->TryCreateConnection(CreateResultPin, RunNode->FindPinChecked(TEXT("Action")));
	bSucceeded &= CompilerContext.MovePinLinksToIntermediate(*GetTargetPin(), *RunNode->FindPinChecked(TEXT("Target"))).CanSafeConnect();
	bSucceeded &= CompilerContext.MovePinLinksToIntermediate(*GetInstigatorPin(), *RunNode->FindPinChecked(TEXT("Instigator"))).CanSafeConnect();
	bSucceeded &= CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *RunNode->GetThenPin()).CanSafeConnect();
	bSucceeded &= CompilerContext.MovePinLinksToIntermediate(*GetStartedPin(), *RunNode->GetReturnValuePin()).CanSafeConnect();

	// World context: only shown in Blueprints without one (function libraries). Elsewhere both calls use Self.
	if (UEdGraphPin* WorldContextPin = GetWorldContextPin())
	{
		UEdGraphPin* CreateWorldContext = CreateNode->FindPin(TEXT("WorldContextObject"));
		UEdGraphPin* RunWorldContext = RunNode->FindPin(TEXT("WorldContextObject"));
		if (CreateWorldContext && RunWorldContext)
		{
			bSucceeded &= CompilerContext.CopyPinLinksToIntermediate(*WorldContextPin, *CreateWorldContext).CanSafeConnect();
			bSucceeded &= CompilerContext.MovePinLinksToIntermediate(*WorldContextPin, *RunWorldContext).CanSafeConnect();
		}
	}

	if (!bSucceeded)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("ExpandFailed", "Internal error expanding @@.").ToString(), this);
	}

	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE

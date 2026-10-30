// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveAssetFactories.h"

#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "IAssetTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ReactionPreset.h"
#include "ReactiveAction.h"
#include "ReactiveRequirement.h"
#include "ReactiveRule.h"
#include "ReactiveTrigger.h"

namespace
{
	uint32 GetReactiveCategory()
	{
		// Registered by the editor module at startup; Misc if that hasn't happened for some reason
		const EAssetTypeCategories::Type Category =
			FAssetToolsModule::GetModule().Get().FindAdvancedAssetCategory(ReactiveActorsEditor::AssetCategoryName);
		return Category != EAssetTypeCategories::None ? static_cast<uint32>(Category) : static_cast<uint32>(EAssetTypeCategories::Misc);
	}
}

// ---- Blueprint factories ------------------------------------------------------------------------

UReactiveBlueprintFactory::UReactiveBlueprintFactory()
{
	SupportedClass = UBlueprint::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;
	bText = false;
}

UObject* UReactiveBlueprintFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UClass* Parent = BlueprintParentClass.Get();
	if (!Parent || !FKismetEditorUtilities::CanCreateBlueprintOfClass(Parent))
	{
		return nullptr;
	}

	return FKismetEditorUtilities::CreateBlueprint(Parent, InParent, InName, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UReactiveBlueprintFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(InClass, InParent, InName, Flags, Context, Warn, NAME_None);
}

uint32 UReactiveBlueprintFactory::GetMenuCategories() const
{
	return GetReactiveCategory();
}

FText UReactiveBlueprintFactory::GetDisplayName() const
{
	// The parent class's display name: Reactive Action, Reactive Requirement...
	const UClass* Parent = BlueprintParentClass.Get();
	return Parent ? Parent->GetDisplayNameText() : Super::GetDisplayName();
}

FText UReactiveBlueprintFactory::GetToolTip() const
{
	return ToolTip;
}

FString UReactiveBlueprintFactory::GetDefaultNewAssetName() const
{
	return DefaultName;
}

FName UReactiveBlueprintFactory::GetNewAssetThumbnailOverride() const
{
	// Registered by FReactiveActorsStyle: ClassThumbnail.ReactiveAction, ClassThumbnail.ReactiveRequirement...
	const UClass* Parent = BlueprintParentClass.Get();
	return Parent ? FName(*(TEXT("ClassThumbnail.") + Parent->GetName())) : NAME_None;
}

UReactiveActionFactory::UReactiveActionFactory()
{
	BlueprintParentClass = UReactiveAction::StaticClass();
	DefaultName = TEXT("ACT_NewAction");
	ToolTip = INVTEXT("A new Reactive Action: one thing done to one actor. Implement Execute Action and call Finish Action when it's done.");
}

UReactiveRequirementFactory::UReactiveRequirementFactory()
{
	BlueprintParentClass = UReactiveRequirement::StaticClass();
	DefaultName = TEXT("REQ_NewRequirement");
	ToolTip = INVTEXT("A new Reactive Requirement: a check about the moment, for action requirements, Branch conditions and trigger conditions. Override Check Requirement.");
}

UReactiveRuleFactory::UReactiveRuleFactory()
{
	BlueprintParentClass = UReactiveRule::StaticClass();
	DefaultName = TEXT("RULE_NewRule");
	ToolTip = INVTEXT("A new Reactive Rule: a rule with memory on a reaction or trigger (like Cooldown). Override Should Block, On Start or On Finish.");
}

UReactiveTriggerFactory::UReactiveTriggerFactory()
{
	BlueprintParentClass = UReactiveTrigger::StaticClass();
	DefaultName = TEXT("TRG_NewTrigger");
	ToolTip = INVTEXT("A new Reactive Trigger: watches for something and sends an event. Bind in On Activate, unbind in On Deactivate, call Fire Trigger.");
}

// ---- Reaction Preset ----------------------------------------------------------------------------

UReactionPresetFactory::UReactionPresetFactory()
{
	SupportedClass = UReactionPreset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;
	bText = false;
}

UObject* UReactionPresetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UReactionPreset>(InParent, InClass, InName, Flags);
}

uint32 UReactionPresetFactory::GetMenuCategories() const
{
	return GetReactiveCategory();
}

FText UReactionPresetFactory::GetToolTip() const
{
	return INVTEXT("A new Reaction Preset: reactions and triggers to share between actors (set it as a Reaction Component's Preset).");
}

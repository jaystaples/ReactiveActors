// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveObjectBase.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectThreadContext.h"

namespace
{
	bool ImplementsScriptSummary(const UClass* Class)
	{
		static const FName GetSummaryName = GET_FUNCTION_NAME_CHECKED(UReactiveObjectBase, GetSummary);
		return Class && Class->IsFunctionImplementedInScript(GetSummaryName);
	}

	/** True while Blueprint code can't safely run on Object (being made, loaded or recompiled). */
	bool IsUnsafeForScript(const UObject* Object)
	{
		return !IsInGameThread()
			|| Object->HasAnyFlags(RF_NeedInitialization | RF_NeedLoad | RF_NeedPostLoad)
			|| FUObjectThreadContext::Get().IsRoutingPostLoad
			|| Object->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists);
	}
}

UObject* UReactiveObjectBase::GetOwnerObject() const
{
	// Walk past enclosing actions (Branch, Run On Targets) to whatever is running them
	UObject* Outer = GetOuter();
	while (Outer && Outer->IsA<UReactiveObjectBase>())
	{
		Outer = Outer->GetOuter();
	}
	return Outer;
}

AActor* UReactiveObjectBase::GetOwningActor() const
{
	return GetTypedOuter<AActor>();
}

UWorld* UReactiveObjectBase::GetWorld() const
{
	// The CDO and archetypes must return null (without calling Super) so the
	// engine treats this class as world-context capable in Blueprint graphs
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return nullptr;
	}

	const UObject* Outer = GetOuter();
	return Outer ? Outer->GetWorld() : nullptr;
}

FString UReactiveObjectBase::BuildSummary() const
{
	return ScriptSummaryOr(GetClass()->GetDisplayNameText().ToString());
}

void UReactiveObjectBase::CollectSettingsProblems(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return; // Class Defaults often leave settings empty on purpose: each use fills them in
	}

	static const FName ValidateName = GET_FUNCTION_NAME_CHECKED(UReactiveObjectBase, ValidateSettings);
	if (GetClass()->IsFunctionImplementedInScript(ValidateName) && !IsUnsafeForScript(this))
	{
		ValidateSettings(OutErrors, OutWarnings); // the Blueprint override (it can call the parent for the C++ checks)
	}
	else
	{
		ValidateSettings_Implementation(OutErrors, OutWarnings);
	}
}

FString UReactiveObjectBase::ScriptSummaryOr(const FString& Fallback) const
{
	// Only Blueprint subclasses that implement it, and never while the object or its class is still being
	// made, loaded or recompiled: Blueprint code can't safely run then
	const UClass* Class = GetClass();
	if (!IsInGameThread()
		|| HasAnyFlags(RF_ClassDefaultObject | RF_NeedInitialization | RF_NeedLoad | RF_NeedPostLoad)
		|| FUObjectThreadContext::Get().IsRoutingPostLoad
		|| Class->HasAnyClassFlags(CLASS_NewerVersionExists)
		|| !ImplementsScriptSummary(Class))
	{
		return Fallback;
	}

	const FString FromScript = GetSummary();
	return FromScript.IsEmpty() ? Fallback : FromScript;
}

#if WITH_EDITOR
void UReactiveObjectBase::RefreshSummary()
{
	Summary = BuildSummary();
}

void UReactiveObjectBase::RefreshSummarySoon()
{
	TWeakObjectPtr<UReactiveObjectBase> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			// This block, then the blocks around it (a Branch listing this condition)
			for (UObject* Object = WeakThis.Get(); Object; Object = Object->GetOuter())
			{
				if (UReactiveObjectBase* Block = Cast<UReactiveObjectBase>(Object))
				{
					Block->RefreshSummary();
				}
				else
				{
					break;
				}
			}
			return false; // once
		}));
}

void UReactiveObjectBase::PostInitProperties()
{
	Super::PostInitProperties();
	RefreshSummary(); // a new block: its defaults
	if (!HasAnyFlags(RF_ClassDefaultObject) && ImplementsScriptSummary(GetClass()))
	{
		RefreshSummarySoon(); // Get Summary can't run mid-construction
	}
}

void UReactiveObjectBase::PostLoad()
{
	Super::PostLoad();
	RefreshSummary(); // Summary isn't saved
	if (ImplementsScriptSummary(GetClass()))
	{
		RefreshSummarySoon(); // Get Summary can't run while loading
	}
}

void UReactiveObjectBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshSummary();
}

void UReactiveObjectBase::PostEditUndo()
{
	Super::PostEditUndo();
	RefreshSummary();
}
#endif

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveAction.h"
#include "ReactiveActorsLog.h"
#include "ReactiveActorsStats.h"
#include "ReactiveActorsSettings.h"
#include "Engine/World.h"
#include "ReactiveRequirement.h"
#include "GameFramework/Actor.h"

const TArray<TObjectPtr<UReactiveRequirement>>& UReactiveAction::GetRequirements() const
{
	// Single source of truth: the Blueprint class's defaults, not this instance's copy
	return GetClass()->GetDefaultObject<UReactiveAction>()->Requirements;
}

bool UReactiveAction::PassesRequirements(const FReactionContext& InContext, FText& OutReason) const
{
	if (!InContext.Target)
	{
		OutReason = INVTEXT("No target actor.");
		return false;
	}
	for (const UReactiveRequirement* Requirement : GetRequirements())   // Class Requirements
	{
		if (Requirement && !Requirement->Evaluate(InContext, OutReason)) return false;
	}
	if (bAllowExtraRequirements)
	{
		for (const UReactiveRequirement* Requirement : ExtraRequirements)   // this copy's own
		{
			if (Requirement && !Requirement->Evaluate(InContext, OutReason)) return false;
		}
	}
	return true;
}

bool UReactiveAction::CanRun() const
{
	FReactionContext Check = RunContext;
	if (!Check.Target)
	{
		Check.Target = GetOwningActor(); // not run yet: check against the owner
	}
	FText Unused;
	return PassesRequirements(Check, Unused);
}

void UReactiveAction::Load()
{
	if (bLoaded || !bEnabled)
	{
		return;
	}

	bLoaded = true;
	OnLoad();

	TArray<UReactiveAction*> Nested;
	GetNestedActions(Nested);
	for (UReactiveAction* Action : Nested)
	{
		if (Action)
		{
			Action->Load();
		}
	}
}

void UReactiveAction::Execute(const FReactionContext& InContext)
{
	// Contract: the same action object never runs twice at once (it holds its run's state).
	// The runners prevent this; reaching here means a C++ caller skipped that check.
	ensureMsgf(!bRunning, TEXT("%s on %s: Execute called while it's still running. Make a new action object, or wait for Finish Action."),
		*GetClass()->GetName(), *GetNameSafe(InContext.Target));

	bFinishedThisRun = false;
	bCancelledThisRun = false;
	bStopSequence = false;
	bLastSucceeded = true; // skipped counts as succeeded: the chain carries on
	bLastExecuteRan = false;

	RunContext = InContext;
	if (!RunContext.Target)
	{
		RunContext.Target = GetOwningActor();
	}

	if (!bEnabled)
	{
		return;
	}

	FText Reason;
	if (!PassesRequirements(RunContext, Reason))
	{
		const bool bStopsReaction = StopsReactionIfNotMet();

		// Recorded for the debug overlay's "skipped" lines
		const UWorld* World = GetWorld();
		LastSkippedTime = World ? World->GetTimeSeconds() : 0.0;
		LastSkippedReason = bStopsReaction
			? FString::Printf(TEXT("stopped the reaction: %s"), *Reason.ToString())
			: Reason.ToString();

		if (bStopsReaction)
		{
			// Stop Reaction If Not Met: fail here, so the runner ends the reaction as Failed (like Finish Action, Success off).
			// Never ran, so not running: nothing to wait for.
			bLastSucceeded = false;
			bStopSequence = true;
			UE_LOG(LogReactiveActors, Log, TEXT("%s on %s stopped its reaction (Stop Reaction If Not Met): %s"),
				*GetClass()->GetName(), *GetNameSafe(RunContext.Target), *Reason.ToString());
		}
		else if (GetDefault<UReactiveActorsSettings>()->bWarnOnSkippedActions)
		{
			UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s skipped: %s"),
				*GetClass()->GetName(), *GetNameSafe(RunContext.Target), *LastSkippedReason);
		}
		else
		{
			UE_LOG(LogReactiveActors, Verbose, TEXT("%s on %s skipped: %s"),
				*GetClass()->GetName(), *GetNameSafe(RunContext.Target), *LastSkippedReason);
		}
		return;
	}

	// Lazily load if nothing has yet
	if (!bLoaded)
	{
		Load();
	}

	bLastExecuteRan = true;
	bRunning = true;

	INC_DWORD_STAT(STAT_ReactionsActionsExecuted);
	SCOPE_CYCLE_COUNTER(STAT_ReactionsExecuteAction);

	// No automatic finish: the action calls Finish Action, here or later
	OnExecuteAction(RunContext); // Target checked valid by the requirements step above
}

void UReactiveAction::FinishAction(bool bSuccess)
{
	FinishActionInternal(bSuccess, /*bStopAfter*/ false);
}

void UReactiveAction::ResetAction()
{
	OnReset();

	// The overlay's "skipped" line too
	LastSkippedTime = -1.0;
	LastSkippedReason.Reset();

	// Actions inside a Branch or Run On Targets
	TArray<UReactiveAction*> Nested;
	GetNestedActions(Nested);
	for (UReactiveAction* Action : Nested)
	{
		if (Action)
		{
			Action->ResetAction();
		}
	}
}

void UReactiveAction::Abort()
{
	if (!bRunning)
	{
		return;
	}

	// Not running from here on, so a Finish Action called from On Cancel (or a timer finishing late) does nothing
	bRunning = false;
	bCancelledThisRun = true;
	bStopSequence = true;
	OnCancel();
}

void UReactiveAction::FinishActionInternal(bool bSuccess, bool bStopAfter)
{
	if (!bRunning)
	{
		// After a cancel, or before any run, a late Finish Action is expected and ignored.
		// A second Finish Action in the same run is a mistake: say so once per class.
		if (bFinishedThisRun && !bCancelledThisRun)
		{
			ReportFinishedTwice();
		}
		return;
	}

	bRunning = false;
	bFinishedThisRun = true;
	bLastSucceeded = bSuccess;
	bStopSequence = bStopAfter || !bSuccess;
	OnFinished.Broadcast(this);
}

void UReactiveAction::ReportFinishedTwice() const
{
#if !UE_BUILD_SHIPPING
	// Once per class per session, so a looping mistake doesn't flood the log
	static TSet<FName> Reported;
	bool bAlreadyReported = false;
	Reported.Add(GetClass()->GetFName(), &bAlreadyReported);
	if (bAlreadyReported)
	{
		return;
	}

	const FString Message = FString::Printf(
		TEXT("%s called Finish Action twice in one run. Only the first one counts: check for two paths both reaching Finish Action."),
		*GetClass()->GetName());

	// C++ actions get a call stack (ensure); Blueprint ones a plain warning, since a C++ stack wouldn't help there
	if (GetClass()->HasAnyClassFlags(CLASS_Native))
	{
		ensureMsgf(false, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s"), *Message);
	}
#endif
}

void UReactiveAction::Cleanup()
{
	// Tearing down mid-run: stop without telling the sequence to continue
	bRunning = false;
	OnFinished.Clear();

	TArray<UReactiveAction*> Nested;
	GetNestedActions(Nested);
	for (UReactiveAction* Action : Nested)
	{
		if (Action)
		{
			Action->Cleanup();
		}
	}

	if (!bLoaded)
	{
		return;
	}

	OnCleanup();
	bLoaded = false;
}


// Get the target actor for this action, or the owning actor if no target is set
AActor* UReactiveAction::GetTarget() const
{
	if (RunContext.Target)
	{
		// Streamed out or destroyed mid-run: return nothing rather than the wrong actor
		return IsValid(RunContext.Target) ? RunContext.Target.Get() : nullptr;
	}
	return GetOwningActor(); // not run yet
}

// Get the instigator actor for this action
AActor* UReactiveAction::GetInstigator() const
{
	if (RunContext.Instigator)
	{
		// Streamed out or destroyed mid-run: return nothing rather than the wrong actor
		return IsValid(RunContext.Instigator) ? RunContext.Instigator.Get() : nullptr;
	}

	return nullptr;
}

// Get the world context for this action
UWorld* UReactiveAction::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return nullptr;
	}

	if (UWorld* World = Super::GetWorld())
	{
		return World;
	}
	return RunContext.Target ? RunContext.Target->GetWorld() : nullptr;
}

void UReactiveAction::PostInitProperties()
{
	Super::PostInitProperties();
	bIsClassDefaults = HasAnyFlags(RF_ClassDefaultObject);
}

FString UReactiveAction::DescribeAction() const
{
	return GetClass()->GetDisplayNameText().ToString();
}

FString UReactiveAction::BuildSummary() const
{
	FString Out = ScriptSummaryOr(DescribeAction());
	if (bAllowExtraRequirements && !ExtraRequirements.IsEmpty())
	{
		const FString Checks = ReactiveSummary::Names(ExtraRequirements, TEXT(" and "));
		if (!Checks.IsEmpty())
		{
			Out += FString::Printf(TEXT("  [if %s]"), *Checks);
		}
	}
	if (StopsReactionIfNotMet() && (!GetRequirements().IsEmpty() || !ExtraRequirements.IsEmpty()))
	{
		Out += TEXT("  (stops if not met)");
	}
	if (!bEnabled)
	{
		Out += TEXT("  (off)");
	}
	return Out;
}

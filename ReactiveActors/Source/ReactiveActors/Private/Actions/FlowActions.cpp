// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions: flow

#include "Actions/FlowActions.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ReactionComponent.h"
#include "ReactionSubsystem.h"
#include "ReactiveActorsLog.h"
#include "ReactiveRequirement.h"
#include "TimerManager.h"

// ---- Branch -------------------------------------------------------------------------------------

UACT_Branch::UACT_Branch()
{
	// Its checks are its Conditions: a failed Extra Requirement would skip both sides, which is easy
	// to mix up with a failed condition (False actions run). So Extra Requirements aren't offered here.
	bAllowExtraRequirements = false;

	// Its True and False lists can't be filled in from the Run Action node's pins
	bAvailableInBlueprintRunAction = false;
}

void UACT_Branch::Execute(const FReactionContext& InContext)
{
	Super::Execute(InContext);
	if (!IsRunning())
	{
		return; // disabled
	}

	// Evaluated once per run, so a condition changing mid-branch doesn't switch lists
	bLastResult = EvaluateConditions(GetContext());

	InnerSequence.OnComplete = [this](EReactionResult Result, bool bStoppedEarly)
		{
			// A failure inside fails the branch (and the chain it's in). A Stop Reaction inside ends that chain too.
			FinishActionInternal(Result == EReactionResult::Succeeded, bStoppedEarly);
		};
	InnerSequence.Start(this, bLastResult ? TrueActions : FalseActions, GetContext());
}

void UACT_Branch::Abort()
{
	InnerSequence.Abort(); // aborts the True or False action it's waiting on
	Super::Abort();
}

void UACT_Branch::Cleanup()
{
	InnerSequence.Stop();
	Super::Cleanup();
}

bool UACT_Branch::EvaluateConditions(const FReactionContext& InContext) const
{
	const bool bRequireAll = (EvaluateMode == EReactiveBranchMode::AllConditions);
	bool bAnyConfigured = false;

	for (const UReactiveRequirement* Condition : Conditions)
	{
		if (!Condition)
		{
			continue;
		}
		bAnyConfigured = true;

		FText Unused;
		const bool bPassed = Condition->Evaluate(InContext, Unused);

		if (bRequireAll && !bPassed)
		{
			return false;
		}
		if (!bRequireAll && bPassed)
		{
			return true;
		}
	}

	// All passed (All Conditions) / none passed (Any Condition) / nothing configured (true)
	return bRequireAll || !bAnyConfigured;
}

void UACT_Branch::GetNestedActions(TArray<UReactiveAction*>& OutActions) const
{
	OutActions.Append(TrueActions);
	OutActions.Append(FalseActions);
}

// ---- Delay --------------------------------------------------------------------------------------

void UACT_Delay::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	UWorld* World = GetWorld();
	if (!World || Duration <= 0.f)
	{
		FinishAction();
		return;
	}

	World->GetTimerManager().SetTimer(TimerHandle, this, &UACT_Delay::HandleDelayFinished, Duration, false);
}

void UACT_Delay::HandleDelayFinished()
{
	FinishAction();
}

void UACT_Delay::OnCleanup_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
}

void UACT_Delay::OnCancel_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
}

// ---- Wait For Event -----------------------------------------------------------------------------

void UACT_WaitForEvent::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	UWorld* World = GetWorld();
	UReactionSubsystem* Subsystem = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;

	const TCHAR* Problem = nullptr;
	if (!Event.IsValid())
	{
		Problem = TEXT("no Event set");
	}
	else if (ListenScope == EReactiveScopePolicy::Identity && !ListenIdentity.IsValid())
	{
		Problem = TEXT("Identity scope but no Listen Identity set");
	}
	else if (!Subsystem)
	{
		Problem = TEXT("no Reaction Subsystem");
	}

	if (Problem)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: %s, not waiting."), *GetClass()->GetName(), *GetNameSafe(Context.Target), Problem);
		FinishAction();
		return;
	}

	// Waits until the event comes. Cancel and Cleanup (the reaction cancelled, the actor ending play) stop it.
	ListeningTo = Subsystem;
	EventHandle = Subsystem->OnEventSent.AddUObject(this, &UACT_WaitForEvent::HandleEvent);

	if (Timeout > 0.f)
	{
		World->GetTimerManager().SetTimer(TimeoutHandle, FTimerDelegate::CreateUObject(this, &UACT_WaitForEvent::HandleTimeout), Timeout, false);
	}
}

void UACT_WaitForEvent::HandleTimeout()
{
	if (!IsRunning())
	{
		return;
	}
	UE_LOG(LogReactiveActors, Log, TEXT("%s on %s: %s didn't come within %.1fs%s."), *GetClass()->GetName(),
		*GetNameSafe(GetTarget()), *Event.ToString(), Timeout, bFailOnTimeout ? TEXT(", reaction failed") : TEXT(", carrying on"));
	StopWaiting();
	FinishAction(/*bSuccess*/ !bFailOnTimeout);
}

void UACT_WaitForEvent::HandleEvent(const FReactiveEventInfo& Info)
{
	if (!ListeningTo.IsValid() || !IsRunning())
	{
		return;
	}

	if (!UReactionSubsystem::EventMatches(Info.Event, Event, bExactMatch))
	{
		return;
	}
	if (ListenScope == EReactiveScopePolicy::Identity && !UReactionSubsystem::EventSentToIdentity(Info, ListenIdentity))
	{
		return;
	}

	StopWaiting();
	FinishAction();
}

void UACT_WaitForEvent::StopWaiting()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutHandle);
	}
	if (UReactionSubsystem* Subsystem = ListeningTo.Get())
	{
		Subsystem->OnEventSent.Remove(EventHandle);
	}
	EventHandle.Reset();
	ListeningTo.Reset();
}

void UACT_WaitForEvent::OnCancel_Implementation()
{
	StopWaiting();
}

void UACT_WaitForEvent::OnCleanup_Implementation()
{
	StopWaiting();
}

FString UACT_WaitForEvent::DescribeAction() const
{
	if (ListenScope == EReactiveScopePolicy::Identity)
	{
		return FString::Printf(TEXT("Wait For %s sent to %s%s"), *ReactiveSummary::Tag(Event), *ReactiveSummary::Tag(ListenIdentity),
			Timeout > 0.f ? *FString::Printf(TEXT(" (up to %.1fs)"), Timeout) : TEXT(""));
	}
	const FString Limit = Timeout > 0.f ? FString::Printf(TEXT(" (up to %.1fs)"), Timeout) : FString();
	return FString::Printf(TEXT("Wait For %s%s"), *ReactiveSummary::Tag(Event), *Limit);
}

// ---- Stop Reaction ------------------------------------------------------------------------------

void UACT_StopReaction::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	// Ends the chain this action is in. A Branch around it passes that on, so the whole reaction ends
	// (with this Result); inside Run On Targets only that target's actions end.
	FinishActionInternal(Result != EReactionResult::Failed, /*bStopAfter*/ true);
}

// ---- Random Action ------------------------------------------------------------------------------

UACT_RandomAction::UACT_RandomAction()
{
	bAvailableInBlueprintRunAction = false; // its options hold lists of actions: only for reactions
}

void UACT_RandomAction::Execute(const FReactionContext& InContext)
{
	Super::Execute(InContext);
	if (!IsRunning())
	{
		return; // disabled, or its requirements failed
	}

	const int32 Picked = PickOption();
	if (Picked == INDEX_NONE)
	{
		FinishAction(); // no options, or every weight is 0: nothing to run
		return;
	}
	LastPicked = Picked;

	InnerSequence.OnComplete = [this](EReactionResult Result, bool bStoppedEarly)
		{
			// Like Branch: a failure inside fails this action, a Stop Reaction inside ends the chain
			FinishActionInternal(Result == EReactionResult::Succeeded, bStoppedEarly);
		};
	InnerSequence.Start(this, Options[Picked].Actions, GetContext());
}

int32 UACT_RandomAction::PickOption() const
{
	// Weighted pick over the options that can be picked
	TArray<int32, TInlineAllocator<8>> Candidates;
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		if (Options[Index].Weight > 0.f)
		{
			Candidates.Add(Index);
		}
	}
	if (bAvoidRepeat && Candidates.Num() > 1)
	{
		Candidates.Remove(LastPicked);
	}

	float Total = 0.f;
	for (const int32 Index : Candidates)
	{
		Total += Options[Index].Weight;
	}
	if (Candidates.IsEmpty() || Total <= 0.f)
	{
		return INDEX_NONE;
	}

	float Roll = FMath::FRand() * Total;
	for (const int32 Index : Candidates)
	{
		Roll -= Options[Index].Weight;
		if (Roll < 0.f)
		{
			return Index;
		}
	}
	return Candidates.Last(); // rounding
}

#if WITH_EDITOR
void UACT_RandomAction::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	const bool bOptionAdded = PropertyChangedEvent.Property
		&& PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UACT_RandomAction, Options)
		&& PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd; // Insert is reported as ArrayAdd too
	if (bOptionAdded)
	{
		const int32 Index = PropertyChangedEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(UACT_RandomAction, Options));
		if (Options.IsValidIndex(Index))
		{
			Options[Index].Weight = 1.f;
		}
	}
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif

void UACT_RandomAction::Abort()
{
	InnerSequence.Abort(); // aborts the picked option's action it's waiting on
	Super::Abort();
}

void UACT_RandomAction::Cleanup()
{
	InnerSequence.Stop();
	Super::Cleanup();
}

void UACT_RandomAction::ResetAction()
{
	LastPicked = INDEX_NONE;
	Super::ResetAction();
}

void UACT_RandomAction::GetNestedActions(TArray<UReactiveAction*>& OutActions) const
{
	for (const FReactiveRandomOption& Option : Options)
	{
		OutActions.Append(Option.Actions);
	}
}

FString UACT_RandomAction::DescribeAction() const
{
	return FString::Printf(TEXT("Random Action: %d option%s%s"), Options.Num(), Options.Num() == 1 ? TEXT("") : TEXT("s"),
		bAvoidRepeat ? TEXT(", no repeats") : TEXT(""));
}

// ---- Run On Targets -----------------------------------------------------------------------------

UACT_RunOnTargets::UACT_RunOnTargets()
{
	bAvailableInBlueprintRunAction = false; // its Actions list can't be filled in from the Run Action node
}

void UACT_RunOnTargets::Execute(const FReactionContext& InContext)
{
	Super::Execute(InContext);
	if (!IsRunning())
	{
		return; // skipped
	}

	TArray<AActor*> Targets;
	if (!TargetIdentity.IsValid())
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: no Target Identity set."), *GetClass()->GetName(), *GetNameSafe(GetTarget()));
	}
	else if (UWorld* World = GetWorld())
	{
		if (const UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			for (AActor* Found : Subsystem->GetActorsByIdentity(TargetIdentity))
			{
				if (IsValid(Found) && !(bExcludeSelf && Found == GetTarget()))
				{
					Targets.Add(Found);
				}
			}
		}
	}

	// Runs for targets that have since been destroyed or streamed out: let them go
	for (int32 Index = TargetRuns.Num() - 1; Index >= 0; --Index)
	{
		UReactiveTargetRun* Run = TargetRuns[Index];
		if (!Run || !Run->Target.IsValid())
		{
			if (Run)
			{
				Run->Runner.Stop();
				for (UReactiveAction* Copy : Run->Actions)
				{
					if (Copy)
					{
						Copy->Cleanup();
					}
				}
			}
			TargetRuns.RemoveAt(Index);
		}
	}

	// Make (or reuse) every target's run first, then start them all
	TArray<UReactiveTargetRun*> ToStart;
	for (AActor* Target : Targets)
	{
		if (UReactiveTargetRun* Run = GetOrMakeRun(Target))
		{
			ToStart.Add(Run);
		}
	}

	PendingRuns = ToStart.Num();
	bStartingRuns = true;
	for (UReactiveTargetRun* Run : ToStart)
	{
		AActor* RunTarget = Run->Target.Get();

		// Debug wiring line, drawn the same as a broadcast reaching that actor
		if (const UWorld* World = GetWorld())
		{
			if (const UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
			{
				Subsystem->DrawDebugWire(GetTarget(), RunTarget, GetContext().Event, /*bFired*/ true);
			}
		}

		// A failure or Stop Reaction only ends this target's actions: just count it as done
		Run->Runner.OnComplete = [this](EReactionResult /*Result*/, bool /*bStoppedEarly*/) { HandleTargetComplete(); };

		FReactionContext TargetContext = GetContext();
		TargetContext.Target = RunTarget;
		Run->Runner.Start(Run, Run->Actions, TargetContext); // instant runs complete right here

		if (!IsRunning())
		{
			bStartingRuns = false;
			return; // something a target's actions did cancelled this action
		}
	}
	bStartingRuns = false;

	FinishIfDone(); // no targets, or every one finished straight away
}

UReactiveTargetRun* UACT_RunOnTargets::GetOrMakeRun(AActor* Target)
{
	for (UReactiveTargetRun* Run : TargetRuns)
	{
		if (Run && Run->Target.Get() == Target)
		{
			return Run;
		}
	}

	// This target's own copies, owned by this action (so Get Owner and Get Owning Actor still resolve)
	UReactiveTargetRun* Run = NewObject<UReactiveTargetRun>(this);
	Run->Target = Target;
	for (const UReactiveAction* Template : Actions)
	{
		Run->Actions.Add(Template ? DuplicateObject<UReactiveAction>(Template, this) : nullptr);
	}
	TargetRuns.Add(Run);
	return Run;
}

void UACT_RunOnTargets::HandleTargetComplete()
{
	--PendingRuns;
	FinishIfDone();
}

void UACT_RunOnTargets::FinishIfDone()
{
	if (!bStartingRuns && PendingRuns <= 0 && IsRunning())
	{
		PendingRuns = 0;
		FinishAction(); // every target done: Run On Targets succeeds, whatever each target's result
	}
}

const FActionRunner* UACT_RunOnTargets::GetActiveInnerSequence() const
{
	// The overlay follows one runner: the first target still going
	for (const UReactiveTargetRun* Run : TargetRuns)
	{
		if (Run && Run->Runner.IsRunning())
		{
			return &Run->Runner;
		}
	}
	return nullptr;
}

void UACT_RunOnTargets::Abort()
{
	// Every target still going: abort the action each one is waiting on
	for (UReactiveTargetRun* Run : TargetRuns)
	{
		if (Run)
		{
			Run->Runner.Abort();
		}
	}
	PendingRuns = 0;
	Super::Abort();
}

void UACT_RunOnTargets::ResetAction()
{
	Super::ResetAction(); // its own On Reset, and the template actions

	// Every target's copies too, so their state goes back to the start
	for (UReactiveTargetRun* Run : TargetRuns)
	{
		if (Run)
		{
			for (UReactiveAction* Copy : Run->Actions)
			{
				if (Copy)
				{
					Copy->ResetAction();
				}
			}
		}
	}
}

void UACT_RunOnTargets::Cleanup()
{
	for (UReactiveTargetRun* Run : TargetRuns)
	{
		if (Run)
		{
			Run->Runner.Stop();
			for (UReactiveAction* Copy : Run->Actions)
			{
				if (Copy)
				{
					Copy->Cleanup();
				}
			}
		}
	}
	TargetRuns.Reset();
	PendingRuns = 0;
	Super::Cleanup();
}

void UACT_RunOnTargets::GetNestedActions(TArray<UReactiveAction*>& OutActions) const
{
	// The templates (for loading, cleanup and Map Check); each target's copies are handled above
	for (UReactiveAction* Action : Actions)
	{
		OutActions.Add(Action);
	}
}

// ---- Summaries (the row titles in the editor) ---------------------------------------------------

FString UACT_Branch::DescribeAction() const
{
	const FString Checks = ReactiveSummary::Names(Conditions,
		EvaluateMode == EReactiveBranchMode::AllConditions ? TEXT(" and ") : TEXT(" or "));
	return FString::Printf(TEXT("Branch: %s  (True %d / False %d)"),
		Checks.IsEmpty() ? TEXT("always true") : *Checks, TrueActions.Num(), FalseActions.Num());
}

FString UACT_Delay::DescribeAction() const
{
	return FString::Printf(TEXT("Delay %.1fs"), Duration);
}

FString UACT_StopReaction::DescribeAction() const
{
	return Result == EReactionResult::Failed ? TEXT("Stop Reaction (Failed)") : TEXT("Stop Reaction (Succeeded)");
}

FString UACT_RunOnTargets::DescribeAction() const
{
	return FString::Printf(TEXT("Run On Targets %s (%d action%s)"), *ReactiveSummary::Tag(TargetIdentity, TEXT("<no identity>")),
		Actions.Num(), Actions.Num() == 1 ? TEXT("") : TEXT("s"));
}

// ---- Validate Settings (Data Validation and Map Check) ----------------------------------------

UACT_WaitForEvent::UACT_WaitForEvent()
{
	// Waits inside a reaction's chain; on its own (Run Action) there's nothing after it to wait for
	bAvailableInBlueprintRunAction = false;
}

void UACT_WaitForEvent::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Event.IsValid())
	{
		Errors.Add(INVTEXT("Event isn't set, so it waits forever."));
	}
	if (ListenScope == EReactiveScopePolicy::Identity && !ListenIdentity.IsValid())
	{
		Errors.Add(INVTEXT("Identity scope but no Listen Identity, so it waits forever."));
	}
}

void UACT_RunOnTargets::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!TargetIdentity.IsValid())
	{
		Errors.Add(INVTEXT("Target Identity isn't set, so it finds no targets."));
	}
	if (Actions.IsEmpty())
	{
		Warnings.Add(INVTEXT("has no actions, so it does nothing."));
	}
}

void UACT_Branch::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (Conditions.IsEmpty())
	{
		Warnings.Add(INVTEXT("has no conditions, so it always takes the True actions."));
	}
	if (TrueActions.IsEmpty() && FalseActions.IsEmpty())
	{
		Warnings.Add(INVTEXT("has no True or False actions, so it does nothing."));
	}

	// Its conditions live inside it, so it reports them (its actions are checked as nested actions)
	for (int32 Index = 0; Index < Conditions.Num(); ++Index)
	{
		const UReactiveRequirement* Condition = Conditions[Index];
		if (!Condition)
		{
			Warnings.Add(FText::Format(INVTEXT("condition slot {0} is empty."), Index));
			continue;
		}
		TArray<FText> ConditionErrors;
		TArray<FText> ConditionWarnings;
		Condition->CollectSettingsProblems(ConditionErrors, ConditionWarnings);
		const FText Name = Condition->GetClass()->GetDisplayNameText();
		for (const FText& Error : ConditionErrors)
		{
			Errors.Add(FText::Format(INVTEXT("condition {0}: {1}"), Name, Error));
		}
		for (const FText& Warning : ConditionWarnings)
		{
			Warnings.Add(FText::Format(INVTEXT("condition {0}: {1}"), Name, Warning));
		}
	}
}

void UACT_RandomAction::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (Options.IsEmpty())
	{
		Warnings.Add(INVTEXT("has no options, so it does nothing."));
		return;
	}

	float TotalWeight = 0.f;
	for (const FReactiveRandomOption& Option : Options)
	{
		TotalWeight += FMath::Max(Option.Weight, 0.f);
	}
	if (TotalWeight <= 0.f)
	{
		Errors.Add(INVTEXT("every option has Weight 0, so none can be picked."));
	}
}

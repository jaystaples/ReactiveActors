// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveTrigger.h"
#include "ReactiveActorsLog.h"
#include "ReactiveActorsStats.h"

#include "Engine/World.h"
#include "ReactiveRule.h"
#include "ReactionComponent.h"
#include "ReactiveRequirement.h"
#include "ReactionSubsystem.h"
#include "GameFramework/Actor.h"

void UReactiveTrigger::Activate(UReactionComponent* InComponent)
{
	if (bActive || !bEnabled)
	{
		return;
	}

	Component = InComponent;
	bActive = true;

	// Blueprint triggers tick only if they implement On Tick, so there's nothing to switch on
	static const FName OnTickName = GET_FUNCTION_NAME_CHECKED(UReactiveTrigger, OnTick);
	bWantsTick = bTickInCpp || GetClass()->IsFunctionImplementedInScript(OnTickName);

	OnActivate();
}

void UReactiveTrigger::Deactivate(EEndPlayReason::Type Reason)
{
	if (!bActive)
	{
		return;
	}

	OnDeactivate(Reason); // still active in here, so End Play style triggers can fire
	bActive = false;
}

void UReactiveTrigger::TickTrigger(float DeltaTime)
{
	if (bActive && bWantsTick)
	{
		OnTick(DeltaTime);
	}
}

void UReactiveTrigger::ResetTrigger()
{
	OnReset();

	for (UReactiveRule* Rule : Rules)
	{
		if (Rule)
		{
			Rule->OnReset();
		}
	}

	LastFiredTime = -1.0;
	LastBlockedTime = -1.0;
}

UReactiveTrigger* UReactiveTrigger::DuplicateFor(UObject* NewOuter) const
{
	UReactiveTrigger* Copy = DuplicateObject<UReactiveTrigger>(this, NewOuter);
	if (!Copy)
	{
		return nullptr;
	}

	// Rules keep state: make sure the copy has its own, not the preset's
	// (inline objects are normally owned by the trigger and duplicated with it, this covers the rest)
	for (TObjectPtr<UReactiveRule>& Rule : Copy->Rules)
	{
		if (Rule && Rule->GetOuter() != Copy)
		{
			Rule = DuplicateObject<UReactiveRule>(Rule, Copy);
		}
	}
	return Copy;
}

AActor* UReactiveTrigger::GetTriggerActor() const
{
	if (const UReactionComponent* Owner = Component.Get())
	{
		return Owner->GetOwner();
	}
	return GetOwningActor(); // before activation: through the outer chain
}

bool UReactiveTrigger::FireTrigger(AActor* Instigator)
{
	SCOPE_CYCLE_COUNTER(STAT_ReactionsFireTrigger);

	if (!bActive || !bEnabled)
	{
		return false;
	}

	// Fired again from inside its own send (its event led straight back to it): refuse, or it never ends
	if (bFiring)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: fired again while sending %s (a loop), ignored."),
			*GetClass()->GetName(), *GetNameSafe(GetTriggerActor()), *Event.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	UReactionSubsystem* Subsystem = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return false;
	}

	// Hold Reactions Until Restored: fire once the restore is in, so rules check the saved state
	{
		TWeakObjectPtr<UReactiveTrigger> WeakThis = this;
		TWeakObjectPtr<AActor> WeakInstigator = Instigator;
		if (Subsystem->DeferIfHolding([WeakThis, WeakInstigator]()
			{
				if (UReactiveTrigger* Self = WeakThis.Get())
				{
					Self->FireTrigger(WeakInstigator.Get());
				}
			}))
		{
			return false;
		}
	}

	// Suspended: don't fire at all, so rules don't count a fire that sent nothing
	if (Subsystem->AreEventsSuspended())
	{
		return false;
	}

	FReactionContext Context;
	Context.Target = GetTriggerActor();
	// Nobody else caused it (Begin Play, Timer, End Play...): this actor did, so receivers know where it came from
	Context.Instigator = Instigator ? Instigator : Context.Target.Get();
	Context.Event = Event;

	const double Now = World->GetTimeSeconds();

	// 1. Rules: memory (Max Activations, Cooldown)
	for (const UReactiveRule* Rule : Rules)
	{
		if (Rule && Rule->ShouldBlock(Context))
		{
			LastBlockedTime = Now;
			LastBlockedBy = Rule->GetClass()->GetDisplayNameText().ToString();
			return false;
		}
	}

	// 2. Conditions: the situation right now
	for (const UReactiveRequirement* Condition : Conditions)
	{
		FText Reason;
		if (Condition && !Condition->Evaluate(Context, Reason))
		{
			LastBlockedTime = Now;
			LastBlockedBy = Reason.ToString();
			return false;
		}
	}

	// 3. Fire. For rules, a fire is a run that takes no time: start and finish back to back.
	for (UReactiveRule* Rule : Rules)
	{
		if (Rule)
		{
			Rule->OnStart(Context);
		}
	}

	// Something happened on this actor: it now has state worth keeping (saves, streaming out)
	if (UReactionComponent* Owner = Component.Get())
	{
		Owner->MarkStateChanged();
	}

	LastFiredTime = Now;
	{
		TGuardValue<bool> FiringGuard(bFiring, true);
		if (Event.IsValid())
		{
			SendEvent(Context); // no event: still a fire (rules count it, On Fired runs), nothing sent
		}
		OnFired(Context);
	}

	for (UReactiveRule* Rule : Rules)
	{
		if (Rule)
		{
			Rule->OnFinish(Context, EReactionResult::Succeeded);
		}
	}

	return true;
}

void UReactiveTrigger::SendEvent(const FReactionContext& Context)
{
	UWorld* World = GetWorld();
	UReactionSubsystem* Subsystem = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
	AActor* Instigator = Context.Instigator.Get();

	switch (Scope)
	{
	case EReactiveScopePolicy::Self:
		if (Subsystem)
		{
			// Through the subsystem (not the component directly) so World listeners hear it too
			Subsystem->BroadcastEventToActor(Context.Target.Get(), Event, Instigator, Context.Target.Get());
		}
		break;

	case EReactiveScopePolicy::World:
		if (Subsystem)
		{
			// This actor is the source, so the debug wiring lines start from it
			Subsystem->BroadcastEventFrom(Event, Instigator, Context.Target.Get());
		}
		break;

	case EReactiveScopePolicy::Identity:
		if (Subsystem)
		{
			if (TargetIdentity.IsValid())
			{
				Subsystem->BroadcastEventToIdentity(TargetIdentity, Event, Instigator, Context.Target.Get());
			}
			else
			{
				UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: Identity scope but no Target Identity set."),
					*GetClass()->GetName(), *GetNameSafe(Context.Target));
			}
		}
		break;
	}
}

#if WITH_EDITOR
void UReactiveTrigger::CheckForErrors(TFunctionRef<void(const FText&)> Warn) const
{
	const FText Name = GetClass()->GetDisplayNameText();

	// No event is fine if something else reacts to the fire (a rule, or On Fired in Blueprint)
	static const FName OnFiredName = GET_FUNCTION_NAME_CHECKED(UReactiveTrigger, OnFired);
	if (!Event.IsValid() && Rules.IsEmpty() && !GetClass()->IsFunctionImplementedInScript(OnFiredName))
	{
		Warn(FText::Format(INVTEXT("trigger {0} has no Event, no rules and no On Fired: firing it does nothing."), Name));
	}
	if (Scope == EReactiveScopePolicy::Identity && !TargetIdentity.IsValid())
	{
		Warn(FText::Format(INVTEXT("trigger {0} uses Identity scope but has no Target Identity."), Name));
	}
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		if (!Rules[Index])
		{
			Warn(FText::Format(INVTEXT("trigger {0}: rule slot {1} is empty."), Name, Index));
		}
	}
	for (int32 Index = 0; Index < Conditions.Num(); ++Index)
	{
		if (!Conditions[Index])
		{
			Warn(FText::Format(INVTEXT("trigger {0}: condition slot {1} is empty."), Name, Index));
		}
	}
}
#endif

FString UReactiveTrigger::DescribeTrigger() const
{
	return GetClass()->GetDisplayNameText().ToString();
}

FString UReactiveTrigger::BuildSummary() const
{
	FString Out = ScriptSummaryOr(DescribeTrigger());
	if (Event.IsValid())
	{
		Out += FString::Printf(TEXT(" -> %s (%s)"), *Event.ToString(), *ReactiveSummary::Scope(Scope, TargetIdentity));
	}
	if (!Rules.IsEmpty())
	{
		Out += FString::Printf(TEXT("  [%s]"), *ReactiveSummary::Names(Rules));
	}
	if (!bEnabled)
	{
		Out += TEXT("  (off)");
	}
	return Out;
}

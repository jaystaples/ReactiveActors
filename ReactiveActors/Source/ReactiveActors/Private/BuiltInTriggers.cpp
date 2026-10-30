// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in triggers

#include "BuiltInTriggers.h"

#include "Engine/World.h"
#include "ReactionSubsystem.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

// ---- Begin Play ---------------------------------------------------------------------------------

void UTRG_BeginPlay::OnActivate_Implementation()
{
	// Next tick, every scope: other actors beginning play at the same time are listening by then, and a save
	// restored while the level loads has already put its rule state back (so nothing fires twice after a load)
	if (UWorld* World = GetWorld())
	{
		NextFrameHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]() { FireTrigger(nullptr); }));
	}
}

void UTRG_BeginPlay::OnDeactivate_Implementation(EEndPlayReason::Type /*Reason*/)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextFrameHandle);
	}
}

// ---- End Play -----------------------------------------------------------------------------------

void UTRG_EndPlay::OnDeactivate_Implementation(EEndPlayReason::Type Reason)
{
	// Never while the game or level is shutting down (PIE ending, quitting, travelling):
	// the other actors are going too, and there's nobody left to tell.
	const bool bDestroyed = Reason == EEndPlayReason::Destroyed;
	const bool bUnloaded = Reason == EEndPlayReason::RemovedFromWorld;

	if (bDestroyed || (bUnloaded && bAlsoWhenUnloaded))
	{
		FireTrigger(nullptr);
	}
}

// ---- Timer --------------------------------------------------------------------------------------

void UTRG_Timer::OnActivate_Implementation()
{
	Schedule(FirstDelay);
}

void UTRG_Timer::OnDeactivate_Implementation(EEndPlayReason::Type /*Reason*/)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
}

void UTRG_Timer::Schedule(float BaseDelay)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Delay = FMath::Max(BaseDelay + FMath::FRandRange(-RandomDeviation, RandomDeviation), 0.01f);
	World->GetTimerManager().SetTimer(TimerHandle, this, &UTRG_Timer::HandleTimer, Delay, /*bLoop*/ false);
}

void UTRG_Timer::HandleTimer()
{
	FireTrigger(nullptr);

	if (bRepeat && IsActive())
	{
		Schedule(Interval);
	}
}

// ---- Event Received -----------------------------------------------------------------------------

void UTRG_EventReceived::GetHeardEvents(FGameplayTagContainer& OutEvents) const
{
	// Self: the component registers for the tag, so broadcasts of it reach this actor
	if (IsEnabled() && ListenScope == EReactiveScopePolicy::Self && ListenFor.IsValid())
	{
		OutEvents.AddTag(ListenFor);
	}
}

bool UTRG_EventReceived::HandleEventToActor(const FGameplayTag& InEvent, AActor* Instigator)
{
	if (ListenScope != EReactiveScopePolicy::Self || !ListenFor.IsValid() || !InEvent.MatchesTag(ListenFor))
	{
		return false;
	}
	return FireTrigger(Instigator);
}

void UTRG_EventReceived::OnActivate_Implementation()
{
	// Self is handled by the component (HandleEventToActor). Identity isn't offered for listening;
	// if it's set anyway (from C++), it listens like World.
	if (ListenScope == EReactiveScopePolicy::Self || !ListenFor.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			WorldHandle = Subsystem->OnEventSent.AddUObject(this, &UTRG_EventReceived::HandleWorldEvent);
		}
	}
}

void UTRG_EventReceived::OnDeactivate_Implementation(EEndPlayReason::Type /*Reason*/)
{
	if (WorldHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
			{
				Subsystem->OnEventSent.Remove(WorldHandle);
			}
		}
		WorldHandle.Reset();
	}
}

void UTRG_EventReceived::HandleWorldEvent(const FReactiveEventInfo& Info)
{
	if (UReactionSubsystem::EventMatches(Info.Event, ListenFor, /*bExactMatch*/ false))
	{
		FireTrigger(Info.Instigator);
	}
}

#if WITH_EDITOR
void UTRG_EventReceived::CheckForErrors(TFunctionRef<void(const FText&)> Warn) const
{
	Super::CheckForErrors(Warn);

	if (!ListenFor.IsValid())
	{
		Warn(INVTEXT("trigger Event Received has nothing in Listen For, it will never fire."));
	}
	else if (GetEvent().IsValid() && GetEvent().MatchesTag(ListenFor))
	{
		Warn(FText::Format(INVTEXT("trigger Event Received sends {0}, which it also listens for: it would hear itself (blocked as a loop)."),
			FText::FromString(GetEvent().ToString())));
	}
}
#endif

// ---- Summaries (the row titles in the editor) ---------------------------------------------------

FString UTRG_EndPlay::DescribeTrigger() const
{
	return bAlsoWhenUnloaded ? TEXT("End Play (or unloaded)") : TEXT("End Play");
}

FString UTRG_Timer::DescribeTrigger() const
{
	FString Out = FString::Printf(TEXT("Timer %.1fs"), FirstDelay);
	if (bRepeat)
	{
		Out += FString::Printf(TEXT(", then every %.1fs"), Interval);
	}
	if (RandomDeviation > 0.f)
	{
		Out += FString::Printf(TEXT(" (+/- %.1fs)"), RandomDeviation);
	}
	return Out;
}

FString UTRG_EventReceived::DescribeTrigger() const
{
	return FString::Printf(TEXT("When %s%s"), *ReactiveSummary::Tag(ListenFor),
		ListenScope == EReactiveScopePolicy::Self ? TEXT(" reaches this actor") : TEXT(""));
}

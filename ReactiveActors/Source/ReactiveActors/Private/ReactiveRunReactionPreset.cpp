// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveRunReactionPreset.h"
#include "ReactiveActorsLog.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ReactionPreset.h"
#include "ReactionSubsystem.h"
#include "TimerManager.h"

UReactiveRunReactionPreset* UReactiveRunReactionPreset::RunReactionPreset(UObject* WorldContextObject, UReactionPreset* Preset,
	const FReactionContext& Context)
{
	UReactiveRunReactionPreset* Action = NewObject<UReactiveRunReactionPreset>();
	Action->Owner = WorldContextObject;
	Action->Preset = Preset;
	Action->Target = Context.Target.Get();
	Action->RunInstigator = Context.Instigator.Get();
	Action->Event = Context.Event;
	Action->RegisterWithGameInstance(WorldContextObject); // kept alive until it ends
	return Action;
}

void UReactiveRunReactionPreset::Activate()
{
	Super::Activate();
	TGuardValue<bool> ActivatingGuard(bActivating, true);

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Owner.Get(), EGetWorldErrorMode::ReturnNull) : nullptr;
	UReactionSubsystem* Subsystem = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("Run Reaction Preset: no Reaction Subsystem (only in play, not the editor world)."));
		End(/*bBlocked*/ true);
		return;
	}

	TWeakObjectPtr<UReactiveRunReactionPreset> WeakThis = this;
	TArray<UReactionOneShot*> Started;

	bStarting = true;
	const int32 StartedCount = Subsystem->RunReactionPreset(Preset, Event, Target.Get(), RunInstigator.Get(),
		[WeakThis](EReactionResult Result)
		{
			if (UReactiveRunReactionPreset* Self = WeakThis.Get())
			{
				Self->HandleRunFinished(Result);
			}
		},
		&Started);
	bStarting = false;

	for (UReactionOneShot* Run : Started)
	{
		Runs.Add(Run);
	}

	// Instant runs have already been counted down below zero; this brings it back to the ones still going
	PendingRuns += StartedCount;

	if (StartedCount == 0)
	{
		End(/*bBlocked*/ true);
	}
	else if (PendingRuns <= 0)
	{
		End(/*bBlocked*/ false);
	}
}

void UReactiveRunReactionPreset::HandleRunFinished(EReactionResult Result)
{
	if (bEnded)
	{
		return;
	}

	// Failed beats Cancelled beats Succeeded
	if (Result == EReactionResult::Failed
		|| (Result == EReactionResult::Cancelled && CombinedResult == EReactionResult::Succeeded))
	{
		CombinedResult = Result;
	}

	--PendingRuns;
	if (!bStarting && PendingRuns <= 0)
	{
		End(/*bBlocked*/ false);
	}
}

void UReactiveRunReactionPreset::End(bool bBlocked)
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;

	auto Broadcast = [this, bBlocked]()
		{
			if (ShouldBroadcastDelegates())
			{
				if (bBlocked)
				{
					OnBlocked.Broadcast(EReactionResult::Failed);
				}
				else
				{
					OnFinished.Broadcast(CombinedResult);
				}
			}
			SetReadyToDestroy();
		};

	// Still inside Activate (nothing started, or every run was instant): wait a frame so Then fires first
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Owner.Get(), EGetWorldErrorMode::ReturnNull) : nullptr;
	if (bActivating)
	{
		if (World)
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, Broadcast));
			return;
		}
	}
	Broadcast();
}

void UReactiveRunReactionPreset::Cancel()
{
	// No pins after a Cancel: mark it ended first, so the runs' Cancelled results are ignored
	bEnded = true;

	const TArray<TWeakObjectPtr<UReactionOneShot>> ToCancel = MoveTemp(Runs);
	for (const TWeakObjectPtr<UReactionOneShot>& Weak : ToCancel)
	{
		if (UReactionOneShot* Run = Weak.Get(); Run && Run->Sequence.IsRunning())
		{
			Run->Sequence.Cancel();
		}
	}

	Super::Cancel();
}

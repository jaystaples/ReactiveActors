// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveWaitForEvent.h"
#include "ReactiveActorsLog.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ReactionSubsystem.h"

UReactiveWaitForEvent* UReactiveWaitForEvent::WaitForWorldEvent(UObject* WorldContextObject, FGameplayTag Event,
	bool bOnlyOnce, bool bExactMatch)
{
	UReactiveWaitForEvent* Action = Make(WorldContextObject, Event, FGameplayTag(), bOnlyOnce, bExactMatch);
	Action->bIdentityScope = false;
	return Action;
}

UReactiveWaitForEvent* UReactiveWaitForEvent::WaitForIdentityEvent(UObject* WorldContextObject, FGameplayTag Event,
	FGameplayTag Identity, bool bOnlyOnce, bool bExactMatch)
{
	UReactiveWaitForEvent* Action = Make(WorldContextObject, Event, Identity, bOnlyOnce, bExactMatch);
	Action->bIdentityScope = true;
	return Action;
}

UReactiveWaitForEvent* UReactiveWaitForEvent::Make(UObject* WorldContextObject, FGameplayTag Event, FGameplayTag Identity,
	bool bOnlyOnce, bool bExactMatch)
{
	UReactiveWaitForEvent* Action = NewObject<UReactiveWaitForEvent>();
	Action->Owner = WorldContextObject;
	Action->ListenFor = Event;
	Action->ListenIdentity = Identity;
	Action->bOnlyOnce = bOnlyOnce;
	Action->bExactMatch = bExactMatch;
	Action->RegisterWithGameInstance(WorldContextObject); // kept alive until it stops
	return Action;
}

void UReactiveWaitForEvent::Activate()
{
	Super::Activate();

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Owner.Get(), EGetWorldErrorMode::ReturnNull) : nullptr;
	UReactionSubsystem* Found = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;

	const TCHAR* Problem = nullptr;
	if (!ListenFor.IsValid())
	{
		Problem = TEXT("no Event set");
	}
	else if (bIdentityScope && !ListenIdentity.IsValid())
	{
		Problem = TEXT("no Identity set");
	}
	else if (!Found)
	{
		Problem = TEXT("no Reaction Subsystem (only in play, not the editor world)");
	}

	if (Problem)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s: %s, so nothing to wait for."),
			bIdentityScope ? TEXT("Wait For Identity Event") : TEXT("Wait For World Event"), Problem);
		Cancel();
		return;
	}

	ListenWorld = World;
	Subsystem = Found;
	EventHandle = Found->OnEventSent.AddUObject(this, &UReactiveWaitForEvent::HandleEvent);

	// Stop with the level, so a listener placed by a widget never outlives its world
	CleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UReactiveWaitForEvent::HandleWorldCleanup);
}

void UReactiveWaitForEvent::HandleEvent(const FReactiveEventInfo& Info)
{
	// Whatever placed the node is gone (a widget removed, an actor destroyed): stop
	if (!Owner.IsValid())
	{
		Cancel();
		return;
	}

	if (!Subsystem.IsValid() || !ShouldBroadcastDelegates()
		|| !UReactionSubsystem::EventMatches(Info.Event, ListenFor, bExactMatch)
		|| (bIdentityScope && !UReactionSubsystem::EventSentToIdentity(Info, ListenIdentity)))
	{
		return;
	}

	// Stop first with Only Once, so On Event can safely start another wait
	if (bOnlyOnce)
	{
		StopListening();
	}

	OnEvent.Broadcast(Info.Event, Info.Instigator);

	if (bOnlyOnce)
	{
		Super::Cancel(); // done: let it be cleaned up
	}
}

void UReactiveWaitForEvent::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (World == ListenWorld.Get())
	{
		Cancel();
	}
}

void UReactiveWaitForEvent::StopListening()
{
	if (UReactionSubsystem* Listening = Subsystem.Get())
	{
		Listening->OnEventSent.Remove(EventHandle);
	}
	EventHandle.Reset();
	Subsystem.Reset();

	if (CleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
		CleanupHandle.Reset();
	}
}

void UReactiveWaitForEvent::Cancel()
{
	StopListening();
	Super::Cancel();
}

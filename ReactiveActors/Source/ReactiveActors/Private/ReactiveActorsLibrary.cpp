// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveActorsLibrary.h"
#include "ReactiveActorsLog.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ReactiveAction.h"
#include "ReactionSubsystem.h"
#include "ReactiveStateCache.h"

UReactionSubsystem* UReactiveActorsLibrary::GetSubsystem(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;

	// Only exists in gameplay worlds (PIE and the packaged game), not the editor
	return World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
}

bool UReactiveActorsLibrary::BroadcastEvent(const UObject* WorldContextObject, FGameplayTag Event, AActor* Instigator)
{
	UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->BroadcastEvent(Event, Instigator);
}

void UReactiveActorsLibrary::SuspendEvents(const UObject* WorldContextObject)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->SetEventsSuspended(true);
	}
}

void UReactiveActorsLibrary::ResumeEvents(const UObject* WorldContextObject)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->SetEventsSuspended(false);
	}
}

bool UReactiveActorsLibrary::AreEventsSuspended(const UObject* WorldContextObject)
{
	const UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->AreEventsSuspended();
}

void UReactiveActorsLibrary::CancelAllReactions(const UObject* WorldContextObject)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->CancelAllReactions();
	}
}

bool UReactiveActorsLibrary::RunAction(const UObject* WorldContextObject, UReactiveAction* Action, AActor* Target, AActor* Instigator)
{
	UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->RunAction(Action, Target, Instigator);
}

UReactiveAction* UReactiveActorsLibrary::CreateActionForRun(UObject* WorldContextObject, TSubclassOf<UReactiveAction> ActionClass)
{
	if (!ActionClass || ActionClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("Run Action: no action class (or an abstract one) picked."));
		return nullptr;
	}

	// Owned by whatever placed the node (an actor, a widget...), like Construct Object from Class with Outer = Self
	UObject* Outer = WorldContextObject ? WorldContextObject : GetTransientPackage();
	return NewObject<UReactiveAction>(Outer, ActionClass);
}

bool UReactiveActorsLibrary::RunActions(const UObject* WorldContextObject, const TArray<UReactiveAction*>& Actions, AActor* Target,
	AActor* Instigator, FGameplayTag Event)
{
	UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->RunActions(Actions, Target, Instigator, Event);
}

bool UReactiveActorsLibrary::RunReaction(const UObject* WorldContextObject, const FReaction& Reaction, AActor* Target,
	AActor* Instigator)
{
	UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->RunReaction(Reaction, Target, Instigator);
}

bool UReactiveActorsLibrary::BroadcastEventToActor(const UObject* WorldContextObject, AActor* Actor, FGameplayTag Event, AActor* Instigator)
{
	UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->BroadcastEventToActor(Actor, Event, Instigator);
}

bool UReactiveActorsLibrary::BroadcastEventToIdentity(const UObject* WorldContextObject, FGameplayTag Identity, FGameplayTag Event, AActor* Instigator)
{
	UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject);
	return Subsystem && Subsystem->BroadcastEventToIdentity(Identity, Event, Instigator);
}

void UReactiveActorsLibrary::CancelAllReactionsForActor(const UObject* WorldContextObject, AActor* Actor)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->CancelReactionsForActor(Actor);
	}
}

void UReactiveActorsLibrary::CancelAllReactionsForIdentity(const UObject* WorldContextObject, FGameplayTag Identity)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->CancelReactionsForIdentity(Identity);
	}
}

void UReactiveActorsLibrary::CancelAllReactionsForEvent(const UObject* WorldContextObject, FGameplayTag Event)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->CancelReactionsForEvent(Event);
	}
}

void UReactiveActorsLibrary::ResetAllReactions(const UObject* WorldContextObject)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->ResetAllReactions();
	}
}

void UReactiveActorsLibrary::ResetAllReactionsForActor(const UObject* WorldContextObject, AActor* Actor)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->ResetReactionsForActor(Actor);
	}
}

void UReactiveActorsLibrary::ResetAllReactionsForIdentity(const UObject* WorldContextObject, FGameplayTag Identity)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->ResetReactionsForIdentity(Identity);
	}
}

void UReactiveActorsLibrary::ResetAllReactionsForEvent(const UObject* WorldContextObject, FGameplayTag Event)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->ResetReactionsForEvent(Event);
	}
}

// ---- Save and load --------------------------------------------------------------------------------

FReactiveWorldState UReactiveActorsLibrary::GetWorldReactionState(const UObject* WorldContextObject)
{
	UReactiveStateCache* Cache = UReactiveStateCache::Get(WorldContextObject);
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return Cache && World ? Cache->GetWorldState(World) : FReactiveWorldState();
}

void UReactiveActorsLibrary::SetWorldReactionState(const UObject* WorldContextObject, const FReactiveWorldState& State)
{
	UReactiveStateCache* Cache = UReactiveStateCache::Get(WorldContextObject);
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (Cache)
	{
		Cache->SetWorldState(World, State);
	}
}

void UReactiveActorsLibrary::ClearWorldReactionState(const UObject* WorldContextObject)
{
	if (UReactiveStateCache* Cache = UReactiveStateCache::Get(WorldContextObject))
	{
		Cache->ClearAll();
	}
}

void UReactiveActorsLibrary::HoldReactionsUntilRestored(const UObject* WorldContextObject)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->HoldUntilRestored();
	}
}

void UReactiveActorsLibrary::ReleaseHeldReactions(const UObject* WorldContextObject)
{
	if (UReactionSubsystem* Subsystem = GetSubsystem(WorldContextObject))
	{
		Subsystem->ReleaseHeld();
	}
}

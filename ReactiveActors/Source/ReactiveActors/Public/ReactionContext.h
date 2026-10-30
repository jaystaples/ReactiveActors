// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Shared types: the context passed to every action a runner runs (what to act on, who caused it,
// and why), and the scope used wherever an event is sent or heard.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ReactionContext.generated.h"

class AActor;

/**
 * Where an event goes (a trigger's Scope, Broadcast Event's Scope), or where an Event Received
 * trigger listens (World or Self only).
 */
UENUM(BlueprintType)
enum class EReactiveScopePolicy : uint8
{
	/** Every actor listening for the event, this one included. When listening: any event sent anywhere. */
	World,
	/** Only this actor (for an action: its target). When listening: events that reach this actor. */
	Self,
	/** Every actor with Target Identity, or a child of it. */
	Identity,
};

/** How a run ended: a reaction, a one-off Run Action, or the actions inside a Branch or Run On Targets. */
UENUM(BlueprintType)
enum class EReactionResult : uint8
{
	/** Every action finished with Success, or a Stop Reaction (Succeeded) ended it early. */
	Succeeded,
	/** An action finished with Success off, or a Stop Reaction (Failed): the rest of the chain was skipped. */
	Failed,
	/** Stopped from outside: Cancel / Reset Reactions, a Restart, its target destroyed or streamed out. */
	Cancelled,
};

/** Small helpers for the one-line summaries blocks show as their row titles in the editor. */
namespace ReactiveSummary
{
	/** "Event.Door.Open", or Fallback when the tag is empty. */
	inline FString Tag(const FGameplayTag& InTag, const TCHAR* Fallback = TEXT("<no event>"))
	{
		return InTag.IsValid() ? InTag.ToString() : FString(Fallback);
	}

	/** "World", "Self", or "Identity Actor.Kitchen". */
	inline FString Scope(EReactiveScopePolicy InScope, const FGameplayTag& Identity)
	{
		switch (InScope)
		{
		case EReactiveScopePolicy::Self:     return TEXT("Self");
		case EReactiveScopePolicy::Identity: return FString::Printf(TEXT("Identity %s"), *Tag(Identity, TEXT("<no identity>")));
		default:                             return TEXT("World");
		}
	}

	/** Display names of the objects in a list, joined: "Has Actor Tags, Is Player". Empty slots skipped. */
	template <typename ObjectType>
	FString Names(const TArray<TObjectPtr<ObjectType>>& Objects, const TCHAR* Separator = TEXT(", "))
	{
		TArray<FString> Parts;
		for (const ObjectType* Object : Objects)
		{
			if (Object)
			{
				Parts.Add(Object->BuildSummary());
			}
		}
		return FString::Join(Parts, Separator);
	}
}

/** Who a run acts on, who caused it, and why. Blueprints can make one (Make Reaction Context) for Run Reaction Preset. */
USTRUCT(BlueprintType)
struct REACTIVEACTORS_API FReactionContext
{
	GENERATED_BODY()

	/** What the actions act on. Defaults to the actor that owns the actions. */
	UPROPERTY(BlueprintReadWrite, Category = "Reactive Actors")
	TObjectPtr<AActor> Target = nullptr;

	/** Who caused this (the player, a camera...). May be null. */
	UPROPERTY(BlueprintReadWrite, Category = "Reactive Actors")
	TObjectPtr<AActor> Instigator = nullptr;

	/** The event that started this run. May be empty. */
	UPROPERTY(BlueprintReadWrite, Category = "Reactive Actors")
	FGameplayTag Event;
};

/** How an event was sent. */
UENUM()
enum class EReactiveEventRoute : uint8
{
	/** To everyone listening (Broadcast Event, World scope). */
	World,
	/** To one actor (Broadcast Event To Actor, Self scope). */
	Actor UMETA(DisplayName = "To Actor"),
	/** To every actor with an identity (Broadcast Event To Identity, Identity scope). */
	Identity UMETA(DisplayName = "To Identity"),
};

/**
 * One event that was sent: what it was, who caused it, where it came from and where it went.
 * C++ only: the subsystem's OnEventSent hands it to listeners. Blueprints get Event and Instigator
 * from the Wait For World / Identity Event nodes.
 */
USTRUCT()
struct REACTIVEACTORS_API FReactiveEventInfo
{
	GENERATED_BODY()

	/** The event tag. */
	UPROPERTY()
	FGameplayTag Event;

	/** Who caused it (the player, a trigger's actor...). May be empty. */
	UPROPERTY()
	TObjectPtr<AActor> Instigator = nullptr;

	/** The actor that sent it (a trigger's actor, a Broadcast Event's target). Same as Instigator when not known. */
	UPROPERTY()
	TObjectPtr<AActor> Source = nullptr;

	/** How it was sent: to everyone, to one actor, or to an identity. */
	UPROPERTY()
	EReactiveEventRoute Route = EReactiveEventRoute::World;

	/** Route To Actor: the actor it was sent to. */
	UPROPERTY()
	TObjectPtr<AActor> TargetActor = nullptr;

	/** Route To Identity: the identity it was sent to. */
	UPROPERTY()
	FGameplayTag TargetIdentity;
};


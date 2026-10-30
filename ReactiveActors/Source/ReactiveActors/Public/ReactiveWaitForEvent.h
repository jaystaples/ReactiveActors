// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Wait For World Event / Wait For Identity Event: Blueprint nodes that fire their On Event pin
// whenever a matching event is sent, from any Blueprint (widgets, the player controller, a quest
// manager, the level Blueprint), with no Reaction Component needed. Same hook as the Wait For Event
// action and the Event Received trigger (the subsystem's OnEventSent).
//
//   Wait For World Event      the event, however it was sent (to everyone, to an actor, to an identity).
//   Wait For Identity Event   only the event sent to that identity (Broadcast Event To Identity), or to
//                             a parent of it: Actor.Kitchen.Lamp hears what's sent to Actor.Kitchen.
//
// It stops when you call Cancel on its Async Action pin, after the first match with Only Once, when
// the object that placed it is destroyed, or when its level ends. Events ignored while events are
// suspended aren't heard.

#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "GameplayTagContainer.h"
#include "ReactionContext.h"
#include "ReactiveWaitForEvent.generated.h"

class UReactionSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FReactiveEventHeard, FGameplayTag, Event, AActor*, Instigator);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class REACTIVEACTORS_API UReactiveWaitForEvent : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	/**
	 * Fires On Event every time Event (or a child of it) is sent, however it's sent. Then fires straight away,
	 * so the graph carries on. Only Once: stop after the first match. Exact Match: only this tag, not its children.
	 * Keep the Async Action pin to Cancel it later. Call Run Action / Run Actions from On Event to react.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Wait For World Event",
			Keywords = "wait for event listen message gameplay reactive", AdvancedDisplay = "bExactMatch"))
	static UReactiveWaitForEvent* WaitForWorldEvent(UObject* WorldContextObject, FGameplayTag Event,
		bool bOnlyOnce = false, bool bExactMatch = false);

	/**
	 * Fires On Event every time Event (or a child of it) is sent to Identity: Broadcast Event To Identity, or
	 * Identity scope. Sends to a parent identity count (Actor.Kitchen.Lamp hears what's sent to Actor.Kitchen);
	 * events sent to everyone don't. Only Once: stop after the first match. Exact Match: only this event tag,
	 * not its children. Keep the Async Action pin to Cancel it later.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Wait For Identity Event",
			Keywords = "wait for event listen message gameplay reactive", AdvancedDisplay = "bExactMatch"))
	static UReactiveWaitForEvent* WaitForIdentityEvent(UObject* WorldContextObject, FGameplayTag Event, FGameplayTag Identity,
		bool bOnlyOnce = false, bool bExactMatch = false);

	/** A matching event was sent. Instigator: who caused it (may be empty). */
	UPROPERTY(BlueprintAssignable)
	FReactiveEventHeard OnEvent;

	virtual void Activate() override;
	virtual void Cancel() override;

private:
	static UReactiveWaitForEvent* Make(UObject* WorldContextObject, FGameplayTag Event, FGameplayTag Identity,
		bool bOnlyOnce, bool bExactMatch);

	void HandleEvent(const FReactiveEventInfo& Info);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void StopListening();

	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<UWorld> ListenWorld;
	TWeakObjectPtr<UReactionSubsystem> Subsystem;
	FDelegateHandle EventHandle;
	FDelegateHandle CleanupHandle;
	FGameplayTag ListenFor;
	/** Set: Wait For Identity Event. Empty: Wait For World Event. */
	FGameplayTag ListenIdentity;
	bool bIdentityScope = false;
	bool bOnlyOnce = false;
	bool bExactMatch = false;
};

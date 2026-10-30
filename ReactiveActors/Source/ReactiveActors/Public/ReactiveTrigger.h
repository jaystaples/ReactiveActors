// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// A Reactive Trigger watches for its own condition and, when it's met, sends an event.
// Authored inline in a Reaction Component's Triggers array, like actions.
//
//   On Activate     the actor has begun play: bind to what you watch (an overlap...), start timers.
//   On Deactivate   the actor is ending play: unbind and clear timers.
//   On Tick         optional, for conditions that have to be polled. Only ticks if you implement it.
//   Fire Trigger    call it when the condition is met. It sends the event for you.
//   On Fired        optional: runs after Fire Trigger has sent the event, for extras (a sound, a log).
//
// The Event is optional: with none, a fire still checks rules and conditions, tells the rules,
// and runs On Fired; it just doesn't send anything.
//
// When it fires: Rules (memory: Max Activations, Cooldown) can block it, then Conditions
// (requirements about the moment: is the instigator the player?) must pass, then the event is sent
// to the chosen Scope. For a trigger's rules, a fire is a run that takes no time: they get
// On Start and On Finish back to back.
//
// Default scope is World: every actor listening for the event hears it, this one included.

#pragma once

#include "CoreMinimal.h"
#include "ReactiveObjectBase.h"
#include "Engine/EngineTypes.h"
#include "ReactionContext.h"
#include "GameplayTagContainer.h"
#include "ReactiveTrigger.generated.h"

class UReactiveRule;
class UReactionComponent;
class UReactiveRequirement;

/** Watches for something and sends an event. Bind in On Activate, unbind in On Deactivate, call Fire Trigger. */
UCLASS(Abstract, Blueprintable, BlueprintType,
	meta = (DisplayName = "Reactive Trigger", PrioritizeCategories = "Trigger"))
class REACTIVEACTORS_API UReactiveTrigger : public UReactiveObjectBase
{
	GENERATED_BODY()

public:
	// ---- Lifecycle (C++ only, driven by the owning component) ------------------------------------

	/** The owning component has begun play (called after its reactions are registered). */
	void Activate(UReactionComponent* InComponent);

	/** The owning component is ending play. */
	void Deactivate(EEndPlayReason::Type Reason);

	/** Ticked by the owning component, if this trigger ticks (see WantsTick). */
	void TickTrigger(float DeltaTime);

	/** Reset All Reactions and friends: the trigger's own On Reset, then its rules'. */
	void ResetTrigger();

	/**
	 * A copy of this trigger owned by NewOuter, with its own rules (they keep state).
	 * Used to give each actor its own copies of a preset's triggers.
	 */
	UReactiveTrigger* DuplicateFor(UObject* NewOuter) const;

	/** DescribeTrigger, then where its event goes: "Timer 5.0s -> Event.Scare (World)". */
	virtual FString BuildSummary() const override;

	bool IsActive() const { return bActive; }
	bool IsEnabled() const { return bEnabled; }

	/** True if a Blueprint trigger implements On Tick, or a C++ one sets bTickInCpp. Worked out on Activate. */
	bool WantsTick() const { return bWantsTick; }

	/**
	 * C++ hooks for triggers that react to events (Event Received). Events this trigger wants sent to
	 * this actor: the component registers for them, so broadcasts reach it.
	 */
	virtual void GetHeardEvents(FGameplayTagContainer& OutEvents) const {}

	/** An event reached this actor's component. Return true if this trigger fired because of it. */
	virtual bool HandleEventToActor(const FGameplayTag& InEvent, AActor* Instigator) { return false; }

	// ---- Blueprint -------------------------------------------------------------------------------

	/**
	 * The condition is met: checks rules and conditions, then sends this trigger's event (if it has one)
	 * to its Scope. No need to broadcast anything yourself.
	 * Instigator: who caused it (the overlapping actor...). Empty: this trigger's actor is the instigator.
	 * Returns true if it fired (not whether anyone reacted to the event). False if the trigger isn't
	 * active (the actor hasn't begun play, or has ended it), is disabled, events are suspended,
	 * a rule or condition blocked it, or it's already firing (a loop).
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Trigger", meta = (UnsafeDuringActorConstruction = "true"))
	bool FireTrigger(AActor* Instigator = nullptr);

	/** The actor this trigger is on. */
	UFUNCTION(BlueprintPure, Category = "Reactive Trigger", meta = (DisplayName = "Get Actor"))
	AActor* GetTriggerActor() const;

	/** The event this trigger sends. */
	UFUNCTION(BlueprintPure, Category = "Reactive Trigger")
	FGameplayTag GetEvent() const { return Event; }

	const TArray<TObjectPtr<UReactiveRule>>& GetRules() const { return Rules; }
	const TArray<TObjectPtr<UReactiveRequirement>>& GetConditions() const { return Conditions; }

	// Debug info for the overlay (not saved)
	double GetLastFiredTime() const { return LastFiredTime; }
	double GetLastBlockedTime() const { return LastBlockedTime; }
	const FString& GetLastBlockedBy() const { return LastBlockedBy; }

#if WITH_EDITOR
	/** Map Check: empty slots, a trigger that does nothing, Identity scope with no identity. Subclasses can add their own. */
	virtual void CheckForErrors(TFunctionRef<void(const FText&)> Warn) const;
#endif

protected:
	/** C++: what this trigger watches, for its summary ("Timer 5.0s"). Default: the class's display name. */
	virtual FString DescribeTrigger() const;

	// ---- Overridable events -----------------------------------------------------------------------

	/** The actor has begun play: bind to what this trigger watches, start timers. */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Trigger", meta = (DisplayName = "On Activate"))
	void OnActivate();
	virtual void OnActivate_Implementation() {}

	/** The actor is ending play: unbind and clear timers. Fire Trigger still works in here. */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Trigger", meta = (DisplayName = "On Deactivate"))
	void OnDeactivate(EEndPlayReason::Type Reason);
	virtual void OnDeactivate_Implementation(EEndPlayReason::Type Reason) {}

	/** Every frame while active, for conditions that have to be polled. The trigger only ticks if you implement this. */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Trigger", meta = (DisplayName = "On Tick"))
	void OnTick(float DeltaTime);
	virtual void OnTick_Implementation(float DeltaTime) {}

	/**
	 * The trigger fired and its event has already been sent (you don't broadcast it here). For extras on
	 * the trigger itself: a sound, a log. Context: this actor, the instigator, and the event (empty if none).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Trigger", meta = (DisplayName = "On Fired"))
	void OnFired(const FReactionContext& Context);
	virtual void OnFired_Implementation(const FReactionContext& Context) {}

	/** Reset All Reactions (and its For... versions): clear any state this trigger keeps. */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Trigger", meta = (DisplayName = "On Reset"))
	void OnReset();
	virtual void OnReset_Implementation() {}

	/** C++ triggers that override OnTick_Implementation: set this in the constructor (Blueprint ones are detected). */
	bool bTickInCpp = false;

	// ---- Settings ---------------------------------------------------------------------------------

public:
	/** Identifies this trigger in saved games (given automatically in the editor; a copy gets a new one). Not shown. */
	UPROPERTY()
	FGuid TriggerId;

protected:
	/** Disabled triggers never activate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	bool bEnabled = true;

	/** The event to send when this trigger fires. Optional: with none, it still fires (rules, On Fired), it just sends nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (ReactiveTags = "Event"))
	FGameplayTag Event;

	/** World: everyone listening (this actor too). Self: this actor's reactions only. Identity: actors with Target Identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (DisplayName = "Event Scope"))
	EReactiveScopePolicy Scope = EReactiveScopePolicy::World;

	/** Identity Scope: send to actors with this identity, or a child of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger",
		meta = (ReactiveTags = "Identity", EditCondition = "Scope == EReactiveScopePolicy::Identity", EditConditionHides))
	FGameplayTag TargetIdentity;

	/** Rules with memory (Max Activations, Cooldown): any one can stop a fire. Checked first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Trigger", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveRule>> Rules;

	/** Checks about the moment (who caused it, the actor's state): all must pass. Checked after Rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Trigger", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveRequirement>> Conditions;

private:
	/** Sends Event to Scope (this actor, the world, or an identity). */
	void SendEvent(const FReactionContext& Context);

	TWeakObjectPtr<UReactionComponent> Component;
	bool bActive = false;
	bool bWantsTick = false;

	/** True while Fire Trigger is sending: a second fire from inside that send is a loop and is refused. */
	bool bFiring = false;

	// Debug info for the overlay (not saved)
	double LastFiredTime = -1.0;
	double LastBlockedTime = -1.0;
	FString LastBlockedBy;
};

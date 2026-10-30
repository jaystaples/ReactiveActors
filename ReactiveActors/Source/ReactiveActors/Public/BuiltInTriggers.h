// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in triggers
//
// Generic triggers that ship with the plugin. NotBlueprintable keeps them out of the
// "pick parent class" list; they still appear in every Triggers dropdown.
// Want one to fire only once? Add a Max Activations rule set to 1.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "ReactiveTrigger.h"
#include "BuiltInTriggers.generated.h"

// ---- Begin Play ---------------------------------------------------------------------------------

/**
 * Fires when the actor begins play, every time it does (including a data layer going back to
 * Activated, or a streaming level shown again). For once only, add Max Activations = 1.
 * It fires on the next tick, not inside Begin Play: actors beginning play at the same time are listening by
 * then, and a save restored as the level loads (Set World Reaction State from a Game Mode or Level Blueprint
 * Begin Play, or a save system's load event) is already in place, so Max Activations counts from the save.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Begin Play"))
class REACTIVEACTORS_API UTRG_BeginPlay : public UReactiveTrigger
{
	GENERATED_BODY()

protected:
	virtual void OnActivate_Implementation() override;
	virtual void OnDeactivate_Implementation(EEndPlayReason::Type Reason) override;

private:
	FTimerHandle NextFrameHandle;
};

// ---- End Play -----------------------------------------------------------------------------------

/**
 * Fires when the actor ends play: when it's destroyed, and optionally when it's unloaded (streamed out,
 * its data layer deactivated). Never when the game or level is shutting down.
 * The actor is on its way out, so its own reactions get instant actions only: ones that take time are stopped
 * straight after. Best with World or Identity scope, telling other actors.
 */
// Hidden from the Triggers dropdown: it fires while actors are on their way out, which is easy to misuse.
// Existing ones still load and work.
UCLASS(NotBlueprintable, HideDropdown, meta = (DisplayName = "End Play"))
class REACTIVEACTORS_API UTRG_EndPlay : public UReactiveTrigger
{
	GENERATED_BODY()

protected:
	virtual void OnDeactivate_Implementation(EEndPlayReason::Type Reason) override;
	virtual FString DescribeTrigger() const override;

	/** Also fire when the actor is unloaded (World Partition streaming, data layers), not only when destroyed. */
	UPROPERTY(EditAnywhere, Category = "Trigger")
	bool bAlsoWhenUnloaded = false;
};

// ---- Timer --------------------------------------------------------------------------------------

/** Fires after a delay, once or repeatedly, optionally at a random interval. Pauses with the game. */
UCLASS(NotBlueprintable, meta = (DisplayName = "Timer"))
class REACTIVEACTORS_API UTRG_Timer : public UReactiveTrigger
{
	GENERATED_BODY()

protected:
	virtual void OnActivate_Implementation() override;
	virtual void OnDeactivate_Implementation(EEndPlayReason::Type Reason) override;
	virtual FString DescribeTrigger() const override;

	/** Seconds after the actor begins play before the first fire. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = 0, UIMin = 0, Units = "s"))
	float FirstDelay = 1.f;

	/** Keep firing after the first time. */
	UPROPERTY(EditAnywhere, Category = "Trigger")
	bool bRepeat = false;

	/** Seconds between fires when repeating. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (EditCondition = "bRepeat", ClampMin = 0.01, UIMin = 0.01, Units = "s"))
	float Interval = 5.f;

	/** Random variation added to each wait, up to this many seconds either way (0 = exact). */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = 0, UIMin = 0, Units = "s"))
	float RandomDeviation = 0.f;

private:
	void Schedule(float BaseDelay);
	void HandleTimer();

	FTimerHandle TimerHandle;
};

// ---- Event Received -----------------------------------------------------------------------------

/**
 * Fires its own event whenever another event is sent: "when Event.Lights.Off happens, send Event.Scare".
 * The instigator is passed on. Child tags count, like reactions: listening for Event.Door hears Event.Door.Open.
 * Events ignored while events are suspended aren't heard either.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Event Received"))
class REACTIVEACTORS_API UTRG_EventReceived : public UReactiveTrigger
{
	GENERATED_BODY()

public:
	virtual void GetHeardEvents(FGameplayTagContainer& OutEvents) const override;
	virtual bool HandleEventToActor(const FGameplayTag& InEvent, AActor* Instigator) override;

#if WITH_EDITOR
	virtual void CheckForErrors(TFunctionRef<void(const FText&)> Warn) const override;
#endif

protected:
	virtual void OnActivate_Implementation() override;
	virtual void OnDeactivate_Implementation(EEndPlayReason::Type Reason) override;
	virtual FString DescribeTrigger() const override;

	/** The event to listen for (or a child of it). */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ReactiveTags = "Event"))
	FGameplayTag ListenFor;

	/**
	 * World: hear it wherever it's sent. Self: only when it reaches this actor (broadcasts, plus anything
	 * sent to it or to its identity).
	 */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (InvalidEnumValues = "Identity"))
	EReactiveScopePolicy ListenScope = EReactiveScopePolicy::World;

private:
	void HandleWorldEvent(const FReactiveEventInfo& Info);

	FDelegateHandle WorldHandle;
};

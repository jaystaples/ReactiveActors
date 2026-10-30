// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Blueprint shortcuts for the Reaction Subsystem: usable from any Blueprint
// (actors, widgets, the level Blueprint) without getting the subsystem first.

#pragma once

#include "CoreMinimal.h"
#include "Reaction.h"
#include "ReactiveSaveData.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "ReactiveActorsLibrary.generated.h"

class AActor;
class UReactiveAction;
class UReactionSubsystem;

UCLASS()
class REACTIVEACTORS_API UReactiveActorsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Sends Event to every Reaction Component listening for it (or for one of its parents).
	 * Returns true if at least one reaction fired.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static bool BroadcastEvent(const UObject* WorldContextObject, FGameplayTag Event, AActor* Instigator = nullptr);

	/**
	 * Ignores every event until Resume Events (cutscenes, menus, scripted moments).
	 * Reactions already running carry on: use Cancel All Reactions to stop those too.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void SuspendEvents(const UObject* WorldContextObject);

	/** Lets events through again after Suspend Events. */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void ResumeEvents(const UObject* WorldContextObject);

	/** True while events are suspended. */
	UFUNCTION(BlueprintPure, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject"))
	static bool AreEventsSuspended(const UObject* WorldContextObject);

	/**
	 * Cancels every running reaction in the world, and every action started with Run Action.
	 * Actions still running get On Cancel; cancelled runs count as finished (Result Cancelled), so rules are told.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void CancelAllReactions(const UObject* WorldContextObject);

	/**
	 * Sends Event to the Reaction Components on Actor only (what Broadcast Event's Self scope does).
	 * Returns true if any reaction fired.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static bool BroadcastEventToActor(const UObject* WorldContextObject, AActor* Actor, FGameplayTag Event, AActor* Instigator = nullptr);

	/**
	 * Sends Event only to actors with Identity (or a child of it), e.g. tell every Actor.Kitchen thing the lights went out.
	 * Returns true if any reaction fired.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static bool BroadcastEventToIdentity(const UObject* WorldContextObject, FGameplayTag Identity, FGameplayTag Event, AActor* Instigator = nullptr);

	/** Cancels the running reactions on Actor, and any Run Action / Run Actions / Run Reaction aimed at it. */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void CancelAllReactionsForActor(const UObject* WorldContextObject, AActor* Actor);

	/** Cancels the running reactions on every actor with Identity (or a child of it). */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void CancelAllReactionsForIdentity(const UObject* WorldContextObject, FGameplayTag Identity);

	/**
	 * Cancels running reactions listening for Event (or a child of it), or running because of it,
	 * and one-off runs started with it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void CancelAllReactionsForEvent(const UObject* WorldContextObject, FGameplayTag Event);

	/**
	 * Resets every reaction in the world as if it had never fired: running ones are cancelled, then every
	 * rule gets On Reset (Max Activations back to 0, Cooldown cleared). For checkpoints and New Game.
	 * One-off runs (Run Action...) are cancelled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void ResetAllReactions(const UObject* WorldContextObject);

	/** Resets the reactions on Actor (see Reset All Reactions). */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void ResetAllReactionsForActor(const UObject* WorldContextObject, AActor* Actor);

	/** Resets the reactions on every actor with Identity (or a child of it). */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void ResetAllReactionsForIdentity(const UObject* WorldContextObject, FGameplayTag Identity);

	/** Resets every reaction listening for Event (or a child of it). */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void ResetAllReactionsForEvent(const UObject* WorldContextObject, FGameplayTag Event);

	/**
	 * Internal, for the Run Action node (which makes the action, sets its pins, then calls this).
	 * Runs Action once on Target, without a Reaction Component. Class Requirements still apply.
	 * Returns false if it didn't run (no action, no target, already running, or a requirement failed).
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static bool RunAction(const UObject* WorldContextObject, UReactiveAction* Action, AActor* Target, AActor* Instigator = nullptr);

	/**
	 * Deprecated: use the Run Action node for one action, or put the actions in a Reaction Preset and use Run Reaction Preset.
	 * Runs Actions in order on Target, without a Reaction Component (each waits for the one before to call Finish Action).
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Event",
		UnsafeDuringActorConstruction = "true", DeprecatedFunction,
		DeprecationMessage = "Use Run Action for one action, or a Reaction Preset with Run Reaction Preset."))
	static bool RunActions(const UObject* WorldContextObject, const TArray<UReactiveAction*>& Actions, AActor* Target,
		AActor* Instigator = nullptr, FGameplayTag Event = FGameplayTag());

	/**
	 * Deprecated: use Run Reaction Preset, which keeps rule state for you and tells you when the run ends.
	 * Runs a reaction struct once on Target, without a Reaction Component.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true",
		DeprecatedFunction, DeprecationMessage = "Use Run Reaction Preset: it keeps rule state for you and has an On Finished pin."))
	static bool RunReaction(const UObject* WorldContextObject, const FReaction& Reaction, AActor* Target,
		AActor* Instigator = nullptr);

	/**
	 * Internal, for the Run Action node: a new action of ActionClass, owned by WorldContextObject (so Get Owner
	 * returns it), ready for its Expose on Spawn variables to be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors", meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UReactiveAction* CreateActionForRun(UObject* WorldContextObject, TSubclassOf<UReactiveAction> ActionClass);

	// ---- Save and load ---------------------------------------------------------------------------

	/**
	 * Everything that changed in the current level (Max Activations counts, Cooldowns, reactions switched off, your
	 * rules' SaveGame variables), including actors that have streamed out: the one struct to put in your save, next to
	 * whatever else you save for this level. Actors where nothing happened aren't included.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save", meta = (WorldContext = "WorldContextObject"))
	static FReactiveWorldState GetWorldReactionState(const UObject* WorldContextObject);

	/**
	 * Hands a saved Reactive World State back. Call it before Open Level and the level gets it as it starts, or after
	 * the level has loaded (in the Game Mode's Begin Play) and it's applied straight away; if that's a few frames after
	 * the level starts, call Hold Reactions Until Restored in Begin Play first. Actors not loaded yet get theirs when
	 * they stream in.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save", meta = (WorldContext = "WorldContextObject"))
	static void SetWorldReactionState(const UObject* WorldContextObject, const FReactiveWorldState& State);

	/**
	 * Forgets all cached and waiting reaction state (New Game, chapter reset). Actors already loaded keep what they have
	 * until the level ends; use Reset All Reactions to reset those too.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save", meta = (WorldContext = "WorldContextObject"))
	static void ClearWorldReactionState(const UObject* WorldContextObject);

	/**
	 * For loading a save after the level has started: triggers and Reactive Volumes hold what they'd fire until Set
	 * World Reaction State, so nothing runs before the saved counts are back. Call it in the Game Mode's Begin Play.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save", meta = (WorldContext = "WorldContextObject"))
	static void HoldReactionsUntilRestored(const UObject* WorldContextObject);

	/** Fires what Hold Reactions Until Restored held, if the restore isn't coming (a failed load). */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save", meta = (WorldContext = "WorldContextObject"))
	static void ReleaseHeldReactions(const UObject* WorldContextObject);

private:
	/** The subsystem for the world WorldContextObject is in. Null outside gameplay worlds (or logs why). */
	static UReactionSubsystem* GetSubsystem(const UObject* WorldContextObject);
};

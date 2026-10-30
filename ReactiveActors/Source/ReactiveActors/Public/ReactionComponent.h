// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Reaction Component: an actor component holding a list of Reactions.
// Each reaction waits for its event tag and runs its own list of actions.
// Reactions and triggers come from an optional Reaction Preset (run first) and the inline arrays.
//
// Identity Tags name this actor for other actions (e.g. Run On Targets: Actor.Kitchen.Lamp).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Reaction.h"
#include "ReactiveSaveData.h"
#include "GameplayTagContainer.h"
#include "ReactionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReactionStarted, FGameplayTag, Event, int32, ReactionIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReactionFinished, int32, ReactionIndex, EReactionResult, Result);

class UBillboardComponent;
class UReactiveAction;
class UReactionPreset;
class UReactiveTrigger;
class USceneComponent;

UCLASS(ClassGroup = (ReactiveActors), Blueprintable, meta = (BlueprintSpawnableComponent, DisplayName = "Reaction Component"))
class REACTIVEACTORS_API UReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UReactionComponent();

	/**
	 * Runs every reaction whose Event Tag matches Event. Instigator (optional) is passed to the actions.
	 * Returns true if any reaction fired.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors")
	bool ReactTo(FGameplayTag Event, AActor* Instigator = nullptr);

	/**
	 * Cancels this component's running reactions: the action each is waiting on gets On Cancel,
	 * and the run counts as finished (rules get On Finish with Result Cancelled).
	 * Event (optional): only reactions listening for it or a child of it, or running because of it.
	 * Returns how many reactions were cancelled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors")
	int32 CancelReactions(FGameplayTag Event);

	/**
	 * Resets this component's reactions and triggers as if they'd never fired: cancels any running
	 * reactions, then every rule, action and trigger gets On Reset (Max Activations back to 0, Cooldown cleared).
	 * Event (optional): only reactions listening for it (or a child of it), and triggers sending it.
	 * Returns how many reactions and triggers were reset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors")
	int32 ResetReactions(FGameplayTag Event);

	/**
	 * Switches reactions on or off while playing. A disabled reaction doesn't fire; one already running
	 * finishes normally. Event (optional): only reactions listening for it or a child of it.
	 * Returns how many reactions changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors")
	int32 SetReactionsEnabled(FGameplayTag Event, bool bEnabled);

	// ---- Save and load ---------------------------------------------------------------------------

	/**
	 * What this component remembers: each reaction's Enabled, and every rule's and trigger's SaveGame variables
	 * (Max Activations counts, Cooldown time left, your own). Empty before Begin Play. Runs in progress aren't included.
	 * Actors placed in levels are saved for you with Get World Reaction State; use this for actors you spawn yourself,
	 * saved alongside however you save that actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save")
	FReactiveActorState CaptureReactionState();

	/**
	 * Puts back what Capture Reaction State saved. Running reactions are cancelled first. Saved values that no longer
	 * match (a reaction removed or its event changed, a rule swapped for another kind) are skipped.
	 * Before Begin Play (e.g. called by your save system while the level loads), it's kept and applied at Begin Play,
	 * before any trigger fires.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save")
	void RestoreReactionState(const FReactiveActorState& State);

	/** C++: the key this component is saved under: its level and actor, then its own name. */
	FString GetSaveKey() const;

	/**
	 * C++: its actor was placed in a level (not spawned while playing). Only these are cached and included in
	 * Get World Reaction State: a spawned actor comes back with a different name, so its state couldn't find it.
	 * Save spawned actors' reaction state with Capture / Restore Reaction State alongside the actor instead.
	 */
	bool IsPlacedInLevel() const;

	/** C++: something happened here since it began play (a reaction ran, a trigger fired, Enabled changed, a restore). */
	bool HasStateChanged() const { return bStateChanged; }
	void MarkStateChanged() { bStateChanged = true; }

	/** The event tags this component listens for (enabled reactions and Self-scope Event Received triggers, from the Preset and inline). Works in the editor too. */
	UFUNCTION(BlueprintPure, Category = "Reactive Actors")
	FGameplayTagContainer GetEventTags() const;

	/** The tags that identify this actor to other actions (Run On Targets, lookups). */
	UFUNCTION(BlueprintPure, Category = "Reactive Actors")
	FGameplayTagContainer GetIdentityTags() const { return IdentityTags; }

	/** Broadcast whenever a reaction fires (handy for debugging, sounds, UI). */
	UPROPERTY(BlueprintAssignable, Category = "Reactive Actors")
	FOnReactionStarted OnReactionStarted;

	/** Broadcast when a reaction's run has ended (its last action called Finish Action), with how it ended. */
	UPROPERTY(BlueprintAssignable, Category = "Reactive Actors")
	FOnReactionFinished OnReactionFinished;

	/** Read-only view of the reactions this component runs (Preset reactions first, then inline). Built on Begin Play. */
	const TArray<FReaction>& GetActiveReactions() const { return ActiveReactions; }

	/** Read-only view of the triggers this component runs (Preset triggers first, then inline). Built on Begin Play. */
	const TArray<TObjectPtr<UReactiveTrigger>>& GetTriggers() const { return ActiveTriggers; }

	/** Ticks the triggers that need it (only enabled while at least one does). */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Editor icon: created when the component registers in an editor world, removed when it unregisters
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	/**
	 * Bounds of the actor's visible parts, leaving out editor icons and other editor-only components.
	 * Used by the debug views to place text and lines. A point at the actor's location if it has no visible parts.
	 */
	static FBox GetDebugBounds(const AActor* Actor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	/** Map Check (Build > Map Check): the Data Validation checks, plus requirements this actor already fails. */
	virtual void CheckForErrors() override;

	/** Data Validation (on save, or right-click > Validate): empty slots, missing settings, each block's Validate Settings. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	/** New reactions and triggers get IDs for saved games (and copies get new ones). */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Both of the above. OwnerForAheadChecks: test requirements against this actor too (Map Check), or null. */
	void GatherProblems(const AActor* OwnerForAheadChecks, TFunctionRef<void(const FText&, bool)> Report) const;
#endif

	/** Names this actor for other actions, e.g. Actor.Kitchen.Lamp. Parent tags match too (Actor.Kitchen). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Actors", meta = (ReactiveTags = "Identity"))
	FGameplayTagContainer IdentityTags;

	/**
	 * Optional reusable behaviour: a set of reactions and triggers. This actor gets its own copies of them,
	 * run before its own reactions and triggers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Actors")
	TObjectPtr<UReactionPreset> Preset;

	/** This actor's own reactions. They run after the Preset's reactions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Actors", meta = (TitleProperty = "EventTag"))
	TArray<FReaction> Reactions;

	/**
	 * Watch for something (begin play, a timer, another event, or your own Blueprint trigger) and send an event when it happens.
	 * With World scope (the default) every actor listening hears it, this one included.
	 * Self scope sends it to this actor's own reactions only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Reactive Actors", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveTrigger>> Triggers;

private:
	/**
	 * Builds ActiveReactions and ActiveTriggers: copies of the Preset's (owned by this component),
	 * then the inline ones.
	 */
	void BuildActiveLists();

	bool TryTrigger(int32 ReactionIndex, const FGameplayTag& Event, AActor* Instigator);

	/** Starts a run of a reaction that's passed its checks: rules told, debug info, sequence started. */
	void BeginRun(int32 ReactionIndex, const FReactionContext& Context);
	void HandleReactionComplete(int32 ReactionIndex, EReactionResult Result);

	/** The reactions actually run (Preset first, then inline). Built on Begin Play; should never resize while playing. */
	UPROPERTY(Transient)
	TArray<FReaction> ActiveReactions;

	/** The triggers actually run (Preset copies first, then inline). Built with ActiveReactions. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UReactiveTrigger>> ActiveTriggers;

	/** Every action in Reaction, nested ones after their parent: the order their saved state is kept in. */
	static TArray<UReactiveAction*> GatherActions(const FReaction& Reaction);

	/** Applies a saved state. bLive: playing already, so cancel running reactions and re-register changed events. */
	void ApplyState(const FReactiveActorState& State, bool bLive);

	/** A Restore Reaction State that arrived before Begin Play, applied there. */
	FReactiveActorState PendingState;
	bool bHasPendingState = false;

	/** Something happened here (see Has State Changed): only changed components are cached and saved. */
	bool bStateChanged = false;

	/** How many of ActiveReactions / ActiveTriggers came from the Preset (they come first). */
	int32 NumPresetReactions = 0;
	int32 NumPresetTriggers = 0;

	/** Event tags registered with the Reaction Subsystem (so the same set is unregistered). */
	FGameplayTagContainer RegisteredTags;

	/**
	 * ActiveReactions is built once per component. Begin Play can run again on the same actor
	 * (a data layer going from Activated to Loaded and back, a streaming level hidden and shown):
	 * rebuilding would replace the Preset copies and lose their state (rules, triggers).
	 */
	bool bListsBuilt = false;


#if WITH_EDITORONLY_DATA
	/** Editor-only icon above the actor, so designers can see it has a Reaction Component. */
	UPROPERTY(Transient)
	TObjectPtr<UBillboardComponent> EditorSprite;
#endif

#if WITH_EDITOR
	/** Puts the editor icon above the actor's bounds (world up), at Reactions.Show.Offset. */
	void UpdateEditorSpriteLocation();
	void HandleRootTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags Flags, ETeleportType Teleport);

	TWeakObjectPtr<USceneComponent> SpriteRoot;
	FDelegateHandle RootTransformHandle;
#endif
};

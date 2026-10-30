// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

// World-wide routing for Reaction Components.
// - Events: components register under the tags their reactions use; Broadcast Event only
//   calls components registered under the event tag or one of its parents.
// - Identities: components register their Identity Tags so actions can find other actors
//   (Get Actors By Identity, Run On Targets).
// - Debug (not in shipping): console commands, Reactions.Show 1 for live status above actors,
//   and Reactions.Show.WiringLines 1 for lines from each event's source to the actors it reached.
//   The full list of commands and settings is at the top of ReactionSubsystemDebug.cpp, where the debug tools live.
//
// Blueprint: Get ReactionSubsystem -> Broadcast Event

#pragma once

#include "CoreMinimal.h"
#include "Reaction.h"
#include "ActionRunner.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "ReactiveSaveData.h"
#include "ReactionSubsystem.generated.h"

class AActor;
class UReactiveAction;
class UReactiveRule;
class UReactionComponent;
class UReactionPreset;

/**
 * Internal: one run started with Run Action, Run Actions or Run Reaction (no component): its actions,
 * its rules (Run Reaction only) and the sequence running them. The subsystem keeps it alive until
 * the run finishes. A UObject so the sequence never moves in memory.
 */
UCLASS(Transient)
class REACTIVEACTORS_API UReactionOneShot : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TObjectPtr<UReactiveAction>> Actions;

	UPROPERTY()
	TArray<TObjectPtr<UReactiveRule>> Rules;

	UPROPERTY()
	FActionRunner Sequence;

	/** What started it (Run Action, Run Actions, Run Reaction), for logs and Reactions.DumpRuns. */
	FString Label;

	/** World time it started. */
	double StartTime = -1.0;
};

/**
 * Internal: for Run Reaction Preset, one Preset reaction on one target. Its rule copies stay between runs
 * (Max Activations, Cooldown work across calls), along with the run going now (for Retrigger).
 */
USTRUCT()
struct FReactivePresetRunSlot
{
	GENERATED_BODY()

	TWeakObjectPtr<const UReactionPreset> Preset;
	TWeakObjectPtr<AActor> Target;
	int32 ReactionIndex = INDEX_NONE;

	/** The Preset reaction's Event Tag (for Reset ... For Event). */
	FGameplayTag EventTag;

	/** This target's own copies of the reaction's rules. */
	UPROPERTY()
	TArray<TObjectPtr<UReactiveRule>> Rules;

	/** The run going now, if any. */
	UPROPERTY()
	TObjectPtr<UReactionOneShot> ActiveRun;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventBroadcast, FGameplayTag, Event);

/** C++: an event was sent (after it was delivered): what, who caused it, where it came from and went. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEventSentNative, const FReactiveEventInfo& /*Info*/);

/** C++: Reset ... was called. OnlyActor: just that actor (else all). OnlyEvent: just things about that event (else all). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnReactionsResetNative, const AActor* /*OnlyActor*/, const FGameplayTag& /*OnlyEvent*/);

UCLASS()
class REACTIVEACTORS_API UReactionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Sends Event to every registered component with a reaction that could match it.
	 * Returns true if at least one component reacted (ran a reaction).
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors")
	bool BroadcastEvent(FGameplayTag Event, AActor* Instigator = nullptr);

	/**
	 * C++: Broadcast Event, also saying which actor it comes from (Source), so the debug
	 * wiring lines start there. Source defaults to Instigator. The Broadcast Event action uses this.
	 */
	bool BroadcastEventFrom(FGameplayTag Event, AActor* Instigator, const AActor* Source);

	/**
	 * Debug: draws a wiring line From -> To, labelled with Event (green = a reaction fired, red = heard
	 * but nothing fired) while Reactions.Show.WiringLines is on. Does nothing in shipping builds.
	 */
	void DrawDebugWire(const AActor* From, const AActor* To, const FGameplayTag& Event, bool bFired) const;

	/**
	 * Suspended: every event is ignored (broadcasts, and events sent straight to a component), for
	 * cutscenes, menus and scripted moments. Reactions already running carry on; cancel them too if needed.
	 * Run Action still works: it's an explicit call, not an event.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors")
	void SetEventsSuspended(bool bSuspend);

	UFUNCTION(BlueprintPure, Category = "Reactive Actors")
	bool AreEventsSuspended() const { return bSuspended; }

	/**
	 * Cancels every running reaction in the world, and every action started with Run Action.
	 * Actions still running get On Cancel; cancelled runs count as finished (Result Cancelled, rules are told).
	 * Returns how many reactions and one-off actions were cancelled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors")
	int32 CancelAllReactions();

	// C++ versions of the library's targeted calls (Blueprint: use the Reactive Actors Library nodes).
	// Cancel: running reactions are cancelled (counts as finishing). Reset: cancelled, then rules get On Reset.
	// One-off runs (Run Action...) are cancelled by both, when aimed at that actor or started with that event.
	// Each returns how many reactions / runs it affected.
	int32 CancelReactionsForActor(AActor* Actor);
	int32 CancelReactionsForIdentity(FGameplayTag Identity);
	int32 CancelReactionsForEvent(FGameplayTag Event);
	int32 ResetAllReactions();
	int32 ResetReactionsForActor(AActor* Actor);
	int32 ResetReactionsForIdentity(FGameplayTag Identity);
	int32 ResetReactionsForEvent(FGameplayTag Event);

	/**
	 * Sends Event to the Reaction Components on Actor only. Returns true if any reaction fired.
	 * Source (optional): the actor sending it, for the wiring lines and the history. Default: Instigator.
	 */
	bool BroadcastEventToActor(AActor* Actor, FGameplayTag Event, AActor* Instigator = nullptr, const AActor* Source = nullptr);

	/** Sends Event to every actor with Identity (or a child of it). Returns true if any reaction fired. Source as above. */
	bool BroadcastEventToIdentity(FGameplayTag Identity, FGameplayTag Event, AActor* Instigator = nullptr, const AActor* Source = nullptr);

	/**
	 * Runs Action once on Target, without a component (see the library's Run Action).
	 * Returns false if it didn't run (no action, no target, already running, or a requirement failed).
	 */
	bool RunAction(UReactiveAction* Action, AActor* Target, AActor* Instigator = nullptr);

	/**
	 * Runs Actions in order on Target, without a component, the way a reaction would (each waits for the one before to call Finish Action).
	 * Returns false if it didn't start (no target, no actions, or one of them is already running).
	 */
	bool RunActions(const TArray<UReactiveAction*>& Actions, AActor* Target, AActor* Instigator = nullptr, FGameplayTag Event = FGameplayTag());

	/**
	 * Runs a reaction once on Target, without a component: its rules can block it and are told about
	 * the run, then its actions run in order. Its Event Tag is the run's event (Get Event).
	 * Returns false if it didn't start (disabled, blocked by a rule, no target, no actions, or already running).
	 */
	bool RunReaction(const FReaction& Reaction, AActor* Target, AActor* Instigator = nullptr);

	/**
	 * Runs a Preset's reactions once on Target, without a component (the Run Reaction Preset node).
	 * Event: the reactions that would fire if it were sent (listening for it or a parent of it). Empty: every enabled reaction.
	 * Each run gets fresh copies of the reaction's actions. Its rules are copied once per target and kept, so
	 * Max Activations and Cooldown work across calls (Reset All Reactions clears them). Retrigger applies:
	 * Ignore skips a reaction still running on Target, Restart cancels that run first.
	 * OnRunFinished: called once for each run that started, when it ends (instant runs: before this returns).
	 * OutRuns (optional): the runs that started, to cancel later.
	 * Returns how many reactions started.
	 */
	int32 RunReactionPreset(const UReactionPreset* Preset, FGameplayTag Event, AActor* Target, AActor* Instigator,
		TFunction<void(EReactionResult)> OnRunFinished, TArray<UReactionOneShot*>* OutRuns = nullptr);

	/** Fires on every broadcast, whether or not any component reacted (for debugging usually). */
	UPROPERTY(BlueprintAssignable, Category = "Reactive Actors")
	FOnEventBroadcast OnEventBroadcast;

	/**
	 * C++: every event sent (Broadcast Event, Broadcast To Actor / Identity, triggers), after it was
	 * delivered. Not called while suspended. The one hook everything that listens for events uses:
	 * the Wait For World / Identity Event nodes, the Wait For Event action, the Event Received trigger
	 * and the history.
	 */
	FOnEventSentNative OnEventSent;

	/**
	 * C++: Reset All Reactions (or its For Actor / For Event versions) was called. Your own systems that keep
	 * state outside Reaction Components (a fact store, counters) can listen here and reset themselves.
	 */
	FOnReactionsResetNative OnReset;

	/**
	 * True if Sent counts as ListenFor: the same tag, or (unless bExactMatch) a child of it,
	 * the way reactions match (listening for Event.Door hears Event.Door.Open).
	 */
	static bool EventMatches(const FGameplayTag& Sent, const FGameplayTag& ListenFor, bool bExactMatch);

	/**
	 * True if the event was sent to Identity (Broadcast Event To Identity, Identity scope), or to a parent
	 * of it: listening as Actor.Kitchen.Lamp hears what's sent to Actor.Kitchen. Events sent to everyone
	 * or to one actor don't count. What Identity scope listening means everywhere.
	 */
	static bool EventSentToIdentity(const FReactiveEventInfo& Info, const FGameplayTag& Identity)
	{
		return Info.Route == EReactiveEventRoute::Identity && Identity.IsValid() && Info.TargetIdentity.IsValid()
			&& Identity.MatchesTag(Info.TargetIdentity);
	}

	/** History: a reaction on Actor ended (the component reports it). Debug builds only; nothing in shipping. */
	void RecordReactionFinished(const AActor* Actor, const FGameplayTag& EventTag, EReactionResult Result, const FString& FailedAt);

	/**
	 * Every loaded actor whose Reaction Component has Identity or a child of it
	 * (Actor.Kitchen gets Actor.Kitchen.Lamp and Actor.Kitchen.TV). Each actor is listed once.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors")
	TArray<AActor*> GetActorsByIdentity(FGameplayTag Identity) const;

	// ---- Save and load ---------------------------------------------------------------------------

	/**
	 * For loading a save after the level has started: triggers and Reactive Volumes queue what they'd fire until
	 * Set World Reaction State (which releases them) or Release Held Reactions, so nothing fires before the saved
	 * counts are back. Call it from the Game Mode's Begin Play when loading. Does nothing if this level already got
	 * its saved state (Set World Reaction State before Open Level).
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save", meta = (DisplayName = "Hold Reactions Until Restored"))
	void HoldUntilRestored();

	/** Fires everything Hold Reactions Until Restored queued, in order, and stops holding. Set World Reaction State calls it for you. */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors|Save", meta = (DisplayName = "Release Held Reactions"))
	void ReleaseHeld();

	UFUNCTION(BlueprintPure, Category = "Reactive Actors|Save", meta = (DisplayName = "Are Reactions Held"))
	bool IsHolding() const { return bHolding; }

	/** C++, for triggers and volumes: while holding, keeps Send to run on release and returns true. Otherwise false. */
	bool DeferIfHolding(TFunction<void()> Send);

	// Called by UReactionComponent on BeginPlay / EndPlay
	void RegisterEvent(UReactionComponent* Component, const FGameplayTagContainer& Tags);
	void UnregisterEvent(UReactionComponent* Component, const FGameplayTagContainer& Tags);
	void RegisterIdentity(UReactionComponent* Component, const FGameplayTagContainer& IdentityTags);
	void UnregisterIdentity(UReactionComponent* Component, const FGameplayTagContainer& IdentityTags);

	/** Read-only view of the one-off runs still going (Run Action and friends). Used by Reactions.DumpRuns. */
	const TArray<TObjectPtr<UReactionOneShot>>& GetOneShots() const { return OneShots; }

	/** Read-only view of the registry (event tag -> components listening for it). Used by debug commands. */
	const TMap<FGameplayTag, TArray<TWeakObjectPtr<UReactionComponent>>>& GetComponentsByEvent() const { return ComponentsByEvent; }

	// Tickable: cancels orphaned one-off runs; draws the overlay and history feed while they are on (never in shipping builds)
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UReactionSubsystem, STATGROUP_Tickables); }

	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Event tag -> components listening for that event. */
	TMap<FGameplayTag, TArray<TWeakObjectPtr<UReactionComponent>>> ComponentsByEvent;

	/** Identity tag (and each of its parents) -> components with that identity. */
	TMap<FGameplayTag, TArray<TWeakObjectPtr<UReactionComponent>>> ComponentsByIdentity;

	/**
	 * Cancels one-off runs whose target has been destroyed or streamed out (they can't do anything useful,
	 * and a stuck one would hold its actions, and the Outer they were made with, until the level ends).
	 * Runs every tick, in every build.
	 */
	void CancelOrphanedOneShots();

	/**
	 * An event went out: tells OnEventBroadcast and OnEventSent and adds it to the history.
	 * Receivers: each actor it reached, and whether a reaction fired there.
	 */
	void NotifyEventSent(const FReactiveEventInfo& Info, const TArray<TPair<FString, bool>>& Receivers);

#if !UE_BUILD_SHIPPING
	/** One line of the event history (Reactions.History, Reactions.Show.History). */
	struct FHistoryLine
	{
		double Time = 0.0;
		FString Text;
		FColor Color = FColor::White;
	};

	void AddHistory(FString Text, FColor Color);

	/** History line for an event that was sent (ReactionSubsystemDebug.cpp). */
	void RecordEventHistory(const FReactiveEventInfo& Info, const TArray<TPair<FString, bool>>& Receivers);

	/** Overlay and on-screen history, every frame while switched on (ReactionSubsystemDebug.cpp). */
	void TickDebug();

	TArray<FHistoryLine> History;

public:
	/** The history, oldest first (debug builds only). */
	const TArray<FHistoryLine>& GetHistory() const { return History; }

private:
#endif

	/** Every registered component (event listeners and identities), as a snapshot. */
	TArray<UReactionComponent*> GetAllComponents() const;

	/** Cancels the running one-off runs ShouldCancel picks. Returns how many. */
	int32 CancelOneShots(TFunctionRef<bool(const UReactionOneShot&)> ShouldCancel);

	/**
	 * Shared by Run Action, Run Actions, Run Reaction and Run Reaction Preset. Label names the caller in warnings.
	 * OnFinished (optional) is called when the run ends, after rules are told and actions cleaned up.
	 * Returns the run, or null if it didn't start. Instant runs have already finished when this returns.
	 */
	UReactionOneShot* StartOneShot(const TArray<TObjectPtr<UReactiveAction>>& Actions, const TArray<TObjectPtr<UReactiveRule>>& Rules,
		const FReactionContext& Context, const TCHAR* Label, TFunction<void(EReactionResult)> OnFinished = nullptr);

	/**
	 * Index of Run Reaction Preset's slot for this preset reaction and target, made (with rule copies) if needed.
	 * An index, not a reference: running actions can add slots (and move the array).
	 */
	int32 FindOrAddPresetSlot(const UReactionPreset* Preset, int32 ReactionIndex, AActor* Target);

	/** Rules get On Reset in the Run Reaction Preset slots ShouldReset picks. Returns how many slots. */
	int32 ResetPresetSlots(TFunctionRef<bool(const FReactivePresetRunSlot&)> ShouldReset);

	/** Run Reaction Preset slots per preset reaction and target (rule state kept between runs). */
	UPROPERTY(Transient)
	TArray<FReactivePresetRunSlot> PresetSlots;

	/** Runs started with Run Action, Run Actions or Run Reaction that are still going. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UReactionOneShot>> OneShots;

	bool bSuspended = false;

	/** Hold Reactions Until Restored: on, and what's waiting to fire. */
	bool bHolding = false;
	TArray<TFunction<void()>> HeldSends;
	FTimerHandle HoldSafetyHandle;
};

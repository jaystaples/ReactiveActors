// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Run Reaction Preset: a Blueprint node that runs a Preset's reactions once on a target, with no
// Reaction Component, and tells you when they end. Usable in any event graph: actors, widgets, managers (not inside functions, like any async node).
//
//   Then          straight away, so the graph carries on.
//   On Finished   when every reaction it started has ended. Result: Failed if any failed, else Cancelled
//                 if any was cancelled, else Succeeded.
//   On Blocked    nothing started: no reaction for that event, a rule stopped it (Max Activations,
//                 Cooldown), it's still running with Retrigger Ignore, or no target.
//
// Rules keep their state per target between runs (Reset All Reactions clears it). Keep the Async Action
// pin to Cancel the runs it started (no pins fire after a Cancel).

#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "GameplayTagContainer.h"
#include "ReactionContext.h"
#include "ReactiveRunReactionPreset.generated.h"

class AActor;
class UReactionOneShot;
class UReactionPreset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReactiveRunResult, EReactionResult, Result);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class REACTIVEACTORS_API UReactiveRunReactionPreset : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	/**
	 * Runs Preset's reactions once on Context's Target, without a Reaction Component.
	 * Context: Target (required: what the actions act on), Instigator (who caused it, optional) and Event
	 * (optional: run the reactions that would fire if it were sent; empty runs every enabled reaction).
	 * Then fires straight away; On Finished when they've all ended; On Blocked if nothing started.
	 * Rules (Max Activations, Cooldown) keep their state per target between runs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Actors",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Run Reaction Preset",
			Keywords = "run reaction preset actions reactive", UnsafeDuringActorConstruction = "true"))
	static UReactiveRunReactionPreset* RunReactionPreset(UObject* WorldContextObject, UReactionPreset* Preset,
		const FReactionContext& Context);

	/** Every reaction it started has ended. */
	UPROPERTY(BlueprintAssignable)
	FReactiveRunResult OnFinished;

	/** Nothing started (Result is Failed). */
	UPROPERTY(BlueprintAssignable)
	FReactiveRunResult OnBlocked;

	virtual void Activate() override;
	virtual void Cancel() override;

private:
	void HandleRunFinished(EReactionResult Result);

	/** Broadcasts once and lets the node go. Deferred a frame when it happens inside Activate, so Then fires first. */
	void End(bool bBlocked);

	UPROPERTY()
	TObjectPtr<UReactionPreset> Preset;

	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AActor> RunInstigator;
	FGameplayTag Event;

	/** The runs it started, for Cancel. */
	TArray<TWeakObjectPtr<UReactionOneShot>> Runs;

	/** Started runs still going. Goes below zero while starting if instant runs finish straight away. */
	int32 PendingRuns = 0;

	EReactionResult CombinedResult = EReactionResult::Succeeded;
	/** Inside the subsystem's start call: finishes are counted, but the end waits until starting is done. */
	bool bStarting = false;

	/** Inside Activate: an end here is deferred a frame so Then fires first. */
	bool bActivating = false;

	bool bEnded = false;
};

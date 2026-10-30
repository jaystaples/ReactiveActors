// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// A Reactive Rule is a rule with memory, attached to something that runs and repeats:
// a reaction (one run = its actions, start to finish) or a trigger (one run = one fire, which
// starts and finishes in the same instant). It can block a run, and hears when runs start and
// finish (count activations, enforce a cooldown...). Each reaction and trigger gets its own
// rule objects, so rules keep their own state.
//
// Requirement or rule? If the check only needs the situation right now (who, what, the target),
// it's a requirement. If it needs to remember what happened before, it's a rule.
//
// Subclass in Blueprint and override the events you need. A rule picks the moment it cares about
// (On Start or On Finish) and works the same on reactions and triggers.

#pragma once

#include "CoreMinimal.h"
#include "ReactiveObjectBase.h"
#include "ReactionContext.h"
#include "ReactiveRule.generated.h"

/** A rule with memory on a reaction or trigger (like Cooldown): can block a run, and hears when runs start and finish. */
UCLASS(Abstract, Blueprintable, BlueprintType, meta = (DisplayName = "Reactive Rule"))
class REACTIVEACTORS_API UReactiveRule : public UReactiveObjectBase
{
	GENERATED_BODY()

public:
	/**
	 * Return true to stop this run. Reaction: checked every time its event arrives.
	 * Trigger: checked every time its condition is met, before it fires.
	 * Context: Target is the owning actor, Instigator who caused it (may be empty), Event the tag involved.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Rule", meta = (DisplayName = "Should Block"))
	bool ShouldBlock(const FReactionContext& Context) const;
	virtual bool ShouldBlock_Implementation(const FReactionContext& Context) const { return false; }

	/** A run starts. Reaction: before its first action. Trigger: as it fires. */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Rule", meta = (DisplayName = "On Start"))
	void OnStart(const FReactionContext& Context);
	virtual void OnStart_Implementation(const FReactionContext& Context) {}

	/**
	 * A run finished. Reaction: after its last action has called Finish Action. Trigger: straight after
	 * On Start, since a fire takes no time (always Succeeded). Context is the same as that run's On Start.
	 * Result: Succeeded (all actions succeeded, or a Stop Reaction ended it as a success), Failed (an
	 * action finished with Success off, or a Stop Reaction ended it as a failure), or Cancelled (stopped
	 * from outside: Cancel..., Reset, a Restart, its target gone).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Rule", meta = (DisplayName = "On Finish"))
	void OnFinish(const FReactionContext& Context, EReactionResult Result);
	virtual void OnFinish_Implementation(const FReactionContext& Context, EReactionResult Result) {}

	/**
	 * Short live status for the debug overlay (Reactions.Show), e.g. "2/3" or "1.4s left".
	 * Return an empty string to show nothing. Called every frame while the overlay is on: keep it read-only.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Rule", meta = (DisplayName = "Get Debug String"))
	FString GetDebugString() const;
	virtual FString GetDebugString_Implementation() const { return FString(); }

	/**
	 * Called by Reset All Reactions (and its For Actor / Identity / Event versions): clear any state
	 * this rule keeps, as if it had never run (counters back to 0, timers cleared).
	 * Used for checkpoints and New Game.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Rule", meta = (DisplayName = "On Reset"))
	void OnReset();
	virtual void OnReset_Implementation() {}
};

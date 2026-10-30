// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in rules
//
// Each reaction and trigger gets its own copy of these, so their state is its own.
// Runtime state is Transient (never baked into a level) and SaveGame (kept by Capture / Restore Reaction State).

#pragma once

#include "CoreMinimal.h"
#include "ReactiveRule.h"
#include "BuiltInRules.generated.h"

/** Blocks once it has run Max Activations times (a reaction firing, or a trigger firing). */
UCLASS(NotBlueprintable, meta = (DisplayName = "Max Activations"))
class REACTIVEACTORS_API URULE_MaxActivations : public UReactiveRule
{
	GENERATED_BODY()

public:
	virtual bool ShouldBlock_Implementation(const FReactionContext& Context) const override;
	virtual void OnStart_Implementation(const FReactionContext& Context) override;
	virtual FString GetDebugString_Implementation() const override;
	virtual void OnReset_Implementation() override;
	virtual FString BuildSummary() const override;

protected:
	/** How many times it may run. 0 or less = unlimited. */
	UPROPERTY(EditAnywhere, Category = "Rule")
	int32 MaxActivations = 1;

	/** Times it has run so far (visible on placed actors while playing). Saved with Capture Reaction State. */
	UPROPERTY(VisibleInstanceOnly, Transient, SaveGame, Category = "Rule")
	int32 ActivationCount = 0;
};

/** Blocks for a number of seconds after each run finishes (for a trigger, after each fire). */
UCLASS(NotBlueprintable, meta = (DisplayName = "Cooldown"))
class REACTIVEACTORS_API URULE_Cooldown : public UReactiveRule
{
	GENERATED_BODY()

public:
	virtual bool ShouldBlock_Implementation(const FReactionContext& Context) const override;
	virtual void OnFinish_Implementation(const FReactionContext& Context, EReactionResult Result) override;
	virtual FString GetDebugString_Implementation() const override;
	virtual void OnReset_Implementation() override;
	virtual FString BuildSummary() const override;
	virtual void PreSaveState() override;
	virtual void PostLoadState() override;

protected:
	/** Seconds after a run finishes before it can run again. */
	UPROPERTY(EditAnywhere, Category = "Rule", meta = (ClampMin = 0, UIMin = 0, Units = "s"))
	float CooldownSeconds = 1.f;

private:
	/** Seconds left when saved (world time starts again after a load, so the finish time itself can't be). -1: never ran. */
	UPROPERTY(Transient, SaveGame)
	float SavedSecondsLeft = -1.f;

	double LastFinishTime = -1.0;
};

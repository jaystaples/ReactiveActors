// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// A Reactive Requirement is a reusable check an action must pass before it runs on its
// target, e.g. "target implements BPI_Light" or "target has a Light component".
// Check Requirement gets the whole context, untouched: Target, Instigator and Event, so a check can
// use any of them ("instigator is within 5 m of the target"). The built-in ones have a Check setting
// to pick the target or the instigator; add your own if your requirement needs one.
// Authored inline in an action's Requirements array (in the action Blueprint's Class Defaults)
// or a Branch's Conditions. Subclass in Blueprint and override Check Requirement.
// Use Extra Requirements to add a requirement to a single action instance (not the class).
// Every requirement has a Not tick box: on, it passes when its check fails ("Not Is Player").

#pragma once

#include "CoreMinimal.h"
#include "ReactiveObjectBase.h"
#include "ReactionContext.h"
#include "ReactiveRequirement.generated.h"

/** Which actor a requirement looks at (the built-in requirements' Check setting; use it in your own too). */
UENUM(BlueprintType)
enum class EReactiveRequirementSubject : uint8
{
	/** The actor being acted on (for a trigger's conditions: the trigger's own actor). */
	Target,
	/** Who caused it (the overlapping pawn...). Fails if there's no instigator. */
	Instigator,
};

/** A check about the moment (who, what, the target's state). Override Check Requirement. */
UCLASS(Abstract, Blueprintable, BlueprintType, meta = (DisplayName = "Reactive Requirement"))
class REACTIVEACTORS_API UReactiveRequirement : public UReactiveObjectBase
{
	GENERATED_BODY()

public:
	/** Runs the check, flipped if Not is on, and fills in a default fail reason if none was given. */
	bool Evaluate(const FReactionContext& Context, FText& OutFailReason) const;

	/** "Not " in front of the description when Not is on. Override DescribeRequirement, not this. */
	virtual FString BuildSummary() const override;

	/** Pass when the check fails instead: "Not Is Player", "Not Has Actor Tags: Lit". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (DisplayName = "Not", DisplayPriority = 0))
	bool bInvert = false;

	/**
	 * C++: true if this requirement looks at the instigator, so Map Check (which has no instigator)
	 * doesn't test it ahead of time. The built-ins return true when Check is Instigator.
	 */
	virtual bool ChecksInstigator() const { return false; }

	/**
	 * C++: false if Map Check shouldn't test this ahead of time: it looks at the instigator, or its answer
	 * changes every time (Random Chance).
	 */
	virtual bool CanCheckAhead() const { return !ChecksInstigator(); }

protected:
	/** C++: the summary without "Not" ("Has Actor Tags: Lit"). The default is the class's display name. */
	virtual FString DescribeRequirement() const;

	/**
	 * Return true if the requirement is met. Not is applied for you: always write the plain check. Context: Target (the actor being acted on), Instigator
	 * (who caused it, may be empty) and Event, exactly as the run has them.
	 * On failure, set Fail Reason to something a designer can act on ("Actor has no light component").
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Requirement", meta = (DisplayName = "Check Requirement"))
	bool CheckRequirement(const FReactionContext& Context, FText& FailReason) const;
	virtual bool CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const { return true; }
};

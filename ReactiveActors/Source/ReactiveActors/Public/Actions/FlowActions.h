// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions
//
// Flow: actions that shape the chain itself. Branch, Delay, Wait For Event, Stop Reaction, Random Action, Run On Targets.
// NotBlueprintable keeps them out of the "pick parent class" list; they still appear in every Actions dropdown.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "ActionRunner.h"
#include "ReactiveAction.h"
#include "FlowActions.generated.h"

class AActor;
class UReactionSubsystem;
class UReactiveRequirement;

// ---- Branch -------------------------------------------------------------------------------------

/** How a Branch combines its Conditions. */
UENUM(BlueprintType)
enum class EReactiveBranchMode : uint8
{
	/** True only if every condition passes (no conditions: true). */
	AllConditions UMETA(DisplayName = "All Conditions"),
	/** True if at least one condition passes (no conditions: true). */
	AnyCondition  UMETA(DisplayName = "Any Condition"),
};

/**
 * Checks its Conditions, then runs its True or False actions in its place. It finishes when the
 * chosen actions do; if one of them fails, the branch fails too, and so does the reaction around it.
 * To end the reaction after one side, put a Stop Reaction at the end of that side's list.
 * For custom logic, make a Reactive Requirement and add it to Conditions. Branches can nest.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Branch"))
class REACTIVEACTORS_API UACT_Branch : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

public:
	UACT_Branch();

	virtual void Execute(const FReactionContext& InContext) override;
	virtual void Cleanup() override;
	virtual void Abort() override;
	virtual void GetNestedActions(TArray<UReactiveAction*>& OutActions) const override;
	virtual const FActionRunner* GetActiveInnerSequence() const override { return InnerSequence.IsRunning() ? &InnerSequence : nullptr; }

	/** C++: which way the last run went. */
	bool GetLastResult() const { return bLastResult; }

protected:
	/** The branch finishes itself, when its chosen actions do. */
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override {}
	virtual FString DescribeAction() const override;

	/** Checked when the branch runs, with the reaction's context. No conditions = true. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Branch", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveRequirement>> Conditions;

	/** All Conditions: true only if every condition passes. Any Condition: true if one passes. */
	UPROPERTY(EditAnywhere, Category = "Branch")
	EReactiveBranchMode EvaluateMode = EReactiveBranchMode::AllConditions;

	/** Run these actions if the condition is true. End with Stop Reaction to end the reaction here. */
	UPROPERTY(EditAnywhere, Instanced, Category = "True Actions", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveAction>> TrueActions;

	/** Run these actions if the condition is false. End with Stop Reaction to end the reaction here. */
	UPROPERTY(EditAnywhere, Instanced, Category = "False Actions", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveAction>> FalseActions;

private:
	/** The Conditions, all or any (per Evaluate Mode). */
	bool EvaluateConditions(const FReactionContext& InContext) const;

	UPROPERTY(Transient)
	FActionRunner InnerSequence;

	bool bLastResult = false;
};

// ---- Delay --------------------------------------------------------------------------------------

/** Waits Duration seconds, then finishes: the reaction's next action runs after the wait. */
UCLASS(NotBlueprintable, meta = (DisplayName = "Delay"))
class REACTIVEACTORS_API UACT_Delay : public UReactiveAction
{
	GENERATED_BODY()

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;
	virtual void OnCleanup_Implementation() override;
	virtual void OnCancel_Implementation() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0, UIMin = 0, Units = "s"))
	float Duration = 1.f;

private:
	void HandleDelayFinished();

	FTimerHandle TimerHandle;
};

// ---- Wait For Event -----------------------------------------------------------------------------

/**
 * Waits until an event is sent, then finishes: the reaction carries on from there.
 *
 * For short, local moments: "close the door, wait for Event.Door.Closed, then play the slam".
 * NOT for quest steps or anything that should survive a save: a reaction that's waiting isn't saved, and
 * after loading it's simply gone. Keep progress in data (your own facts or variables, checked by requirements)
 * and give each step its own reaction instead.
 *
 * Waits as long as it takes, unless Timeout is set. Uses the same hook as the Wait For World / Identity
 * Event nodes and the Event Received trigger.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Wait For Event"))
class REACTIVEACTORS_API UACT_WaitForEvent : public UReactiveAction
{
	GENERATED_BODY()

public:
	UACT_WaitForEvent();

	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;
	virtual void OnCancel_Implementation() override;
	virtual void OnCleanup_Implementation() override;

	/** The event to wait for (or a child of it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ReactiveTags = "Event"))
	FGameplayTag Event;

	/** Only this exact tag, not its children. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bExactMatch = false;

	/**
	 * World: the event however it's sent (to everyone, to an actor, to an identity).
	 * Identity: only when it's sent to Listen Identity, or to a parent of it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, InvalidEnumValues = "Self"))
	EReactiveScopePolicy ListenScope = EReactiveScopePolicy::World;

	/** Give up after this many seconds. 0: no timeout, wait as long as it takes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0, UIMin = 0, Units = "s"))
	float Timeout = 0.f;

	/** Timeout: on, the reaction fails (the actions after this don't run). Off, it carries on as if the event came. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, EditCondition = "Timeout > 0", EditConditionHides))
	bool bFailOnTimeout = true;

	/** Identity scope: the identity the event must be sent to (or a child of what it's sent to). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action",
		meta = (ExposeOnSpawn, ReactiveTags = "Identity", EditCondition = "ListenScope == EReactiveScopePolicy::Identity", EditConditionHides))
	FGameplayTag ListenIdentity;

private:
	void HandleEvent(const FReactiveEventInfo& Info);
	void HandleTimeout();
	void StopWaiting();

	TWeakObjectPtr<class UReactionSubsystem> ListeningTo;
	FDelegateHandle EventHandle;
	FTimerHandle TimeoutHandle;
};

// ---- Stop Reaction ------------------------------------------------------------------------------

/**
 * Ends the reaction here: the actions after this one don't run. Use it at the end of a Branch's True
 * or False list to end the reaction on that side. Result: Succeeded ends it as a success (rules see a
 * normal finish), Failed ends it as a failure, like an action finishing with Success off.
 * Inside Run On Targets it ends only that target's actions.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Stop Reaction"))
class REACTIVEACTORS_API UACT_StopReaction : public UReactiveAction
{
	GENERATED_BODY()

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	/** How the reaction ends: Succeeded or Failed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, InvalidEnumValues = "Cancelled"))
	EReactionResult Result = EReactionResult::Succeeded;
};

// ---- Random Action ------------------------------------------------------------------------------

/** One of a Random Action's options: its actions, and how likely it is to be picked. */
USTRUCT(BlueprintType)
struct REACTIVEACTORS_API FReactiveRandomOption
{
	GENERATED_BODY()

	FReactiveRandomOption() : Weight(1.f) {}

	/** How likely this option is, against the others' weights (2 is twice as likely as 1). 0 = never. */
	UPROPERTY(EditAnywhere, Category = "Option", meta = (ClampMin = 0, UIMin = 0))
	float Weight = 1.f;

	/** The actions run when this option is picked. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Option", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveAction>> Actions;
};

/**
 * Picks one of its Options at random (by Weight) and runs its actions in its place. It finishes when
 * they do; if one of them fails, the Random Action fails too. Use Extra Requirements to gate the whole thing.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Random Action"))
class REACTIVEACTORS_API UACT_RandomAction : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

public:
	UACT_RandomAction();

	virtual void Execute(const FReactionContext& InContext) override;
	virtual void Cleanup() override;
	virtual void Abort() override;
	virtual void ResetAction() override;
	virtual void GetNestedActions(TArray<UReactiveAction*>& OutActions) const override;
	virtual const FActionRunner* GetActiveInnerSequence() const override { return InnerSequence.IsRunning() ? &InnerSequence : nullptr; }

#if WITH_EDITOR
	/** A new option starts at Weight 1 (the Details panel can add it zeroed). */
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

protected:
	/** It finishes itself, when the picked option's actions do. */
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override {}
	virtual FString DescribeAction() const override;

	/** The options to pick from. */
	UPROPERTY(EditAnywhere, Category = "Random", meta = (TitleProperty = "Weight {Weight}"))
	TArray<FReactiveRandomOption> Options;

	/** Never pick the same option twice in a row (when there's more than one to pick from). */
	UPROPERTY(EditAnywhere, Category = "Random")
	bool bAvoidRepeat = false;

private:
	/** A weighted pick, or INDEX_NONE if nothing can be picked. */
	int32 PickOption() const;

	UPROPERTY(Transient)
	FActionRunner InnerSequence;

	/** Avoid Repeat: the option picked last time. Saved, so it still avoids it after a load. */
	UPROPERTY(Transient, SaveGame)
	int32 LastPicked = INDEX_NONE;
};

// ---- Run On Targets -----------------------------------------------------------------------------

/** Internal: one target's run inside Run On Targets: its own copies of the actions and its own runner. */
UCLASS(Transient, NotBlueprintable)
class REACTIVEACTORS_API UReactiveTargetRun : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<AActor> Target;

	/** This target's own copies of Run On Targets' actions (they keep their own state, per target). */
	UPROPERTY()
	TArray<TObjectPtr<UReactiveAction>> Actions;

	UPROPERTY()
	FActionRunner Runner;
};

/**
 * Runs its actions on every actor whose Reaction Component has Target Identity (or a child tag),
 * all at once: each target gets its own copy of the actions. Finishes when every target's actions
 * are done. A failure (or Stop Reaction) only ends that target's actions; the others carry on, and
 * Run On Targets itself still succeeds. Only finds actors that are loaded and playing.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Run On Targets"))
class REACTIVEACTORS_API UACT_RunOnTargets : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

public:
	UACT_RunOnTargets();

	virtual void Execute(const FReactionContext& InContext) override;
	virtual void Cleanup() override;
	virtual void Abort() override;
	virtual void ResetAction() override;
	virtual void GetNestedActions(TArray<UReactiveAction*>& OutActions) const override;
	virtual bool NestedActionsKeepTarget() const override { return false; }
	virtual const FActionRunner* GetActiveInnerSequence() const override;

protected:
	/** Finishes itself when every target is done. */
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override {}
	virtual FString DescribeAction() const override;

	/** Identity tag to look for, e.g. Actor.Kitchen.Lamp (Actor.Kitchen matches every kitchen actor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ReactiveTags = "Identity"))
	FGameplayTag TargetIdentity;

	/** Skip this action's own target if it also has the identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bExcludeSelf = true;

	/** Run on each found actor (that actor becomes the target). Each target runs its own copy. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Action", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveAction>> Actions;

private:
	/** The run for Target: reused if that target ran before (its copies keep their state), else new. */
	UReactiveTargetRun* GetOrMakeRun(AActor* Target);

	void HandleTargetComplete();

	/** Finishes this action once every target's run is done. */
	void FinishIfDone();

	/** One per target this action has run on. Runs for destroyed targets are dropped on the next run. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UReactiveTargetRun>> TargetRuns;

	int32 PendingRuns = 0;
	bool bStartingRuns = false;
};

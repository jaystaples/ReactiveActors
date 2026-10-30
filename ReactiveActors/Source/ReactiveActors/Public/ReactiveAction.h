// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
// 
// Abstract shell for a single, composable piece of logic.
//
// Actions know nothing about what triggers them. They are authored inline in an
// array (add element -> pick class from dropdown -> fill in variables underneath),
// and whatever runs that array (an Action Runner) calls Load, Execute and Cleanup.
//
// Subclass in Blueprint (preferred) or C++ and override Execute Action.
//
// Every action ends by calling Finish Action: straight away for something instant, or later for work
// over time (a sequence, a fade, waiting for the player). Nothing runs after it until it does.
// Finish Action (Success off) fails the reaction: the rest of its actions are skipped, and the
// Branch or reaction around it fails too. Compiling a Blueprint action whose Execute Action never
// calls Finish Action gives a warning.
//
// One action, one actor: an action affects its Target (Context Target on Execute Action) and nothing else.
// To affect several actors, use Run On Targets, or give each actor its own reaction for the same
// event, so every actor describes its own reaction. Routing and world actions (Broadcast Event,
// Run On Targets, Set Data Layer State, Delay, Stop Reaction) are the exceptions: they pass the
// event on or work on the world, not on a target.
//
// Run actions at Begin Play or later, never from a construction script. Construction scripts run in the
// editor every time an actor is moved or edited, run before Begin Play in the game, and aren't run at all
// for placed actors in a packaged game (their result was saved in the editor). Actions there would either
// do nothing (no subsystem, nothing has begun play) or change the actor in ways that get saved into the level.
// Run Action and friends refuse to start on a target that hasn't begun play.

#pragma once

#include "CoreMinimal.h"
#include "ReactiveObjectBase.h"
#include "ReactionContext.h"
#include "ReactiveAction.generated.h"

class UReactiveAction;
class UReactiveRequirement;
struct FActionRunner;

/** Native: fired when a running action finishes (the runner uses this to continue, or fail). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActionFinished, UReactiveAction*);

/** One thing done to one actor. Override Execute Action, and call Finish Action when it's done. */
UCLASS(Abstract, Blueprintable, BlueprintType,
	meta = (DisplayName = "Reactive Action", PrioritizeCategories = "Reactive Action"))
class REACTIVEACTORS_API UReactiveAction : public UReactiveObjectBase
{
	GENERATED_BODY()

public:
	// ---- Lifecycle (C++ only, driven by the runner running this action) ---------------------------

	/** Prepares the action (and any nested actions). Only loads once. */
	virtual void Load();

	/** Runs the action with Context. Does nothing if disabled or a requirement fails on the target. */
	virtual void Execute(const FReactionContext& InContext);

	/** Tears the action (and any nested actions) down. */
	virtual void Cleanup();

	/**
	 * C++: stops this action mid-run because its reaction was cancelled. Runs On Cancel so it can stop
	 * what it's doing (timers, sequences...). The action counts as finished, but no Finish Action is sent:
	 * whatever cancelled it handles what comes next. Does nothing if the action isn't running.
	 */
	virtual void Abort();

	/**
	* C++: resets this action and every action nested inside it (called by Reset All Reactions and friends).
	* Runs On Reset on each. Called after any running reaction has been cancelled.
	*/
	virtual void ResetAction();

	/**
	 * Every action must call this when it's done, now or later: the next action waits until it does.
	 * Success off: the action failed. The rest of the reaction is skipped and it ends as Failed
	 * (Branches and the reaction around this action fail too; inside Run On Targets, only that target's actions).
	 */
	UFUNCTION(BlueprintCallable, Category = "Reactive Action")
	void FinishAction(bool bSuccess = true);

	/** Fires when Finish Action is called on a running action. */
	FOnActionFinished OnFinished;

	/** True between Execute and Finish Action. */
	bool IsRunning() const { return bRunning; }

	/** True unless the last run finished with Success off. Skipped actions count as succeeded. */
	bool DidSucceed() const { return bLastSucceeded; }

	/** True if this action asked the runner to drop the actions after it (Stop Reaction, or a failure). */
	bool RequestedSequenceStop() const { return bStopSequence; }

	/** True if the last Execute actually ran the action (false if it was disabled or a requirement failed).
	 *  A requirement failing with Stop Reaction If Not Met on also fails the action (Did Succeed false). */
	bool DidLastExecuteRun() const { return bLastExecuteRan; }

	/** C++: true if every requirement (class and extra) passes for InContext. Used by Execute. */
	bool PassesRequirements(const FReactionContext& InContext, FText& OutReason) const;

	/** True if this action's requirements pass right now, checked against its target. */
	UFUNCTION(BlueprintCallable, Category = "Reactive Action")
	bool CanRun() const;

	/** The requirements authored on this action's Blueprint class (always read from Class Defaults). */
	const TArray<TObjectPtr<UReactiveRequirement>>& GetRequirements() const;

	/** C++ only: actions nested inside this one (Branch, Run On Targets), for loading, cleanup and Map Check. */
	virtual void GetNestedActions(TArray<UReactiveAction*>& OutActions) const {}

	/** C++ only: false if nested actions run on other targets (so Map Check can't check them against this actor). */
	virtual bool NestedActionsKeepTarget() const { return true; }

	/** C++ only: the runner running inside this action right now (Branch, Run On Targets). For the debug overlay. */
	virtual const FActionRunner* GetActiveInnerSequence() const { return nullptr; }

	// Debug: when this action was last skipped because a requirement failed, and why (Reactions.Show overlay)
	double GetLastSkippedTime() const { return LastSkippedTime; }
	const FString& GetLastSkippedReason() const { return LastSkippedReason; }

	// Plain C++ getters for the owning component. Blueprint subclasses read the variables directly.
	bool IsEnabled() const { return bEnabled; }
	bool IsPersistent() const { return bPersistent; }

	/** True if a failed requirement stops the whole reaction (Stop Reaction If Not Met) rather than skipping. */
	bool StopsReactionIfNotMet() const { return bStopReactionIfNotMet && bAllowExtraRequirements; }

	/** This use's own Extra Requirements (empty when the class doesn't offer them). */
	const TArray<TObjectPtr<UReactiveRequirement>>& GetExtraRequirements() const { return ExtraRequirements; }

	/** False if this class can't be used with the Run Action node (Available in Blueprint Run Action, in Class Defaults). */
	bool IsAvailableInBlueprintRunAction() const { return bAvailableInBlueprintRunAction; }

	// ---- Context (Blueprint) -----------------------------------------------------------------------

	/** The actor this action acts on. Defaults to the owning actor. */
	UFUNCTION(BlueprintPure, Category = "Reactive Action")
	AActor* GetTarget() const;

	/** Who caused this run (the player, a camera...). May be null. */
	UFUNCTION(BlueprintPure, Category = "Reactive Action")
	AActor* GetInstigator() const;

	/** The event that started this run. May be empty. */
	UFUNCTION(BlueprintPure, Category = "Reactive Action")
	FGameplayTag GetEvent() const { return RunContext.Event; }

	/**
	 * The full context this action is running with. Execute Action gets it as a pin too: use this
	 * where the pin can't reach (after a Delay, in a timer or a bound event).
	 */
	UFUNCTION(BlueprintPure, Category = "Reactive Action")
	FReactionContext GetContext() const { return RunContext; }

	/** World from the outer chain, or from the target when hosted outside an actor. */
	virtual UWorld* GetWorld() const override;

	// ---- Designer settings (shown on every action) ---------------------------------------------
	virtual void PostInitProperties() override;

	/** DescribeAction, plus "(off)" when disabled and its Extra Requirements. */
	virtual FString BuildSummary() const override;

protected:
	// ---- Overridable events --------------------------------------------------------------------

	/** Load or fetch any data this action needs. Called once, before the first Execute. */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Action", meta = (DisplayName = "Load"))
	void OnLoad();
	virtual void OnLoad_Implementation() {}

	/**
	 * Do the thing, to Context's Target: the one actor this action affects (the same as Get Target).
	 * Target is never null here: an action with no target is skipped before this is called.
	 * Context also has the Instigator (who caused it) and the Event (why).
	 * Call Finish Action when done, here or later. Not overridden: the action finishes straight away.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Action", meta = (DisplayName = "Execute Action"))
	void OnExecuteAction(const FReactionContext& Context);
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) { FinishAction(); }

	/** Unload data and remove any bindings made in Load or Execute Action. Called when the action is torn down (its actor ends play, or a one-off run ends). */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Action", meta = (DisplayName = "Cleanup"))
	void OnCleanup();
	virtual void OnCleanup_Implementation() {}

	/**
	 * The reaction was cancelled while this action was running (Cancel All Reactions...).
	 * Stop whatever it's doing (clear timers, stop sequences) and reset variables, so it's ready for its next run.
	 * It stays loaded. Don't call Finish Action.
	 * Only actions still waiting to call Finish Action get this, so instant actions can ignore it.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Action", meta = (DisplayName = "On Cancel"))
	void OnCancel();
	virtual void OnCancel_Implementation() {}

	/**
	* Reset All Reactions (or a For Actor / Identity / Event version) was called: clear any state this
	* action keeps between runs (counters, "already done" flags), as if it had never run.
	* Not called on Cancel: a cancel only stops the current run.
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Action", meta = (DisplayName = "On Reset"))
	void OnReset();
	virtual void OnReset_Implementation() {}

	/** C++: what this action does, for its summary ("Delay 2.0s"). Default: the class's display name. */
	virtual FString DescribeAction() const;

	/**
	 * C++: finish. bSuccess false fails the chain (and always stops it). bStopAfter with success:
	 * end the chain early as a success (Stop Reaction).
	 */
	void FinishActionInternal(bool bSuccess, bool bStopAfter);

	/** A second Finish Action in one run: an ensure for C++ actions, a warning for Blueprint ones (once per class). */
	void ReportFinishedTwice() const;

	// ---- Designer settings (shown on every action) ---------------------------------------------

	/** Disabled actions never load or execute. Lets you switch an action off without deleting it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Action")
	bool bEnabled = true;

	/**
	 * Reserved for save and load support, which is still being worked on: hidden from the Details panel
	 * until it does something. Kept as a property so existing saved values still load.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Reactive Action")
	bool bPersistent = true;

	/** Always checked for this action type. Set in the action Blueprint's Class Defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Reactive Action",
		meta = (DisplayName = "Class Requirements", TitleProperty = "Summary", EditCondition = "bIsClassDefaults", HideEditConditionToggle))
	TArray<TObjectPtr<UReactiveRequirement>> Requirements;

	/** Extra checks for this use of the action only. If any fails, the action is skipped (or, with Stop Reaction If Not Met, the reaction stops). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Reactive Action",
		meta = (DisplayName = "Extra Requirements", TitleProperty = "Summary", EditCondition = "!bIsClassDefaults && bAllowExtraRequirements", EditConditionHides, HideEditConditionToggle))
	TArray<TObjectPtr<UReactiveRequirement>> ExtraRequirements;

	/**
	 * If a requirement fails (Class or Extra), stop the whole reaction here instead of skipping this action.
	 * The reaction ends as Failed, like a Finish Action with Success off. Off: the action is skipped and the
	 * reaction carries on. Set it in Class Defaults for an action where nothing after it makes sense if it
	 * can't run; each placed action can change it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Action",
		meta = (DisplayName = "Stop Reaction If Not Met", EditCondition = "bAllowExtraRequirements", EditConditionHides, HideEditConditionToggle))
	bool bStopReactionIfNotMet = false;

	/** C++ subclasses: set false in the constructor to hide (and ignore) Extra Requirements (Branch does). */
	UPROPERTY(Transient)
	bool bAllowExtraRequirements = true;

	/**
	 * Can be used with the Run Action node. Turn off for actions that only make sense inside a reaction, such as
	 * ones holding lists of other actions that a node's pins can't fill in (Branch, Random Action, Run On Targets).
	 * The Run Action node then refuses the class with a compile error.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AdvancedDisplay, Category = "Reactive Action", meta = (DisplayName = "Available in Blueprint Run Action"))
	bool bAvailableInBlueprintRunAction = true;

private:

	/** True only on the action Blueprint's own Class Defaults (drives what the Details panel shows). */
	UPROPERTY(Transient)
	bool bIsClassDefaults = false;

	/** The context of the current (or last) run. */
	UPROPERTY(Transient)
	FReactionContext RunContext;

	bool bLoaded = false;
	bool bRunning = false;
	bool bStopSequence = false;
	bool bLastSucceeded = true;
	bool bLastExecuteRan = false;

	/** This run has already called Finish Action (to catch a second call). Cleared when a run starts. */
	bool bFinishedThisRun = false;

	/** This run was cancelled (a Finish Action after that is expected, e.g. a Delay node completing late). */
	bool bCancelledThisRun = false;

	// Debug info for the overlay (not saved)
	double LastSkippedTime = -1.0;
	FString LastSkippedReason;
};

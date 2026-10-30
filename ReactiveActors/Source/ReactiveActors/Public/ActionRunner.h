// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Runs a list of actions in order. Each action runs until it calls Finish Action (straight away, or
// later for work over time); the next one starts when it does. An action finishing with Success off
// ends the run as Failed and skips the rest. Used by the component (one per reaction), by Run Action,
// and by actions that contain actions (Branch, and Run On Targets, one runner per target).
//
// Lives inside its owner. It must not move in memory while running: keep it as a member of a
// UObject, or in an array that isn't resized while sequences are running.

#pragma once

#include "CoreMinimal.h"
#include "ReactionContext.h"
#include "ActionRunner.generated.h"

class UReactiveAction;

USTRUCT()
struct REACTIVEACTORS_API FActionRunner
{
	GENERATED_BODY()

	/**
	 * Called when the list finishes, with how it ended. bStoppedEarly: it didn't reach the end of the list
	 * (a failure, a Stop Reaction, or a cancel), so a Branch around it knows to end its own chain too.
	 * Not called by Stop() or Abort().
	 */
	TFunction<void(EReactionResult /*Result*/, bool /*bStoppedEarly*/)> OnComplete;

	/** Starts running Actions with Context. Owner is the UObject this sequence lives in. */
	void Start(UObject* InOwner, const TArray<TObjectPtr<UReactiveAction>>& InActions, const FReactionContext& InContext);

	/** Abandons the run (no OnComplete). The action being waited on is left as it is. */
	void Stop();

	/**
	 * Cancels the run from outside: the action being waited on is aborted (its On Cancel runs), then
	 * the run counts as finished: OnComplete(Cancelled).
	 */
	void Cancel();

	/** Like Cancel, but silent: aborts the action being waited on, with no OnComplete. For nested sequences. */
	void Abort();

	bool IsRunning() const { return bRunning; }

	/** How the last run ended (Succeeded while it's still running). */
	EReactionResult GetResult() const { return Result; }

	/** The action that failed the last run, if it ended as Failed. For logs and the debug overlay. */
	UReactiveAction* GetFailedAction() const { return FailedAction.Get(); }

	/**
	 * True while any sequence is in the middle of running actions right now (we're inside some action's
	 * Execute Action on this call stack). Restarting a sequence then isn't safe, because it may be one of
	 * the sequences on the stack: the component cancels straight away and starts the new run next frame.
	 */
	static bool IsAnyExecuting() { return GlobalExecuteDepth > 0; }
	const FReactionContext& GetContext() const { return Context; }

	/** The action this runner is waiting on (it hasn't called Finish Action yet), if any. */
	UReactiveAction* GetWaitingOn() const { return WaitingOn.Get(); }

	// ---- Debug (read by the Reactions.Show overlay) ----
	/** 1-based position of the action running now (or last started), e.g. 3 in "3/5". */
	int32 GetCurrentStep() const { return NextIndex; }

	/** How many actions this sequence runs, e.g. 5 in "3/5". */
	int32 GetNumSteps() const { return Actions.Num(); }

	/** World time this runner started waiting on its current action. -1 if not waiting. */
	double GetWaitStartTime() const { return WaitStartTime; }

private:
	void Advance();
	void HandleActionFinished(UReactiveAction* Action);

	/** An action just finished: true if the run should carry on to the next one. */
	bool ContinueAfter(UReactiveAction* Action);
	void Complete(EReactionResult InResult, bool bStoppedEarly);
	void Unbind();

	/** How many Advance calls (of any sequence) are on the call stack right now. Game thread only. */
	static int32 GlobalExecuteDepth;


	UPROPERTY(Transient)
	TArray<TObjectPtr<UReactiveAction>> Actions;

	UPROPERTY(Transient)
	FReactionContext Context;

	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<UReactiveAction> WaitingOn;
	TWeakObjectPtr<UReactiveAction> FailedAction;
	FDelegateHandle WaitHandle;
	int32 NextIndex = 0;
	double WaitStartTime = -1.0;
	bool bRunning = false;
	EReactionResult Result = EReactionResult::Succeeded;
};

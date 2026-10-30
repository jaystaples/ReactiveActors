// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ActionRunner.h"

#include "Engine/World.h"
#include "ReactiveAction.h"
#include "Misc/ScopeExit.h"

void FActionRunner::Start(UObject* InOwner, const TArray<TObjectPtr<UReactiveAction>>& InActions, const FReactionContext& InContext)
{
	Stop();

	Owner = InOwner;
	Actions = InActions;
	Context = InContext;
	NextIndex = 0;
	bRunning = true;
	Result = EReactionResult::Succeeded;
	FailedAction.Reset();

	Advance();
}

void FActionRunner::Stop()
{
	Unbind();
	bRunning = false;
	NextIndex = 0;
}

void FActionRunner::Cancel()
{
	if (!bRunning)
	{
		return;
	}

	UReactiveAction* Waiting = WaitingOn.Get();
	Unbind();

	if (Waiting)
	{
		Waiting->Abort();
	}

	// A cancelled run still counts as finishing (rules get On Finish, with Cancelled)
	Complete(EReactionResult::Cancelled, /*bStoppedEarly*/ true);
}

void FActionRunner::Abort()
{
	UReactiveAction* Waiting = WaitingOn.Get();
	Stop();

	if (Waiting)
	{
		Waiting->Abort();
	}
}

void FActionRunner::Unbind()
{
	if (UReactiveAction* Action = WaitingOn.Get())
	{
		Action->OnFinished.Remove(WaitHandle);
	}
	WaitingOn.Reset();
	WaitHandle.Reset();
	WaitStartTime = -1.0;
}

int32 FActionRunner::GlobalExecuteDepth = 0;

void FActionRunner::Advance()
{
	// Tracks whether we're inside an action's Execute (see IsAnyExecuting)
	++GlobalExecuteDepth;
	ON_SCOPE_EXIT { --GlobalExecuteDepth; };

	while (bRunning && NextIndex < Actions.Num())
	{
		UReactiveAction* Action = Actions[NextIndex++];
		if (!Action)
		{
			continue;
		}

		Action->Execute(Context);

		if (!bRunning)
		{
			return; // something the action did stopped this runner
		}

		// Still running: it hasn't called Finish Action yet, so wait until it does
		if (Action->IsRunning())
		{
			WaitingOn = Action;
			auto Resume = [this](UReactiveAction* Finished) { HandleActionFinished(Finished); };
			UObject* OwnerObject = Owner.Get();

			// For the debug overlay's step timer and stuck detection
			const UWorld* World = OwnerObject ? OwnerObject->GetWorld() : Action->GetWorld();
			WaitStartTime = World ? World->GetTimeSeconds() : -1.0;

			WaitHandle = OwnerObject
				? Action->OnFinished.AddWeakLambda(OwnerObject, Resume)
				: Action->OnFinished.AddLambda(Resume);
			return;
		}

		// Finished straight away (called Finish Action during Execute Action, or was skipped)
		if (!ContinueAfter(Action))
		{
			return;
		}
	}

	if (bRunning)
	{
		Complete(EReactionResult::Succeeded, /*bStoppedEarly*/ false);
	}
}

bool FActionRunner::ContinueAfter(UReactiveAction* Action)
{
	// Success off: the whole chain fails here, the rest is skipped
	if (!Action->DidSucceed())
	{
		FailedAction = Action;
		Complete(EReactionResult::Failed, /*bStoppedEarly*/ true);
		return false;
	}

	// Stop Reaction (Succeeded): end early, as a success
	if (Action->RequestedSequenceStop())
	{
		Complete(EReactionResult::Succeeded, /*bStoppedEarly*/ true);
		return false;
	}
	return true;
}

void FActionRunner::HandleActionFinished(UReactiveAction* Action)
{
	if (!bRunning || Action != WaitingOn.Get())
	{
		return;
	}

	Unbind();
	if (ContinueAfter(Action))
	{
		Advance();
	}
}

void FActionRunner::Complete(EReactionResult InResult, bool bStoppedEarly)
{
	bRunning = false;
	Result = InResult;

	if (OnComplete)
	{
		// Copy first: the callback may restart this runner
		const TFunction<void(EReactionResult, bool)> Callback = OnComplete;
		Callback(InResult, bStoppedEarly);
	}
}

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - test-only blocks

#include "ReactiveTestProbes.h"

TArray<FName>& UReactiveTestProbe::RunLog()
{
	static TArray<FName> Log;
	return Log;
}

int32 UReactiveTestProbe::CountRuns(FName Label)
{
	int32 Count = 0;
	for (const FName& Entry : RunLog())
	{
		Count += Entry == Label ? 1 : 0;
	}
	return Count;
}

void UReactiveTestProbe::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	++ExecuteCount;
	++SavedRuns;
	LastContext = Context;
	RunLog().Add(Label);

	if (bFinishImmediately)
	{
		FinishAction(bSucceed);
	}
}

void UReactiveTestProbe::OnCancel_Implementation()
{
	++CancelCount;
}

void UReactiveTestProbe::OnReset_Implementation()
{
	++ResetCount;
}

void UReactiveTestProbeRule::OnStart_Implementation(const FReactionContext& Context)
{
	++StartCount;
}

void UReactiveTestProbeRule::OnFinish_Implementation(const FReactionContext& Context, EReactionResult Result)
{
	++FinishCount;
	LastResult = Result;
}

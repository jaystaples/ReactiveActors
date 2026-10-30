// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - test-only blocks
//
// A probe action and a probe rule that record what happened to them, so a test can check the order things
// ran in, how they finished, and what got cancelled or reset. Hidden from every dropdown.

#pragma once

#include "CoreMinimal.h"
#include "ReactiveAction.h"
#include "ReactiveRule.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "ReactiveTestProbes.generated.h"

/** Records each run. Finishes straight away (with bSucceed) unless bFinishImmediately is off: then the test calls FinishAction. */
UCLASS(NotBlueprintable, HideDropdown, meta = (DisplayName = "Test Probe"))
class UReactiveTestProbe : public UReactiveAction
{
	GENERATED_BODY()

public:
	/** Every probe run, in order, by Label. Cleared at the start of each test. Copies (Presets, Run On Targets) log too. */
	static TArray<FName>& RunLog();

	/** How many times Label appears in the run log. */
	static int32 CountRuns(FName Label);

	UPROPERTY()
	FName Label;

	UPROPERTY()
	bool bFinishImmediately = true;

	UPROPERTY()
	bool bSucceed = true;

	/** Counts runs like ExecuteCount, but saved (SaveGame): for the save tests. */
	UPROPERTY(SaveGame)
	int32 SavedRuns = 0;

	int32 ExecuteCount = 0;
	int32 CancelCount = 0;
	int32 ResetCount = 0;
	FReactionContext LastContext;

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual void OnCancel_Implementation() override;
	virtual void OnReset_Implementation() override;
};

/** SaveGame variables that don't survive a load, for the validation test. */
UCLASS(NotBlueprintable, HideDropdown, meta = (DisplayName = "Test Bad Save Rule"))
class UReactiveTestBadSaveRule : public UReactiveRule
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	TObjectPtr<AActor> RememberedActor;

	UPROPERTY(SaveGame)
	FTimerHandle RememberedTimer;

	/** Fine: a plain count. */
	UPROPERTY(SaveGame)
	int32 RememberedCount = 0;
};

/** Records how runs start and finish (for checking a run's Result). */
UCLASS(NotBlueprintable, HideDropdown, meta = (DisplayName = "Test Probe Rule"))
class UReactiveTestProbeRule : public UReactiveRule
{
	GENERATED_BODY()

public:
	int32 StartCount = 0;
	int32 FinishCount = 0;
	EReactionResult LastResult = EReactionResult::Succeeded;

protected:
	virtual void OnStart_Implementation(const FReactionContext& Context) override;
	virtual void OnFinish_Implementation(const FReactionContext& Context, EReactionResult Result) override;
};

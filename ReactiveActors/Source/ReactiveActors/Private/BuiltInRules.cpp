// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in rules

#include "BuiltInRules.h"

#include "Engine/World.h"

// ---- Max Activations ----------------------------------------------------------------------------

bool URULE_MaxActivations::ShouldBlock_Implementation(const FReactionContext& Context) const
{
	return MaxActivations > 0 && ActivationCount >= MaxActivations;
}

void URULE_MaxActivations::OnStart_Implementation(const FReactionContext& Context)
{
	// Counted when a run starts, so an activation counts even while its actions are still running
	++ActivationCount;
}

FString URULE_MaxActivations::GetDebugString_Implementation() const
{
	return MaxActivations > 0
		? FString::Printf(TEXT("%d/%d"), ActivationCount, MaxActivations)
		: FString::Printf(TEXT("%d/unlimited"), ActivationCount);
}

// ---- Cooldown -----------------------------------------------------------------------------------

bool URULE_Cooldown::ShouldBlock_Implementation(const FReactionContext& Context) const
{
	if (CooldownSeconds <= 0.f || LastFinishTime < 0.0)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastFinishTime) < CooldownSeconds;
}

void URULE_Cooldown::OnFinish_Implementation(const FReactionContext& Context, EReactionResult Result)
{
	if (const UWorld* World = GetWorld())
	{
		LastFinishTime = World->GetTimeSeconds();
	}
}

FString URULE_Cooldown::GetDebugString_Implementation() const
{
	const UWorld* World = GetWorld();
	if (!World || CooldownSeconds <= 0.f || LastFinishTime < 0.0)
	{
		return TEXT("ready");
	}

	const double Left = CooldownSeconds - (World->GetTimeSeconds() - LastFinishTime);
	return Left > 0.0 ? FString::Printf(TEXT("%.1fs left"), Left) : FString(TEXT("ready"));
}

void URULE_Cooldown::PreSaveState()
{
	SavedSecondsLeft = -1.f;
	const UWorld* World = GetWorld();
	if (World && LastFinishTime >= 0.0)
	{
		SavedSecondsLeft = FMath::Max(0.f, static_cast<float>(CooldownSeconds - (World->GetTimeSeconds() - LastFinishTime)));
	}
}

void URULE_Cooldown::PostLoadState()
{
	LastFinishTime = -1.0;
	const UWorld* World = GetWorld();
	if (World && SavedSecondsLeft >= 0.f)
	{
		// As if it finished just long enough ago to have SavedSecondsLeft to go
		LastFinishTime = World->GetTimeSeconds() - (CooldownSeconds - SavedSecondsLeft);
	}
}

// ---- Reset --------------------------------------------------------------------------------------

void URULE_MaxActivations::OnReset_Implementation()
{
	ActivationCount = 0;
}

void URULE_Cooldown::OnReset_Implementation()
{
	LastFinishTime = -1.0;
}

FString URULE_MaxActivations::BuildSummary() const
{
	return MaxActivations > 0 ? FString::Printf(TEXT("Max Activations %d"), MaxActivations) : FString(TEXT("Max Activations (unlimited)"));
}

FString URULE_Cooldown::BuildSummary() const
{
	return FString::Printf(TEXT("Cooldown %.1fs"), CooldownSeconds);
}

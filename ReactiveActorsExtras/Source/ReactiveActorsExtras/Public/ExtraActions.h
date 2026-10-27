// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors Extras - built-in actions that need more than the engine core
//
//   Spawn VFX              a Niagara system at the target, or attached to it (needs the Niagara plugin)
//   Set Data Layer State   a World Partition data layer's runtime state
//
// They appear in every Actions dropdown alongside the Reactive Actors ones while this plugin is enabled.
// NotBlueprintable keeps them out of the "pick parent class" list.

#pragma once

#include "CoreMinimal.h"
#include "ReactiveAction.h"
#include "WorldPartition/DataLayer/DataLayerManager.h" // for EDataLayerRuntimeState
#include "ExtraActions.generated.h"

class UDataLayerAsset;
class UNiagaraSystem;

// ---- Spawn VFX ----------------------------------------------------------------------------------

/**
 * Spawns a Niagara effect at the target, or attached to it so it follows. Carries on straight away;
 * the effect cleans itself up when it finishes (a looping one keeps going until its actor is gone).
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Spawn VFX"))
class REACTIVEACTORSEXTRAS_API UACT_SpawnVFX : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	/** The Niagara system to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	TObjectPtr<UNiagaraSystem> System;

	/** On: attached to the target (or a socket), following it. Off: left at the target's location. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bAttach = false;

	/** Attach: the socket or bone to attach to. None = the target's root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, EditCondition = "bAttach", EditConditionHides))
	FName Socket;

	/** Location, rotation and scale relative to the target (or socket), in the target's own space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	FTransform Transform = FTransform::Identity;
};

// ---- Set Data Layer State -----------------------------------------------------------------------

/** Changes a World Partition data layer's runtime state (Unloaded / Loaded / Activated). */
UCLASS(NotBlueprintable, meta = (DisplayName = "Set Data Layer State"))
class REACTIVEACTORSEXTRAS_API UACT_SetDataLayerState : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	TObjectPtr<UDataLayerAsset> DataLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	EDataLayerRuntimeState State = EDataLayerRuntimeState::Activated;

	/** Also apply the state to child data layers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bRecursive = false;
};

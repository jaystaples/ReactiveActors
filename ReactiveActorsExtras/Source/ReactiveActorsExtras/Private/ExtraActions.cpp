// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors Extras - built-in actions

#include "ExtraActions.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ReactiveActorsLog.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"

// ---- Spawn VFX ----------------------------------------------------------------------------------

void UACT_SpawnVFX::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	AActor* Target = Context.Target;
	if (!System || !Target)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: %s, nothing spawned."), *GetClass()->GetName(), *GetNameSafe(Target),
			!System ? TEXT("no System set") : TEXT("no target"));
		FinishAction();
		return;
	}

	if (bAttach && Target->GetRootComponent())
	{
		UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Target->GetRootComponent(), Socket,
			Transform.GetLocation(), Transform.Rotator(), EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ true);
		if (Component)
		{
			Component->SetRelativeScale3D(Transform.GetScale3D());
		}
	}
	else
	{
		// Location and rotation in the target's space, ignoring the target's own scale
		const FTransform TargetTransform = Target->GetActorTransform();
		const FVector Location = TargetTransform.TransformPositionNoScale(Transform.GetLocation());
		const FRotator WorldRotation = (TargetTransform.GetRotation() * Transform.GetRotation()).Rotator();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location, WorldRotation, Transform.GetScale3D(), /*bAutoDestroy*/ true);
	}

	FinishAction();
}

FString UACT_SpawnVFX::DescribeAction() const
{
	return FString::Printf(TEXT("Spawn VFX %s%s"), System ? *System->GetName() : TEXT("<no system>"),
		bAttach ? TEXT(" (attached)") : TEXT(""));
}

void UACT_SpawnVFX::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!System)
	{
		Errors.Add(INVTEXT("System isn't set, so it spawns nothing."));
	}
}

// ---- Set Data Layer State -----------------------------------------------------------------------

void UACT_SetDataLayerState::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	AActor* Target = Context.Target; // never null here
	if (!DataLayer)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: no Data Layer set."), *GetClass()->GetName(), *GetNameSafe(Target));
		FinishAction();
		return;
	}

	UDataLayerManager* Manager = UDataLayerManager::GetDataLayerManager(Target);
	if (!Manager)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s: no Data Layer Manager in this world (is it a World Partition level?)."), *GetClass()->GetName());
		FinishAction();
		return;
	}

	Manager->SetDataLayerRuntimeState(DataLayer, State, bRecursive);
	FinishAction();
}

FString UACT_SetDataLayerState::DescribeAction() const
{
	const FString StateName = StaticEnum<EDataLayerRuntimeState>()->GetDisplayNameTextByValue(static_cast<int64>(State)).ToString();
	return FString::Printf(TEXT("Data Layer %s -> %s%s"), DataLayer ? *DataLayer->GetName() : TEXT("<none>"), *StateName,
		bRecursive ? TEXT(" (and children)") : TEXT(""));
}

void UACT_SetDataLayerState::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!DataLayer)
	{
		Errors.Add(INVTEXT("Data Layer isn't set, so it changes nothing."));
	}
}

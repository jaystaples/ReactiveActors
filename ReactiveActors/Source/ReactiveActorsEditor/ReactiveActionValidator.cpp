// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveActionValidator.h"

#include "Engine/Blueprint.h"
#include "Misc/DataValidation.h"
#include "ReactiveAction.h"
#include "ReactiveActionChecks.h"
#include "ReactiveObjectBase.h"
#include "ReactiveSaveData.h"

bool UReactiveActionValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	const UBlueprint* Blueprint = Cast<UBlueprint>(InObject);
	return Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UReactiveObjectBase::StaticClass());
}

EDataValidationResult UReactiveActionValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	const UBlueprint* Blueprint = Cast<UBlueprint>(InAsset);

	// SaveGame variables that won't survive a load, with what to do instead
	if (Blueprint && Blueprint->GeneratedClass)
	{
		ReactiveSave::GatherSaveGameProblems(Blueprint->GeneratedClass, [this, InAsset](const FText& Message)
			{
				AssetWarning(InAsset, FText::Format(INVTEXT("{0}: {1}"), FText::FromString(InAsset->GetName()), Message));
			});
	}

	if (Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UReactiveAction::StaticClass())
		&& ReactiveActionChecks::FindExecuteWithoutFinish(Blueprint))
	{
		// A warning, like the compile check: it doesn't block saving or submitting
		AssetWarning(InAsset, FText::Format(INVTEXT("{0} {1}"), FText::FromString(InAsset->GetName()),
			ReactiveActionChecks::MissingFinishMessage()));
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

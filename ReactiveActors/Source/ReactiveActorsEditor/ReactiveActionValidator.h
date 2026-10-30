// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Data Validation for Reactive Actors Blueprints (actions, requirements, rules, triggers):
//  - actions that never call Finish Action (the same check as the compile warning);
//  - SaveGame variables that won't survive a load (actor references, timer handles) or do nothing (on requirements).
// Shows on save, on Validate Assets and before submitting.
// Found and run automatically by the Data Validation plugin.
// (Reactions, Presets, Reactive Volumes and every block's Validate Settings are checked by their own
// IsDataValid, in the runtime module.)

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "ReactiveActionValidator.generated.h"

UCLASS()
class UReactiveActionValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};

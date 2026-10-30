// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// How the plugin's assets appear in the Content Browser: the type label on thumbnails and in
// tooltips, the colour strip, and the filter category (Filters > Reactive).
// Asset Definitions are found automatically: no registration needed.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "ReactiveAssetDefinitions.generated.h"

/** Reaction Preset assets: labelled "Reaction Preset" instead of "Data Asset". */
UCLASS()
class UAssetDefinition_ReactionPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveAssetDefinitions.h"

#include "ReactionPreset.h"

FText UAssetDefinition_ReactionPreset::GetAssetDisplayName() const
{
	return INVTEXT("Reaction Preset");
}

FLinearColor UAssetDefinition_ReactionPreset::GetAssetColor() const
{
	return FLinearColor(FColor(230, 120, 40)); // warm orange, so presets stand out from other data assets
}

TSoftClassPtr<UObject> UAssetDefinition_ReactionPreset::GetAssetClass() const
{
	return UReactionPreset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_ReactionPreset::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(INVTEXT("Reactive")) };
	return Categories;
}

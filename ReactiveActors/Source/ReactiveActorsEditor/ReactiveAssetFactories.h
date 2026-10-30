// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Content Browser shortcuts: right-click (or Add) > Reactive > Reactive Action, Requirement, Rule,
// Trigger or Reaction Preset. Each makes a new asset with the right parent class, no class picker.
// The "Reactive" category is registered by the editor module (ReactiveActorsEditorModule.cpp).

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "ReactiveAssetFactories.generated.h"

/** Shared base: makes a Blueprint whose parent is BlueprintParentClass. */
UCLASS(Abstract)
class UReactiveBlueprintFactory : public UFactory
{
	GENERATED_BODY()

public:
	UReactiveBlueprintFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTip() const override;
	virtual FString GetDefaultNewAssetName() const override;
	/** The parent class's icon in the Content Browser's create menu, instead of the plain Blueprint icon. */
	virtual FName GetNewAssetThumbnailOverride() const override;

protected:
	/** The class the new Blueprint derives from. */
	UPROPERTY()
	TSubclassOf<UObject> BlueprintParentClass;

	/** e.g. ACT_NewAction. */
	FString DefaultName;

	FText ToolTip;
};

UCLASS()
class UReactiveActionFactory : public UReactiveBlueprintFactory
{
	GENERATED_BODY()

public:
	UReactiveActionFactory();
};

UCLASS()
class UReactiveRequirementFactory : public UReactiveBlueprintFactory
{
	GENERATED_BODY()

public:
	UReactiveRequirementFactory();
};

UCLASS()
class UReactiveRuleFactory : public UReactiveBlueprintFactory
{
	GENERATED_BODY()

public:
	UReactiveRuleFactory();
};

UCLASS()
class UReactiveTriggerFactory : public UReactiveBlueprintFactory
{
	GENERATED_BODY()

public:
	UReactiveTriggerFactory();
};

/** A new Reaction Preset data asset (reactions and triggers to share between actors). */
UCLASS()
class UReactionPresetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UReactionPresetFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetToolTip() const override;
	virtual FString GetDefaultNewAssetName() const override { return TEXT("RP_NewPreset"); }
};

namespace ReactiveActorsEditor
{
	/** The Content Browser category key, registered at module startup. */
	inline const FName AssetCategoryName(TEXT("Reactive"));
}

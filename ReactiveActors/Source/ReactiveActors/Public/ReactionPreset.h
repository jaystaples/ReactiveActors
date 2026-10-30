// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Reaction Preset: a reusable set of reactions and triggers, a complete behaviour you can drop on
// any actor ("flickering light", "radio that turns on when you look away").
// Reference it from a Reaction Component's Preset: each actor gets its own copies, so state
// (Max Activations, cooldowns, timers) is never shared. The preset's reactions and triggers run
// first, then the component's own.
//
// Was Game Action Preset (before that, Game Action Protocol Data): old assets load through the
// CoreRedirects in Docs/CoreRedirects.ini (copied into the project's DefaultEngine.ini).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Reaction.h"
#include "ReactionPreset.generated.h"

class UReactiveTrigger;

UCLASS(BlueprintType, meta = (DisplayName = "Reaction Preset"))
class REACTIVEACTORS_API UReactionPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Reactions every actor using this preset gets. They run before the actor's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Actors", meta = (TitleProperty = "EventTag"))
	TArray<FReaction> Reactions;

	/** Triggers every actor using this preset gets (each actor its own copy). They activate before the actor's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Reactive Actors", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveTrigger>> Triggers;

#if WITH_EDITOR
	/** Data Validation (on save, or right-click > Validate): empty slots, missing settings, each block's Validate Settings. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	/** New reactions and triggers get IDs for saved games (and copies get new ones). */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

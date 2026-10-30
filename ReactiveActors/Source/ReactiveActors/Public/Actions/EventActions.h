// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions
//
// Events: actions that send events or switch reactions. Broadcast Event, Set Reaction Enabled, Reset Reactions.
// NotBlueprintable keeps them out of the "pick parent class" list; they still appear in every Actions dropdown.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ReactiveAction.h"
#include "EventActions.generated.h"

// ---- Broadcast Event ----------------------------------------------------------------------

/** Sends another event: the way to chain reactions ("when this finishes, fire that"). */
UCLASS(NotBlueprintable, meta = (DisplayName = "Broadcast Event"))
class REACTIVEACTORS_API UACT_BroadcastEvent : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ReactiveTags = "Event"))
	FGameplayTag Event;

	/** World: everyone listening. Self: only this action's target. Identity: actors with Target Identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, DisplayName = "Event Scope"))
	EReactiveScopePolicy Scope = EReactiveScopePolicy::World;

	/** Identity Scope: send to actors with this identity, or a child of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action",
		meta = (ExposeOnSpawn, ReactiveTags = "Identity", EditCondition = "Scope == EReactiveScopePolicy::Identity", EditConditionHides))
	FGameplayTag TargetIdentity;
};

// ---- Set Reaction Enabled -----------------------------------------------------------------------

/**
 * Switches reactions on the target's Reaction Component on or off while playing. A disabled reaction
 * doesn't fire; one already running finishes normally.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Set Reaction Enabled"))
class REACTIVEACTORS_API UACT_SetReactionEnabled : public UReactiveAction
{
	GENERATED_BODY()

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	/** Reactions listening for this event (or a child of it). Empty = every reaction on the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ReactiveTags = "Event"))
	FGameplayTag Event;

	/** On: switch the reactions on. Off: switch them off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, DisplayName = "Enable"))
	bool bEnableReactions = true;
};

// ---- Reset Reactions ----------------------------------------------------------------------------

/**
 * Resets reactions on the target's Reaction Component as if they'd never fired: cancels running ones,
 * then rules (Max Activations, Cooldown), actions and triggers get On Reset.
 */
// Deprecated as an action and hidden from the Actions dropdown: resetting mid-reaction is rarely what you want.
// Reset from game code with the Reset All Reactions (and For Actor / Identity / Event) nodes instead.
// Existing ones still load and work.
UCLASS(NotBlueprintable, HideDropdown, meta = (DisplayName = "Reset Reactions (deprecated)"))
class REACTIVEACTORS_API UACT_ResetReactions : public UReactiveAction
{
	GENERATED_BODY()

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	/** Reactions listening for this event (or a child of it), and triggers sending it. Empty = everything on the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ReactiveTags = "Event"))
	FGameplayTag Event;
};

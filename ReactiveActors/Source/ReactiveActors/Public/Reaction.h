// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// A Reaction: "when this event arrives, run these actions".
// Authored inline in a Reaction Component's Reactions array (or a Reaction Preset).

#pragma once

#include "CoreMinimal.h"
#include "ActionRunner.h"
#include "GameplayTagContainer.h"
#include "Reaction.generated.h"

class UReactiveAction;
class UReactiveRule;

/** What a reaction does when its event arrives while it's still running. */
UENUM(BlueprintType)
enum class EReactiveRetriggerPolicy : uint8
{
	/** Keep the current run going; the new event is ignored. */
	Ignore,
	/** Cancel the current run, then start again from the first action (if the rules allow a new run). */
	Restart,
};

USTRUCT(BlueprintType)
struct REACTIVEACTORS_API FReaction
{
	GENERATED_BODY()

	/** Disabled reactions never fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	bool bEnabled = true;

	/**
	 * The event that fires this reaction. Parent tags match children: Event.Photo also fires on Event.Photo.Kitchen.
	 * The actions can read it (Get Event, or Context's Event). Only leave it empty for a reaction you run
	 * directly with Run Reaction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction", meta = (ReactiveTags = "Event"))
	FGameplayTag EventTag;

	/**
	 * If the event arrives while this reaction is still running (an action hasn't called Finish Action yet):
	 * Ignore keeps the current run going. Restart cancels it and starts again from the first action.
	 * A restart is a new run: the rules must allow it, and it counts as an activation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	EReactiveRetriggerPolicy Retrigger = EReactiveRetriggerPolicy::Ignore;

	/** Run in order every time this reaction fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Reaction", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveAction>> Actions;

	/**
	 * Shape how this reaction activates (Max Activations, Cooldown): any rule can block it, and all hear
	 * when it starts and finishes (with its Result). Each reaction has its own copies, with their own state.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Reaction", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveRule>> Rules;

	/**
	 * Identifies this reaction in saved games, so saves still match after the list is reordered. Given automatically
	 * in the editor (a copy gets a new one). Not shown.
	 */
	UPROPERTY()
	FGuid ReactionId;

	/** Runtime: runs Actions when the reaction fires. Never edited or saved. */
	UPROPERTY(Transient)
	FActionRunner Sequence;

	// ---- Runtime debug info (read by the Reactions.Show overlay; not edited or saved) ----
	double LastFiredTime = -1.0;
	FGameplayTag LastFiredEvent;
	double LastBlockedTime = -1.0;
	FString LastBlockedBy;
	double LastFailedTime = -1.0;
	FString LastFailedBy;

	bool Matches(const FGameplayTag& Event) const { return EventTag.IsValid() && Event.MatchesTag(EventTag); }
};

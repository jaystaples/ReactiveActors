// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Project Settings > Plugins > Reactive Actors. Saved to the project's DefaultGame.ini.
// The debug overlay values are applied to the Reactions.* console variables at startup and whenever
// they're edited here; typing a console command still overrides them for that session.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ReactiveActorsSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Reactive Actors"))
class REACTIVEACTORS_API UReactiveActorsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UReactiveActorsSettings();

	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

	/** Pushes the overlay values below into the Reactions.* console variables (not in shipping builds). */
	void ApplyToConsoleVariables() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ---- Gameplay tags ---------------------------------------------------------------------------

	/**
	 * Event pickers (a reaction's Event Tag, a trigger's Event, Broadcast Event, Event Received) only
	 * offer this tag and its children, e.g. Event. Empty: every tag.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Tags")
	FGameplayTag EventTagRoot;

	/**
	 * Identity pickers (Identity Tags, Target Identity) only offer this tag and its children,
	 * e.g. Actor. Empty: every tag.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Tags")
	FGameplayTag IdentityTagRoot;

	// ---- Save and load -----------------------------------------------------------------------------

	/**
	 * A Reaction Component whose level streams out (a streaming level, a World Partition cell) keeps what it
	 * remembers (Max Activations counts, Cooldowns, reactions switched off) and gets it back when it streams in.
	 * Off: it starts fresh each time it streams in. Works without any saving.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Save and Load")
	bool bKeepStateWhenStreamedOut = true;

	/**
	 * Hold Reactions Until Restored lets go by itself after this many seconds if no restore has come (a load
	 * that failed part way), with a warning, so reactions fire late rather than never. 0: wait for ever.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Save and Load", meta = (ClampMin = 0, Units = "s"))
	float HoldSafetyRelease = 10.f;

	// ---- Logging ---------------------------------------------------------------------------------

	/**
	 * Log a warning when an action is skipped because a requirement failed. Off: logged at Verbose
	 * instead (still on the overlay's "skipped" lines).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Logging")
	bool bWarnOnSkippedActions = true;

	/** Log a line when a reaction ends as Failed, naming the action that failed it. */
	UPROPERTY(Config, EditAnywhere, Category = "Logging")
	bool bLogFailedReactions = true;

	// ---- Debug overlay (Reactions.Show) ----------------------------------------------------------

	/** Max distance from the camera to draw an actor's status (Reactions.Show.Distance). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Overlay", meta = (ClampMin = 0, Units = "cm"))
	float ShowDistance = 3000.f;

	/** Height above the actor's bounds for the debug text and the editor icon (Reactions.Show.Offset). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Overlay", meta = (Units = "cm"))
	float ShowOffset = 10.f;

	/** How long 'fired', 'blocked', 'failed' and 'skipped' lines stay (Reactions.Show.Linger). 0 = off. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Overlay", meta = (ClampMin = 0, Units = "s"))
	float LingerTime = 3.f;

	/** An action that hasn't called Finish Action after this long is flagged as stuck (Reactions.Show.StuckTime). 0 = never. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Overlay", meta = (ClampMin = 0, Units = "s"))
	float StuckTime = 15.f;

	/** Show each rule's live state, e.g. Max Activations 2/3 (Reactions.Show.Rules). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Overlay")
	bool bShowRules = true;

	/** How long each wiring line stays (Reactions.Show.WiringTime). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Overlay", meta = (ClampMin = 0, Units = "s"))
	float WiringTime = 3.f;
};

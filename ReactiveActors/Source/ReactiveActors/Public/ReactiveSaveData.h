// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Save and load: what Reaction Components remember, as plain structs any save system can carry.
//
// The plugin keeps an in-memory cache of what changed in the current level (the Reactive State Cache, a Game Instance
// subsystem): components write to it when they stream out or the level ends, and read from it when they begin play.
// Your game only ever handles one struct per level, Reactive World State:
//
//   Get World Reaction State     the current level's state (loaded and streamed-out actors), to put in your save
//   Set World Reaction State     hand it back: before Open Level, or after the level loads
//   Clear World Reaction State   forget it (New Game, chapter reset)
//   On World Reaction State Expiring   the level is ending: store the struct it gives you (travel between levels)
//
// What's saved: each reaction's Enabled, and the SaveGame variables of its rules, actions and triggers: Max Activations
// counts, Cooldown time left and Random Action's last pick built in, plus any variable you tick SaveGame on in your own.
// Only actors where something actually happened are included. Not saved: runs in progress (a reaction half way through
// a Delay is simply gone after loading; nothing restarts). Tick SaveGame on plain values (counts, flags, durations,
// names, tags), not timers or actor references: Data Validation flags those.
//
// Everything here is plain data: names, tags, IDs, numbers and bytes, no object pointers, so Unreal's own SaveGame,
// SPUD and Easy Multi Save all carry it as an ordinary variable.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Misc/Guid.h"
#include "ReactiveSaveData.generated.h"

class UReactiveTrigger;
struct FReaction;

/** One rule's (or trigger's) saved variables. */
USTRUCT(BlueprintType)
struct REACTIVEACTORS_API FReactiveSavedObject
{
	GENERATED_BODY()

	/**
	 * Its class (full path), to check it's still the same kind of rule on load. Renamed classes are followed through
	 * Core Redirects (C++) and redirectors (Blueprints).
	 */
	UPROPERTY(SaveGame)
	FString ClassPath;

	/** Its SaveGame variables, written by Unreal's own property saving (tolerates variables added or removed later). */
	UPROPERTY(SaveGame)
	TArray<uint8> Data;

	/**
	 * The engine's file versions when Data was written, so it's read back the way it was written after an engine
	 * upgrade (the same thing Unreal's own Save Game files record). 0: not recorded, read as the current version.
	 */
	UPROPERTY(SaveGame)
	int32 FileVersionUE4 = 0;

	UPROPERTY(SaveGame)
	int32 FileVersionUE5 = 0;

	UPROPERTY(SaveGame)
	int32 FileVersionLicensee = 0;

	/** The custom versions (engine and plugin formats) Data's properties used, if any. */
	UPROPERTY(SaveGame)
	TArray<uint8> CustomVersions;
};

/** One reaction's saved state. */
USTRUCT(BlueprintType)
struct REACTIVEACTORS_API FReactiveSavedReaction
{
	GENERATED_BODY()

	/** The reaction's ID (given in the editor), so it's found again even if the list is reordered. */
	UPROPERTY(SaveGame)
	FGuid ReactionId;

	/** Fallback for reactions saved before they had an ID: Preset or own list, position, and event tag. */
	UPROPERTY(SaveGame)
	bool bFromPreset = false;

	UPROPERTY(SaveGame)
	int32 Index = INDEX_NONE;

	UPROPERTY(SaveGame)
	FGameplayTag EventTag;

	UPROPERTY(SaveGame)
	bool bEnabled = true;

	/** Its rules, in order. */
	UPROPERTY(SaveGame)
	TArray<FReactiveSavedObject> Rules;

	/** Its actions' SaveGame variables, in order, nested ones (inside Branch, Random Action, Run On Targets) after their parent. */
	UPROPERTY(SaveGame)
	TArray<FReactiveSavedObject> Actions;
};

/** One trigger's saved state. */
USTRUCT(BlueprintType)
struct REACTIVEACTORS_API FReactiveSavedTrigger
{
	GENERATED_BODY()

	/** The trigger's ID (given in the editor). */
	UPROPERTY(SaveGame)
	FGuid TriggerId;

	/** Fallback for triggers saved before they had an ID: Preset or own list, and position. */
	UPROPERTY(SaveGame)
	bool bFromPreset = false;

	UPROPERTY(SaveGame)
	int32 Index = INDEX_NONE;

	/** The trigger's own SaveGame variables (and its class, to check). */
	UPROPERTY(SaveGame)
	FReactiveSavedObject Trigger;

	UPROPERTY(SaveGame)
	TArray<FReactiveSavedObject> Rules;
};

/** What one Reaction Component remembers. Capture Reaction State gives you one (for actors you spawn yourself). */
USTRUCT(BlueprintType)
struct REACTIVEACTORS_API FReactiveActorState
{
	GENERATED_BODY()

	/** Which component: its level, actor and component name. Stable for actors placed in a level. */
	UPROPERTY(SaveGame)
	FString Key;

	UPROPERTY(SaveGame)
	TArray<FReactiveSavedReaction> Reactions;

	UPROPERTY(SaveGame)
	TArray<FReactiveSavedTrigger> Triggers;

	bool IsEmpty() const { return Reactions.IsEmpty() && Triggers.IsEmpty(); }
};

/**
 * Everything that changed in one level: the one struct you store per level. Get World Reaction State gives you
 * the current level's; Set World Reaction State takes it back.
 */
USTRUCT(BlueprintType)
struct REACTIVEACTORS_API FReactiveWorldState
{
	GENERATED_BODY()

	/** Which level (its map name, without the editor's PIE prefix). Set World Reaction State uses it to find the level. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Reactive Actors")
	FString WorldKey;

	/** Format version, for future changes. */
	UPROPERTY(SaveGame)
	int32 Version = 1;

	/** One entry per actor where something happened. */
	UPROPERTY(SaveGame)
	TArray<FReactiveActorState> Actors;

	bool IsEmpty() const { return Actors.IsEmpty(); }
};

namespace ReactiveSave
{
	/** C++: an object's SaveGame variables as bytes (all of them, defaults included), and back. */
	REACTIVEACTORS_API FReactiveSavedObject SaveObject(UObject* Object, bool bKeepClassWhenEmpty = false);

	/** C++: true if Class has any variable with SaveGame ticked. */
	REACTIVEACTORS_API bool HasSaveGameVariables(const UClass* Class);
	REACTIVEACTORS_API bool LoadObject(UObject* Object, const FReactiveSavedObject& Saved);

	/** C++: true if Class is the class SavedPath names, following renames (Core Redirects, Blueprint redirectors). */
	REACTIVEACTORS_API bool IsSameClass(const UClass* Class, const FString& SavedPath);

	/** C++: the key a world's state is stored under: its map package name, without the PIE prefix. */
	REACTIVEACTORS_API FString GetWorldKey(const UWorld* World);

#if WITH_EDITOR
	/**
	 * Editor: gives every reaction and trigger in these lists an ID if it has none, and a new one if it shares an ID
	 * with another (copy-pasted or duplicated). Returns true if anything changed.
	 */
	REACTIVEACTORS_API bool EnsureIds(TArray<FReaction>& Reactions, TArray<TObjectPtr<UReactiveTrigger>>& Triggers);

	/**
	 * Editor: SaveGame variables on a block class that won't survive a load (actor or component references, timer
	 * handles), or that do nothing (on a requirement, which is never saved). Warn is called with each message.
	 */
	REACTIVEACTORS_API void GatherSaveGameProblems(const UClass* Class, TFunctionRef<void(const FText&)> Warn);
#endif
}

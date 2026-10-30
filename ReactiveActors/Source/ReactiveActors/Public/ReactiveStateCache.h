// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Reactive State Cache: what Reaction Components remember, kept in memory for the current level so it survives
// streaming, and handed to your game as one Reactive World State struct per level. A Game Instance subsystem, so it
// outlives level travel. It never writes files: your save system (Unreal's SaveGame, SPUD, Easy Multi Save...) stores
// the struct like any other variable.
//
// How state moves:
//   - A component where something happened (a reaction ran, a trigger fired, a reaction was switched on or off) writes
//     its state here when it streams out or its level ends, and takes it back when it begins play again.
//   - Get World Reaction State: the current level's state, loaded actors and streamed-out ones, for your save.
//   - Set World Reaction State: hands a saved state back. Before Open Level, it waits for that level; after the level
//     has loaded, it's applied straight away (use Hold Reactions Until Restored if it comes a few frames late).
//   - On World Reaction State Expiring: the level is ending (travel). Store the struct it gives you; the cache then
//     forgets it, so only the current level is ever held.
//   - Clear World Reaction State: forget everything (New Game, chapter reset).

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ReactiveSaveData.h"
#include "ReactiveStateCache.generated.h"

class UReactionComponent;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldReactionStateExpiring, const FReactiveWorldState&, WorldState);

UCLASS(meta = (DisplayName = "Reactive State Cache"))
class REACTIVEACTORS_API UReactiveStateCache : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The cache for WorldContextObject's game. Null outside a game (the editor world, a commandlet). */
	static UReactiveStateCache* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * A level is ending (travel to another level, or the game or Play In Editor ending): its state, to store in your
	 * save if you want to come back to it. Fired after its actors have written their state, and only if something
	 * happened in it. The cache forgets it straight after.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Reactive Actors|Save")
	FOnWorldReactionStateExpiring OnWorldReactionStateExpiring;

	/** The current level's state: loaded actors where something happened, plus ones that streamed out. */
	FReactiveWorldState GetWorldState(UWorld* World);

	/**
	 * Hands a saved state back. If World is playing that level, actors that are loaded get it now (running reactions
	 * are cancelled first) and the rest when they stream in. Otherwise it waits for that level to start (Open Level).
	 * Releases Hold Reactions Until Restored.
	 */
	void SetWorldState(UWorld* World, const FReactiveWorldState& State);

	/** Forgets everything: the current level's cache and any state waiting for a level to start. */
	void ClearAll();

	/** True if a saved state was given to World (it was waiting when World started, or Set while it was playing). */
	bool WasRestored(const UWorld* World) const;

	// ---- C++, from UReactionComponent ---------------------------------------------------------------

	/** A component's state, as it streams out or its level ends. */
	void StoreActor(const UWorld* World, FReactiveActorState&& State);

	/** A component beginning play: takes its state, if the cache has one. */
	bool TakeActor(const UWorld* World, const FString& Key, FReactiveActorState& OutState);

private:
	void BindWorldDelegates();
	void UnbindWorldDelegates();

	void HandlePostWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	/** Every Reaction Component in World that has begun play. */
	static TArray<UReactionComponent*> GetLoadedComponents(UWorld* World);

	/** Writes loaded components where something happened into Bucket (replacing their old entries). */
	static void FlushLoaded(UWorld* World, FReactiveWorldState& Bucket);

	static bool IsGameWorld(const UWorld* World);

	/** The current level's cache, by world key (normally just one). */
	TMap<FString, FReactiveWorldState> Live;

	/** Set World Reaction State made before its level started: waiting for it, by world key. */
	TMap<FString, FReactiveWorldState> Pending;

	/** World keys whose current level was given a saved state (for Hold Reactions Until Restored). */
	TSet<FString> Restored;

	/** Worlds already handed out by On World Reaction State Expiring: anything they write after that is ignored. */
	TSet<TWeakObjectPtr<UWorld>> HandedOut;

	/** Set World Reaction State applied to a playing level: kept in Pending until that level ticks again, so an
	 *  Open Level of the same map straight after (a reload) still gets it. */
	TWeakObjectPtr<UWorld> PendingKeptFor;
	FString PendingKeptKey;

	FDelegateHandle PostWorldInitHandle;
	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle TickStartHandle;
};

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveStateCache.h"
#include "ReactiveActorsLog.h"
#include "ReactionComponent.h"
#include "ReactionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

UReactiveStateCache* UReactiveStateCache::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr)
	{
		return GameInstance->GetSubsystem<UReactiveStateCache>();
	}

#if WITH_EDITOR
	// A game world with no Game Instance: only the automated tests make those. One shared cache for them.
	if (World && World->IsGameWorld())
	{
		static TStrongObjectPtr<UReactiveStateCache> NoGameInstanceCache;
		if (!NoGameInstanceCache.IsValid())
		{
			NoGameInstanceCache.Reset(NewObject<UReactiveStateCache>(GetTransientPackage()));
			NoGameInstanceCache->BindWorldDelegates();
		}
		return NoGameInstanceCache.Get();
	}
#endif
	return nullptr;
}

void UReactiveStateCache::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BindWorldDelegates();
}

void UReactiveStateCache::Deinitialize()
{
	UnbindWorldDelegates();
	ClearAll();
	Super::Deinitialize();
}

void UReactiveStateCache::BindWorldDelegates()
{
	PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UReactiveStateCache::HandlePostWorldInit);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UReactiveStateCache::HandleWorldCleanup);
}

void UReactiveStateCache::UnbindWorldDelegates()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle);
	TickStartHandle.Reset();
}

bool UReactiveStateCache::IsGameWorld(const UWorld* World)
{
	return World && World->IsGameWorld();
}

// ---- Public ---------------------------------------------------------------------------------------

FReactiveWorldState UReactiveStateCache::GetWorldState(UWorld* World)
{
	const FString Key = ReactiveSave::GetWorldKey(World);

	// A copy: streamed-out actors from the cache, then every loaded one where something happened
	FReactiveWorldState State;
	if (const FReactiveWorldState* Bucket = Live.Find(Key))
	{
		State = *Bucket;
	}
	State.WorldKey = Key;
	FlushLoaded(World, State);
	return State;
}

void UReactiveStateCache::SetWorldState(UWorld* World, const FReactiveWorldState& State)
{
	const FString Key = State.WorldKey.IsEmpty() ? ReactiveSave::GetWorldKey(World) : State.WorldKey;

	FReactiveWorldState Saved = State;
	Saved.WorldKey = Key;

	// Waits for that level to start (Open Level after this)
	Pending.Add(Key, Saved);

	// Already playing that level (restoring after it loaded): apply now
	if (IsGameWorld(World) && World->HasBegunPlay() && ReactiveSave::GetWorldKey(World) == Key)
	{
		TMap<FString, UReactionComponent*> Loaded;
		for (UReactionComponent* Component : GetLoadedComponents(World))
		{
			Loaded.Add(Component->GetSaveKey(), Component);
		}

		// The save replaces whatever this session had cached for the level
		FReactiveWorldState Bucket;
		Bucket.WorldKey = Key;
		for (const FReactiveActorState& Actor : Saved.Actors)
		{
			if (UReactionComponent** Found = Loaded.Find(Actor.Key))
			{
				(*Found)->RestoreReactionState(Actor);
			}
			else
			{
				Bucket.Actors.Add(Actor); // restored when it streams in
			}
		}
		Live.Add(Key, MoveTemp(Bucket));
		Restored.Add(Key);

		// Keep the pending copy until this level ticks again: if Open Level of the same map comes first (a reload),
		// the new level gets it; if not, it's dropped so a later visit doesn't pick up a stale copy
		PendingKeptFor = World;
		PendingKeptKey = Key;
		if (!TickStartHandle.IsValid())
		{
			TickStartHandle = FWorldDelegates::OnWorldTickStart.AddWeakLambda(this, [this](UWorld* TickingWorld, ELevelTick, float)
				{
					if (TickingWorld && TickingWorld == PendingKeptFor.Get())
					{
						Pending.Remove(PendingKeptKey);
						PendingKeptFor.Reset();
						FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle);
						TickStartHandle.Reset();
					}
				});
		}

		if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			Subsystem->ReleaseHeld();
		}
	}
}

void UReactiveStateCache::ClearAll()
{
	Live.Reset();
	Pending.Reset();
	Restored.Reset();
	PendingKeptFor.Reset();
}

bool UReactiveStateCache::WasRestored(const UWorld* World) const
{
	return Restored.Contains(ReactiveSave::GetWorldKey(World));
}

// ---- Components ------------------------------------------------------------------------------------

void UReactiveStateCache::StoreActor(const UWorld* World, FReactiveActorState&& State)
{
	if (!IsGameWorld(World) || HandedOut.Contains(MakeWeakObjectPtr(const_cast<UWorld*>(World))) || State.Key.IsEmpty())
	{
		return; // its level was already handed out (or it's not a game): nothing should come back from it
	}

	const FString WorldKey = ReactiveSave::GetWorldKey(World);
	FReactiveWorldState& Bucket = Live.FindOrAdd(WorldKey);
	Bucket.WorldKey = WorldKey;

	const FString ActorKey = State.Key;
	Bucket.Actors.RemoveAll([&ActorKey](const FReactiveActorState& Entry) { return Entry.Key == ActorKey; });
	Bucket.Actors.Add(MoveTemp(State));
}

bool UReactiveStateCache::TakeActor(const UWorld* World, const FString& Key, FReactiveActorState& OutState)
{
	FReactiveWorldState* Bucket = Live.Find(ReactiveSave::GetWorldKey(World));
	if (!Bucket)
	{
		return false;
	}

	const int32 Index = Bucket->Actors.IndexOfByPredicate([&Key](const FReactiveActorState& Entry) { return Entry.Key == Key; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	// The live component holds it from here; it comes back when it streams out again
	OutState = MoveTemp(Bucket->Actors[Index]);
	Bucket->Actors.RemoveAtSwap(Index);
	return true;
}

// ---- Worlds ----------------------------------------------------------------------------------------

void UReactiveStateCache::HandlePostWorldInit(UWorld* World, const UWorld::InitializationValues /*IVS*/)
{
	// Not filtered by Game Instance: it isn't always set on the world yet when this fires
	if (!IsGameWorld(World))
	{
		return;
	}

	HandedOut.Remove(MakeWeakObjectPtr(World));
	for (auto It = HandedOut.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// A level starting fresh: its cache starts empty, unless a saved state is waiting for it
	const FString Key = ReactiveSave::GetWorldKey(World);
	Live.Remove(Key);
	Restored.Remove(Key);

	FReactiveWorldState Waiting;
	if (Pending.RemoveAndCopyValue(Key, Waiting))
	{
		Waiting.WorldKey = Key;
		Live.Add(Key, MoveTemp(Waiting));
		Restored.Add(Key);
	}
}

void UReactiveStateCache::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	if (!IsGameWorld(World) || World->GetGameInstance() != GetGameInstance() || HandedOut.Contains(MakeWeakObjectPtr(World)))
	{
		return;
	}

	const FString Key = ReactiveSave::GetWorldKey(World);
	FReactiveWorldState Bucket;
	Live.RemoveAndCopyValue(Key, Bucket);
	Bucket.WorldKey = Key;

	// Actors that haven't ended play yet write their state now
	FlushLoaded(World, Bucket);
	HandedOut.Add(MakeWeakObjectPtr(World));
	Restored.Remove(Key);

	if (Bucket.IsEmpty())
	{
		return;
	}

	if (OnWorldReactionStateExpiring.IsBound())
	{
		OnWorldReactionStateExpiring.Broadcast(Bucket);
	}
	else
	{
		UE_LOG(LogReactiveActors, Log, TEXT("%s ended with %d actor(s) of reaction state, and nothing bound to On World Reaction State Expiring, so it wasn't kept."),
			*Key, Bucket.Actors.Num());
	}
}

// ---- Helpers ---------------------------------------------------------------------------------------

TArray<UReactionComponent*> UReactiveStateCache::GetLoadedComponents(UWorld* World)
{
	TArray<UReactionComponent*> Result;
	if (!World)
	{
		return Result;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UReactionComponent*> Components(*It);
		for (UReactionComponent* Component : Components)
		{
			if (Component && Component->HasBegunPlay())
			{
				Result.Add(Component);
			}
		}
	}
	return Result;
}

void UReactiveStateCache::FlushLoaded(UWorld* World, FReactiveWorldState& Bucket)
{
	for (UReactionComponent* Component : GetLoadedComponents(World))
	{
		if (!Component->HasStateChanged() || !Component->IsPlacedInLevel())
		{
			continue; // nothing happened (its default state is right), or spawned (saved with its actor, not here)
		}
		FReactiveActorState State = Component->CaptureReactionState();
		const FString ActorKey = State.Key;
		Bucket.Actors.RemoveAll([&ActorKey](const FReactiveActorState& Entry) { return Entry.Key == ActorKey; });
		Bucket.Actors.Add(MoveTemp(State));
	}
}

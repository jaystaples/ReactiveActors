// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - test helpers

#include "ReactiveTestHelpers.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "GameplayTagContainer.h"
#include "ReactionComponent.h"
#include "ReactionSubsystem.h"
#include "ReactiveStateCache.h"
#include "ReactiveTestProbes.h"

namespace ReactiveTests
{
	// The test tags live in Config/Tags/ReactiveActorsTestTags.ini, not as native tags: native tags must come from a
	// module that also exists in game builds (Unreal ensures on startup otherwise), and this module is Editor only.
	static FGameplayTag TestTag(const TCHAR* Name)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Name), /*ErrorIfNotFound*/ false);
		ensureMsgf(Tag.IsValid(), TEXT("Test tag %s is missing: is Config/Tags/ReactiveActorsTestTags.ini in the plugin?"), Name);
		return Tag;
	}

	FGameplayTag Ping() { static const FGameplayTag Tag = TestTag(TEXT("ReactiveTest.Ping")); return Tag; }
	FGameplayTag PingChild() { static const FGameplayTag Tag = TestTag(TEXT("ReactiveTest.Ping.Child")); return Tag; }
	FGameplayTag Pong() { static const FGameplayTag Tag = TestTag(TEXT("ReactiveTest.Pong")); return Tag; }
	FGameplayTag IdentityA() { static const FGameplayTag Tag = TestTag(TEXT("ReactiveTest.Identity.A")); return Tag; }
	FGameplayTag IdentityB() { static const FGameplayTag Tag = TestTag(TEXT("ReactiveTest.Identity.B")); return Tag; }

	UReactiveTestProbe* MakeProbe(UObject* Outer, FName Label, bool bFinishNow, bool bSucceed)
	{
		UReactiveTestProbe* Probe = NewObject<UReactiveTestProbe>(Outer);
		Probe->Label = Label;
		Probe->bFinishImmediately = bFinishNow;
		Probe->bSucceed = bSucceed;
		return Probe;
	}

	FReaction MakeReaction(const FGameplayTag& Event, const TArray<UReactiveAction*>& Actions,
		const TArray<UReactiveRule*>& Rules, EReactiveRetriggerPolicy Retrigger)
	{
		FReaction Reaction;
		Reaction.EventTag = Event;
		Reaction.Retrigger = Retrigger;
		for (UReactiveAction* Action : Actions)
		{
			Reaction.Actions.Add(Action);
		}
		for (UReactiveRule* Rule : Rules)
		{
			Reaction.Rules.Add(Rule);
		}
		return Reaction;
	}

	void ResetLog()
	{
		UReactiveTestProbe::RunLog().Reset();
	}
}

FReactiveTestWorld::FReactiveTestWorld()
{
	World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);

	// No Game Instance in a bare world: the tests share one cache, emptied for each test
	if (UReactiveStateCache* Cache = UReactiveStateCache::Get(World))
	{
		Cache->ClearAll();
	}

	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	// No game mode in a bare world, so begin play the way it would: actors spawned from now on begin play straight away
	if (AWorldSettings* Settings = World->GetWorldSettings())
	{
		Settings->NotifyBeginPlay();
	}
}

FReactiveTestWorld::~FReactiveTestWorld()
{
	if (World)
	{
		// End play properly first, as leaving a level would (also stops anything still running)
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(/*bInformEngineOfWorld*/ false);
		World->RemoveFromRoot();
		World = nullptr;
	}
}

UReactionSubsystem* FReactiveTestWorld::Subsystem() const
{
	return World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
}

AActor* FReactiveTestWorld::SpawnActor() const
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (Actor)
	{
		Actor->bNetStartup = true; // behaves like an actor placed in the level (the save cache only keeps those)
	}
	return Actor;
}

UReactionComponent* FReactiveTestWorld::AddComponent(AActor* Actor, const FGameplayTagContainer& IdentityTags) const
{
	UReactionComponent* Component = NewObject<UReactionComponent>(Actor);
	ReactiveTests::Prop<FGameplayTagContainer>(Component, TEXT("IdentityTags")) = IdentityTags;
	return Component;
}

void FReactiveTestWorld::AddReaction(UReactionComponent* Component, const FReaction& Reaction) const
{
	ReactiveTests::Prop<TArray<FReaction>>(Component, TEXT("Reactions")).Add(Reaction);
}

void FReactiveTestWorld::Start(UReactionComponent* Component) const
{
	// The actor has already begun play, so registering begins play on the component too
	Component->RegisterComponent();
}

void FReactiveTestWorld::Tick(float Seconds, float Step) const
{
	for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += Step)
	{
		// The timer manager only ticks once per engine frame, and a test runs inside one frame:
		// move the frame counter on so each step counts as a new frame (Delay and other timers)
		++GFrameCounter;
		World->Tick(LEVELTICK_All, Step);
	}
}

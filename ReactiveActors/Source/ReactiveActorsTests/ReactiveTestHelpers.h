// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - test helpers
//
// FReactiveTestWorld: a small game world made for one test and torn down after it, with begun play,
// a Reaction Subsystem and a way to move time forward. Plus helpers to build actors and reactions
// without going through the editor (protected settings are set by name, the way the Details panel does).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Reaction.h"

class AActor;
class UReactionComponent;
class UReactionSubsystem;
class UReactiveAction;
class UReactiveRule;
class UReactiveTestProbe;
class UWorld;

namespace ReactiveTests
{
	/** The test tags: ReactiveTest.Ping, its child Ping.Child, ReactiveTest.Pong, and identities A and B. */
	FGameplayTag Ping();
	FGameplayTag PingChild();
	FGameplayTag Pong();
	FGameplayTag IdentityA();
	FGameplayTag IdentityB();

	/** A UPROPERTY on Object, by name, so tests can set protected settings. Asserts if the name is wrong. */
	template <typename T>
	T& Prop(UObject* Object, const TCHAR* Name)
	{
		FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), Name);
		checkf(Property, TEXT("Test helper: %s has no property %s"), *Object->GetClass()->GetName(), Name);
		return *Property->ContainerPtrToValuePtr<T>(Object);
	}

	/** A probe action owned by Outer: logs Label each run, finishes straight away unless bFinishNow is false. */
	UReactiveTestProbe* MakeProbe(UObject* Outer, FName Label, bool bFinishNow = true, bool bSucceed = true);

	/** A reaction: Event, then Actions in order, with Rules. */
	FReaction MakeReaction(const FGameplayTag& Event, const TArray<UReactiveAction*>& Actions,
		const TArray<UReactiveRule*>& Rules = {}, EReactiveRetriggerPolicy Retrigger = EReactiveRetriggerPolicy::Ignore);

	/** Clears the probe run log. Call at the start of every test. */
	void ResetLog();
}

/** A game world for one test. Destroyed when it goes out of scope. */
class FReactiveTestWorld
{
public:
	FReactiveTestWorld();
	~FReactiveTestWorld();

	UWorld* GetWorld() const { return World; }
	UReactionSubsystem* Subsystem() const;

	/** A plain actor that has begun play. */
	AActor* SpawnActor() const;

	/**
	 * A Reaction Component on Actor, not registered yet: add reactions with AddReaction (actions made with the
	 * component as their owner), then call Start to register it, which begins play on it.
	 */
	UReactionComponent* AddComponent(AActor* Actor, const FGameplayTagContainer& IdentityTags = FGameplayTagContainer()) const;
	void AddReaction(UReactionComponent* Component, const FReaction& Reaction) const;
	void Start(UReactionComponent* Component) const;

	/** Moves time forward by Seconds, in Step-sized ticks (timers, Delay, Cooldown). */
	void Tick(float Seconds, float Step = 0.05f) const;

private:
	UWorld* World = nullptr;
};

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Reactive Volume: a placeable box (Place Actors > Reactive), invisible in game. It sends events as
// actors come and go, and keeps track of who's inside so the In Volume requirement can ask
// "is the player in Actor.Room.Kitchen?".
//
//   Enter Event   an actor came in (and passed the Conditions). Instigator: that actor.
//   Leave Event   an actor whose Enter was sent went out again.
//
// Both are optional and go to the Event Scope (World, or an Identity). Target Actor Identity is the easy
// filter: only actors with that identity (e.g. Actor.Player) count at all, for events and for In Volume.
// Conditions narrow the events further (any requirement); In Volume ignores them, it only asks where an
// actor is. Only actors whose collision generates overlap events are seen (pawns do by default).
// Size it with Size (top of the Details panel) or the scale tool; it shows as an orange box in the editor.
// A box component rather than an engine brush volume, so it always has a shape, however it's placed or spawned.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ReactionContext.h"
#include "ReactiveVolume.generated.h"

class UBillboardComponent;
class UBoxComponent;
class UReactiveRequirement;

/**
 * A box that sends events when actors enter or leave it, and tells In Volume requirements
 * who's in it. Invisible in game.
 */
UCLASS(meta = (DisplayName = "Reactive Volume", PrioritizeCategories = "Reactive Volume"),
	HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking))
class REACTIVEACTORS_API AReactiveVolume : public AActor
{
	GENERATED_BODY()

public:
	AReactiveVolume(const FObjectInitializer& ObjectInitializer);

	/** True if Actor is inside this volume right now (it must have Target Actor Identity, if set; Conditions don't matter). */
	UFUNCTION(BlueprintPure, Category = "Reactive Volume")
	bool IsActorInside(const AActor* Actor) const;

	/** Every actor inside this volume right now. */
	UFUNCTION(BlueprintPure, Category = "Reactive Volume")
	TArray<AActor*> GetActorsInside() const;

	/** The tags naming this volume for In Volume requirements. */
	UFUNCTION(BlueprintPure, Category = "Reactive Volume")
	FGameplayTagContainer GetVolumeIdentityTags() const { return VolumeIdentityTags; }

	/**
	 * C++: is Actor inside any loaded Reactive Volume with Identity, or a child of it (a volume tagged
	 * Actor.Room.Kitchen counts for Actor.Room)? bOutFoundVolume: whether any such volume is loaded at all.
	 */
	static bool IsActorInVolumeWithIdentity(const AActor* Actor, const FGameplayTag& Identity, bool& bOutFoundVolume);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	/** Applies Size to the box (placing, moving and editing in the level, and spawning). */
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	/** Map Check: the same problems as Data Validation. */
	virtual void CheckForErrors() override;

	/** Data Validation: no events and no identity, Identity scope with no identity, condition problems. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	void GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const;
#endif

#if WITH_EDITORONLY_DATA
	/** Editor icon, so the volume can be found and clicked. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif

	/** The zone: overlaps only, nothing collides with it. Sized by Size. */
	UPROPERTY(BlueprintReadOnly, Category = "Reactive Volume")
	TObjectPtr<UBoxComponent> Box;

	/** How big the box is, in cm from the centre to each side (half its width, depth and height). The scale tool works too. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Volume", meta = (ClampMin = 1, UIMin = 1, Units = "cm"))
	FVector Size = FVector(200.f, 200.f, 100.f);

	/** Names this volume for In Volume requirements, e.g. Actor.Room.Kitchen. Parent tags match too (Actor.Room). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Volume", meta = (ReactiveTags = "Identity"))
	FGameplayTagContainer VolumeIdentityTags;

	/**
	 * Optional: only actors with this identity (or a child of it) count, e.g. Actor.Player. Others are ignored
	 * entirely: no events, and they're never inside for In Volume. The identity comes from the actor's
	 * Reaction Component (Identity Tags). Empty: every actor that overlaps counts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Volume", meta = (ReactiveTags = "Identity"))
	FGameplayTag TargetActorIdentity;

	/** Optional: sent when an actor comes in and passes the Conditions. Instigator: that actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Volume", meta = (ReactiveTags = "Event"))
	FGameplayTag EnterEvent;

	/** Optional: sent when an actor whose Enter Event was sent goes out again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Volume", meta = (ReactiveTags = "Event"))
	FGameplayTag LeaveEvent;

	/** World: everyone listening hears the events. Identity: only actors with Target Identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Volume", meta = (DisplayName = "Event Scope", InvalidEnumValues = "Self"))
	EReactiveScopePolicy Scope = EReactiveScopePolicy::World;

	/** Identity scope: send to actors with this identity, or a child of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive Volume",
		meta = (ReactiveTags = "Identity", EditCondition = "Scope == EReactiveScopePolicy::Identity", EditConditionHides))
	FGameplayTag TargetIdentity;

	/**
	 * Which actors the events are about: all must pass (e.g. Is Class: your player pawn). Each check sees the
	 * actor coming in as both Target and Instigator. Empty: every actor that overlaps.
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Reactive Volume", meta = (TitleProperty = "Summary"))
	TArray<TObjectPtr<UReactiveRequirement>> Conditions;

private:
	/** Actors overlapping when play began count as coming in. */
	void CatchUpExistingOverlaps();

	void HandleEnter(AActor* Actor);
	void HandleLeave(AActor* Actor);

	bool PassesConditions(AActor* Actor) const;

	/** True if Target Actor Identity is empty, or Actor's Reaction Component has it (or a child of it). */
	bool HasTargetActorIdentity(const AActor* Actor) const;

	void SendVolumeEvent(const FGameplayTag& Event, AActor* EventInstigator);

	/** Every actor overlapping right now (In Volume reads this). */
	TSet<TWeakObjectPtr<AActor>> Inside;

	/** The actors inside that passed the Conditions (their Enter Event was sent): only these send Leave Event. */
	TSet<TWeakObjectPtr<AActor>> Entered;

	FTimerHandle CatchUpHandle;
};

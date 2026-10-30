// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions
//
// World: actions that change actors and the level. Set Actor Hidden (Hide, Show or Toggle), Launch Actor.
// Set Data Layer State lives in the Reactive Actors Extras plugin.
// NotBlueprintable keeps them out of the "pick parent class" list; they still appear in every Actions dropdown.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/TimerHandle.h"
#include "ReactiveAction.h"
#include "WorldActions.generated.h"

// ---- Set Actor Hidden ---------------------------------------------------------------------------

/** What Set Actor Hidden does to the target. */
UENUM(BlueprintType)
enum class EReactiveHiddenChange : uint8
{
	Hide,
	Show,
	/** Hidden becomes shown, shown becomes hidden. */
	Toggle,
};

/** Hides, shows or toggles the target actor, optionally switching its collision to match. */
UCLASS(NotBlueprintable, meta = (DisplayName = "Set Actor Hidden"))
class REACTIVEACTORS_API UACT_SetActorHidden : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	/** Hide, Show, or Toggle (flip whichever it is now). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	EReactiveHiddenChange Change = EReactiveHiddenChange::Hide;

	/** Before Toggle was folded in this was a tick box. Read once on load (false = Show), never saved again. */
	UPROPERTY(meta = (DeprecatedProperty))
	bool bHidden_DEPRECATED = true;

	/** Also disable collision while hidden (and enable it when shown). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bUpdateCollision = true;
};

// ---- Toggle Actor Hidden (deprecated) -----------------------------------------------------------

/**
 * Deprecated: use Set Actor Hidden with Change set to Toggle. Kept (hidden from the dropdowns) so existing
 * reactions still load and work.
 */
UCLASS(NotBlueprintable, HideDropdown, meta = (DisplayName = "Toggle Actor Hidden (deprecated)"))
class REACTIVEACTORS_API UACT_ToggleActorHidden : public UReactiveAction
{
	GENERATED_BODY()

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;

	/** Also disable collision while hidden (and enable it when shown). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bUpdateCollision = true;
};

// ---- Launch Actor -------------------------------------------------------------------------------

/** Which way Launch Actor sends the target. */
UENUM(BlueprintType)
enum class EReactiveLaunchDirection : uint8
{
	/** Away from whoever caused it (knocked away from the player). Falls back to the target's forward with no instigator. */
	AwayFromInstigator UMETA(DisplayName = "Away From Instigator"),
	/** Towards whoever caused it (pulled towards the player). */
	TowardsInstigator UMETA(DisplayName = "Towards Instigator"),
	/** A direction in the target's own space: X forward, Y right, Z up. Turns with the actor. */
	TargetRelative UMETA(DisplayName = "Relative to Target"),
	/** A fixed direction in the world. */
	World UMETA(DisplayName = "World Direction"),
};

/**
 * Launches the target with a burst of speed: a physics prop flies off, a character is knocked back.
 * Physics props get the speed straight away whatever their mass (a vase and a chair go equally far), plus optional spin.
 * Characters are launched with their movement (Launch Character). Props that aren't simulating physics yet can be
 * switched on, and switched off again once they've come to rest. Finishes straight away, or with Wait Until Settled,
 * once it has landed and stopped.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Launch Actor"))
class REACTIVEACTORS_API UACT_LaunchActor : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

	/**
	 * C++ (and tests): the direction to launch in, before Spread. Away / Towards Instigator are flattened to the ground
	 * and tilted up by Lift degrees, so things arc rather than skid along the floor or fly into it.
	 */
	static FVector ComputeDirection(EReactiveLaunchDirection Mode, const FTransform& TargetTransform, const FVector* InstigatorLocation,
		const FVector& RelativeDirection, const FVector& WorldDirection, float LiftDegrees);

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	EReactiveLaunchDirection Direction = EReactiveLaunchDirection::AwayFromInstigator;

	/** Relative to Target: X forward, Y right, Z up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action",
		meta = (ExposeOnSpawn, EditCondition = "Direction == EReactiveLaunchDirection::TargetRelative", EditConditionHides))
	FVector RelativeDirection = FVector(1.f, 0.f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action",
		meta = (ExposeOnSpawn, EditCondition = "Direction == EReactiveLaunchDirection::World", EditConditionHides))
	FVector WorldDirection = FVector(0.f, 0.f, 1.f);

	/** Away / Towards Instigator: how far above level it's launched. 0 = along the ground, 45 = a lob, 90 = straight up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0, ClampMax = 90, UIMin = 0, UIMax = 90, Units = "Degrees",
		EditCondition = "Direction == EReactiveLaunchDirection::AwayFromInstigator || Direction == EReactiveLaunchDirection::TowardsInstigator", EditConditionHides))
	float Lift = 25.f;

	/** How fast it leaves. 300 is a nudge, 800 a good shove, 2000 a blast. Ignores mass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0, UIMin = 0, UIMax = 3000, Units = "CentimetersPerSecond"))
	float Speed = 800.f;

	/** Random variation in direction, so repeated launches don't look identical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0, ClampMax = 45, UIMin = 0, UIMax = 45, Units = "Degrees"))
	float Spread = 5.f;

	/** Physics props: random tumble as it flies, in degrees per second. 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0, UIMin = 0, UIMax = 1080))
	float Spin = 0.f;

	/**
	 * A prop that isn't simulating physics yet is switched on: made Movable if it's Static, given physics collision if
	 * it has none, and set simulating. Its mesh needs a collision shape. Off: only props already simulating are launched.
	 * A prop with baked lighting leaves its baked shadow behind when it moves; use Movable or Stationary lights on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, DisplayName = "Start Physics If Needed"))
	bool bStartPhysics = true;

	/**
	 * Props: once it has landed and come to rest, physics is switched off so it stays exactly where it fell (cheaper,
	 * it won't be nudged about afterwards, and save systems see one fixed position). Whatever Start Physics If Needed
	 * changed is put back: Static again if it was Static, and its old collision. Characters aren't affected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bStopPhysicsWhenSettled = false;

	/** The reaction waits here until it has landed and come to rest (characters: until they're on the ground). Off: carries on straight away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bWaitUntilSettled = false;

	/** Counts as settled after this long even if it's still rocking or rolling. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0, UIMin = 0, UIMax = 30, Units = "s",
		EditCondition = "bStopPhysicsWhenSettled || bWaitUntilSettled"))
	float MaxSettleTime = 10.f;

	virtual void OnCancel_Implementation() override;
	virtual void OnCleanup_Implementation() override;

private:
	/** Watches what was launched until it has come to rest, then stops its physics and/or finishes. */
	void StartSettleWatch(AActor* Target, class UPrimitiveComponent* Body);
	void CheckSettled();
	void Settled();
	void StopSettleWatch();

	TWeakObjectPtr<AActor> SettleActor;
	TWeakObjectPtr<class UPrimitiveComponent> SettleBody;
	FTimerHandle SettleTimer;
	float SettleElapsed = 0.f;
	float StillFor = 0.f;

	/** Put back when it settles: only what Start Physics If Needed changed. */
	bool bRestoreStatic = false;
	bool bRestoreCollision = false;
	TEnumAsByte<ECollisionEnabled::Type> CollisionBeforeLaunch = ECollisionEnabled::QueryAndPhysics;
};

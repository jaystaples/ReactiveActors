// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions: world

#include "Actions/WorldActions.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/PrimitiveComponent.h"
#include "ReactiveActorsLog.h"

// ---- Set Actor Hidden ---------------------------------------------------------------------------

void UACT_SetActorHidden::PostLoad()
{
	Super::PostLoad();

	// Saved before Toggle was folded in, with Hidden unticked: that meant Show. (Hidden ticked was the default,
	// so it was never saved and Change's default, Hide, already matches.)
	if (!bHidden_DEPRECATED)
	{
		Change = EReactiveHiddenChange::Show;
		bHidden_DEPRECATED = true;
	}
}

void UACT_SetActorHidden::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	AActor* Target = Context.Target; // never null here
	const bool bHidden = Change == EReactiveHiddenChange::Toggle ? !Target->IsHidden() : Change == EReactiveHiddenChange::Hide;
	Target->SetActorHiddenInGame(bHidden);
	if (bUpdateCollision)
	{
		Target->SetActorEnableCollision(!bHidden);
	}
	FinishAction();
}

// ---- Toggle Actor Hidden ------------------------------------------------------------------------

void UACT_ToggleActorHidden::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	AActor* Target = Context.Target; // never null here
	const bool bHidden = !Target->IsHidden();
	Target->SetActorHiddenInGame(bHidden);
	if (bUpdateCollision)
	{
		Target->SetActorEnableCollision(!bHidden);
	}
	FinishAction();
}

// ---- Summaries (the row titles in the editor) ---------------------------------------------------

FString UACT_SetActorHidden::DescribeAction() const
{
	const TCHAR* Verb = Change == EReactiveHiddenChange::Toggle ? TEXT("Toggle") : Change == EReactiveHiddenChange::Hide ? TEXT("Hide") : TEXT("Show");
	return FString::Printf(TEXT("%s actor%s"), Verb,
		bUpdateCollision ? TEXT(" and collision") : TEXT(""));
}

// ---- Launch Actor -------------------------------------------------------------------------------

FVector UACT_LaunchActor::ComputeDirection(EReactiveLaunchDirection Mode, const FTransform& TargetTransform, const FVector* InstigatorLocation,
	const FVector& InRelativeDirection, const FVector& InWorldDirection, float LiftDegrees)
{
	const FVector Forward = TargetTransform.GetRotation().GetForwardVector();
	const FVector TargetLocation = TargetTransform.GetLocation();

	switch (Mode)
	{
	case EReactiveLaunchDirection::TargetRelative:
		return TargetTransform.TransformVectorNoScale(InRelativeDirection).GetSafeNormal(UE_SMALL_NUMBER, Forward);

	case EReactiveLaunchDirection::World:
		return InWorldDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);

	default:
		break;
	}

	// Away from / towards the instigator, seen from above (height differences would otherwise fling things into the
	// floor or the sky), then tilted up by Lift so it arcs. No instigator, or it's the target itself: the target's forward.
	FVector Flat = Forward;
	if (InstigatorLocation)
	{
		const FVector Between = Mode == EReactiveLaunchDirection::AwayFromInstigator
			? TargetLocation - *InstigatorLocation
			: *InstigatorLocation - TargetLocation;
		const FVector BetweenFlat(Between.X, Between.Y, 0.f);
		if (!BetweenFlat.IsNearlyZero(1.f))
		{
			Flat = BetweenFlat;
		}
	}
	Flat.Z = 0.f;
	if (Flat.IsNearlyZero())
	{
		return FVector::UpVector; // facing straight up or down: up is the only sensible way
	}

	const float LiftRadians = FMath::DegreesToRadians(FMath::Clamp(LiftDegrees, 0.f, 90.f));
	return (Flat.GetSafeNormal() * FMath::Cos(LiftRadians) + FVector::UpVector * FMath::Sin(LiftRadians)).GetSafeNormal();
}

void UACT_LaunchActor::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	AActor* Target = Context.Target; // never null here
	const AActor* Instigator = Context.Instigator;
	FVector InstigatorLocation = FVector::ZeroVector;
	const bool bHasInstigator = IsValid(Instigator) && Instigator != Target;
	if (bHasInstigator)
	{
		InstigatorLocation = Instigator->GetActorLocation();
	}

	FVector Dir = ComputeDirection(Direction, Target->GetActorTransform(), bHasInstigator ? &InstigatorLocation : nullptr,
		RelativeDirection, WorldDirection, Lift);
	if (Spread > 0.f)
	{
		Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(Spread));
	}
	const FVector Velocity = Dir * Speed;

	// Characters: their movement launches them (knockback), like Launch Character in Blueprint
	if (ACharacter* Character = Cast<ACharacter>(Target))
	{
		Character->LaunchCharacter(Velocity, /*bXYOverride*/ true, /*bZOverride*/ true);
		if (bWaitUntilSettled)
		{
			StartSettleWatch(Character, nullptr); // finishes once they're back on the ground
			return;
		}
		FinishAction();
		return;
	}

	// Props: the root if it's a primitive (a Static Mesh Actor's mesh), otherwise the first one already simulating, or the first one
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	if (!Body)
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Target);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive && Primitive->IsSimulatingPhysics())
			{
				Body = Primitive;
				break;
			}
		}
		if (!Body && Primitives.Num() > 0)
		{
			Body = Primitives[0];
		}
	}
	if (!Body)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("Launch Actor on %s: nothing to launch (no mesh or other primitive component)."), *GetNameSafe(Target));
		FinishAction();
		return;
	}

	// What Start Physics If Needed changes, so Stop Physics When Settled can put it back
	bool bChangedToMovable = false;
	bool bChangedCollision = false;
	const ECollisionEnabled::Type CollisionBefore = Body->GetCollisionEnabled();

	if (!Body->IsSimulatingPhysics())
	{
		if (!bStartPhysics)
		{
			UE_LOG(LogReactiveActors, Warning, TEXT("Launch Actor on %s: it isn't simulating physics and Start Physics If Needed is off."), *GetNameSafe(Target));
			FinishAction();
			return;
		}
		if (Body->Mobility != EComponentMobility::Movable)
		{
			Body->SetMobility(EComponentMobility::Movable);
			bChangedToMovable = true;
		}
		if (CollisionBefore != ECollisionEnabled::QueryAndPhysics && CollisionBefore != ECollisionEnabled::PhysicsOnly)
		{
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			bChangedCollision = true;
		}
		Body->SetSimulatePhysics(true);
		if (!Body->IsSimulatingPhysics())
		{
			UE_LOG(LogReactiveActors, Warning, TEXT("Launch Actor on %s: %s couldn't start simulating physics (does its mesh have a collision shape?)."),
				*GetNameSafe(Target), *Body->GetName());
			// Put back what was changed: it isn't going anywhere
			if (bChangedCollision) Body->SetCollisionEnabled(CollisionBefore);
			if (bChangedToMovable) Body->SetMobility(EComponentMobility::Static);
			FinishAction();
			return;
		}
	}

	// Speed, not force: the same launch looks the same on a vase and a chair
	Body->WakeAllRigidBodies();
	Body->AddImpulse(Velocity, NAME_None, /*bVelChange*/ true);
	if (Spin > 0.f)
	{
		Body->AddAngularImpulseInDegrees(FMath::VRand() * Spin, NAME_None, /*bVelChange*/ true);
	}

	if (bStopPhysicsWhenSettled || bWaitUntilSettled)
	{
		// Launched again before it settled: it's still simulating, so keep what it was before the first launch
		if (SettleBody.Get() != Body)
		{
			bRestoreStatic = bChangedToMovable;
			bRestoreCollision = bChangedCollision;
			CollisionBeforeLaunch = CollisionBefore;
		}
		StartSettleWatch(Target, Body);
	}
	if (!bWaitUntilSettled)
	{
		FinishAction(); // the settle watch carries on by itself
	}
}

// Settled: barely moving for this long (checked every SettleInterval), or asleep
static constexpr float SettleInterval = 0.1f;
static constexpr float SettleStillTime = 0.5f;
static constexpr float SettleMaxSpeed = 5.f;            // cm/s
static constexpr float SettleMaxSpin = 20.f;            // degrees/s

void UACT_LaunchActor::StartSettleWatch(AActor* Target, UPrimitiveComponent* Body)
{
	StopSettleWatch(); // a new launch replaces the last watch
	SettleActor = Target;
	SettleBody = Body;
	SettleElapsed = 0.f;
	StillFor = 0.f;
	if (UWorld* World = GetWorld())
	{
		// The first check waits a moment, so it has left the ground before "not moving" can count
		World->GetTimerManager().SetTimer(SettleTimer, FTimerDelegate::CreateUObject(this, &UACT_LaunchActor::CheckSettled),
			SettleInterval, /*bLoop*/ true, /*FirstDelay*/ 0.3f);
	}
}

void UACT_LaunchActor::CheckSettled()
{
	SettleElapsed += SettleInterval;

	bool bStill = true;
	if (const ACharacter* Character = Cast<ACharacter>(SettleActor.Get()))
	{
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		bStill = !Movement || Movement->IsMovingOnGround();
	}
	else if (UPrimitiveComponent* Body = SettleBody.Get()) // not const: the engine's velocity getters aren't
	{
		bStill = !Body->IsSimulatingPhysics() || !Body->IsAnyRigidBodyAwake()
			|| (Body->GetPhysicsLinearVelocity().Size() < SettleMaxSpeed && Body->GetPhysicsAngularVelocityInDegrees().Size() < SettleMaxSpin);
	}
	// (the actor or body gone: counts as settled, so a waiting reaction carries on)

	StillFor = bStill ? StillFor + SettleInterval : 0.f;
	if (StillFor >= SettleStillTime || (MaxSettleTime > 0.f && SettleElapsed >= MaxSettleTime))
	{
		Settled();
	}
}

void UACT_LaunchActor::Settled()
{
	UPrimitiveComponent* Body = SettleBody.Get();
	StopSettleWatch();

	if (bStopPhysicsWhenSettled && Body && Body->IsSimulatingPhysics())
	{
		Body->SetSimulatePhysics(false); // stays where it came to rest
		if (bRestoreCollision)
		{
			Body->SetCollisionEnabled(CollisionBeforeLaunch);
		}
		if (bRestoreStatic)
		{
			Body->SetMobility(EComponentMobility::Static); // as placed (its children stay Movable)
		}
	}
	bRestoreStatic = false;
	bRestoreCollision = false;
	if (bWaitUntilSettled && IsRunning())
	{
		FinishAction();
	}
}

void UACT_LaunchActor::StopSettleWatch()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettleTimer);
	}
	SettleActor.Reset();
	SettleBody.Reset();
}

void UACT_LaunchActor::OnCancel_Implementation()
{
	// The reaction was stopped while waiting: stop waiting, but still let the prop come to rest and freeze if asked
	if (!bStopPhysicsWhenSettled)
	{
		StopSettleWatch();
	}
}

void UACT_LaunchActor::OnCleanup_Implementation()
{
	StopSettleWatch();
}

FString UACT_LaunchActor::DescribeAction() const
{
	FString Where;
	switch (Direction)
	{
	case EReactiveLaunchDirection::AwayFromInstigator: Where = FString::Printf(TEXT("away from instigator, %.0f deg up"), Lift); break;
	case EReactiveLaunchDirection::TowardsInstigator: Where = FString::Printf(TEXT("towards instigator, %.0f deg up"), Lift); break;
	case EReactiveLaunchDirection::TargetRelative: Where = FString::Printf(TEXT("relative (%.1f, %.1f, %.1f)"), RelativeDirection.X, RelativeDirection.Y, RelativeDirection.Z); break;
	default: Where = FString::Printf(TEXT("world (%.1f, %.1f, %.1f)"), WorldDirection.X, WorldDirection.Y, WorldDirection.Z); break;
	}
	return FString::Printf(TEXT("Launch %s at %.0f cm/s%s"), *Where, Speed, bWaitUntilSettled ? TEXT(", wait to land") : TEXT(""));
}

void UACT_LaunchActor::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (Speed <= 0.f)
	{
		Warnings.Add(INVTEXT("Speed is 0, so nothing moves."));
	}
	if (Direction == EReactiveLaunchDirection::TargetRelative && RelativeDirection.IsNearlyZero())
	{
		Errors.Add(INVTEXT("Relative Direction is zero, so there's no direction to launch in."));
	}
	if (Direction == EReactiveLaunchDirection::World && WorldDirection.IsNearlyZero())
	{
		Errors.Add(INVTEXT("World Direction is zero, so there's no direction to launch in."));
	}
}

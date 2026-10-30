// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in requirements
//
// Generic checks that ship with the plugin. NotBlueprintable keeps them out of the
// "pick parent class" list; they still appear in every Requirements dropdown.
// Requirements are stateless and shared per action class: always use the Context passed in.
// Each has a Check setting: test the target (default) or the instigator ("only if the player did it").
// "The actor" below means whichever one Check picks.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ReactiveRequirement.h"
#include "BuiltInRequirements.generated.h"

class UActorComponent;

/** Internal base for the built-ins: the Check setting, and the actor it picks. */
UCLASS(Abstract, NotBlueprintable)
class REACTIVEACTORS_API UREQ_ActorCheckBase : public UReactiveRequirement
{
	GENERATED_BODY()

public:
	virtual bool ChecksInstigator() const override { return Check == EReactiveRequirementSubject::Instigator; }

	/** "Has Actor Tags: Player, Lit (any) on Instigator". */
	virtual FString DescribeRequirement() const override;

protected:
	/** The actor Check picks from Context. False (with a fail reason) if it isn't there. */
	bool GetCheckedActor(const FReactionContext& Context, const AActor*& OutActor, FText& FailReason) const;

	/** C++: the configured entries for the summary, and whether all are needed. */
	virtual void DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const {}

	/** Which actor to test: the target (the actor being acted on) or the instigator (who caused it). */
	UPROPERTY(EditAnywhere, Category = "Requirement")
	EReactiveRequirementSubject Check = EReactiveRequirementSubject::Target;
};

/** The actor must have components of the listed classes (all of them, or any one). */
UCLASS(NotBlueprintable, meta = (DisplayName = "Has Components"))
class REACTIVEACTORS_API UREQ_HasComponents : public UREQ_ActorCheckBase
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual bool CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const override;
	virtual void DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const override;

	/** Component classes to look for on the actor (e.g. Light Component). */
	UPROPERTY(EditAnywhere, Category = "Requirement", meta = (AllowAbstract = "true"))
	TArray<TSubclassOf<UActorComponent>> Components;

	/** On: the actor needs every listed component. Off: any one is enough. */
	UPROPERTY(EditAnywhere, Category = "Requirement")
	bool bRequireAll = true;
};

/** The actor must implement the listed interfaces (all of them, or any one). */
UCLASS(NotBlueprintable, meta = (DisplayName = "Implements Interfaces"))
class REACTIVEACTORS_API UREQ_ImplementsInterfaces : public UREQ_ActorCheckBase
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual bool CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const override;
	virtual void DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const override;

	/** Interfaces the actor must implement (C++ or Blueprint interfaces). */
	UPROPERTY(EditAnywhere, Category = "Requirement", meta = (AllowAbstract = "true"))
	TArray<TSubclassOf<UInterface>> Interfaces;

	/** On: the actor needs every listed interface. Off: any one is enough. */
	UPROPERTY(EditAnywhere, Category = "Requirement")
	bool bRequireAll = true;
};

/** The actor must have the listed actor tags (all of them, or any one). */
UCLASS(NotBlueprintable, meta = (DisplayName = "Has Actor Tags"))
class REACTIVEACTORS_API UREQ_HasActorTags : public UREQ_ActorCheckBase
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual bool CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const override;
	virtual void DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const override;

	/** Actor tags (Details > Actor > Tags) the actor must have. */
	UPROPERTY(EditAnywhere, Category = "Requirement")
	TArray<FName> Tags;

	/** On: the actor needs every listed tag. Off: any one is enough. */
	UPROPERTY(EditAnywhere, Category = "Requirement")
	bool bRequireAll = true;
};

/** The actor must be one of the listed classes, or a child of one ("is a BP_Door"). */
UCLASS(NotBlueprintable, meta = (DisplayName = "Is Class"))
class REACTIVEACTORS_API UREQ_IsClass : public UREQ_ActorCheckBase
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual bool CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const override;
	virtual void DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const override;

	/** The actor must be any one of these classes (children count: BP_Door also passes BP_DoorBase). */
	UPROPERTY(EditAnywhere, Category = "Requirement")
	TArray<TSubclassOf<AActor>> Classes;
};

/**
 * The actor's Reaction Component must have this identity, or a child of it: an actor with
 * Actor.Kitchen.Lamp passes Actor.Kitchen (the same match as sending an event to an identity).
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Has Identity"))
class REACTIVEACTORS_API UREQ_HasIdentity : public UREQ_ActorCheckBase
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual bool CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const override;
	virtual void DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const override;

	/** The identity the actor must have (or a child of it). */
	UPROPERTY(EditAnywhere, Category = "Requirement", meta = (ReactiveTags = "Identity"))
	FGameplayTag Identity;
};

/**
 * The actor must be inside a Reactive Volume with this identity, or a child of it: a volume tagged
 * Actor.Room.Kitchen counts for Actor.Room ("only if the player is upstairs"). Checks the instigator by default.
 * Only actors whose collision generates overlap events are tracked (pawns do by default).
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "In Volume"))
class REACTIVEACTORS_API UREQ_InVolume : public UREQ_ActorCheckBase
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

public:
	UREQ_InVolume();

	/** Where actors are changes all the time: nothing for Map Check to test ahead of time. */
	virtual bool CanCheckAhead() const override { return false; }

protected:
	virtual bool CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const override;
	virtual void DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const override;

	/** A Reactive Volume identity, e.g. Actor.Room.Kitchen. Any loaded volume with it (or a child of it) counts. */
	UPROPERTY(EditAnywhere, Category = "Requirement", meta = (ReactiveTags = "Identity"))
	FGameplayTag Volume;
};

/** Passes by chance, rolled fresh every time it's checked. "Only creak half the time." */
UCLASS(NotBlueprintable, meta = (DisplayName = "Random Chance"))
class REACTIVEACTORS_API UREQ_RandomChance : public UReactiveRequirement
{
	GENERATED_BODY()

public:
	/** A dice roll: nothing for Map Check to test ahead of time. */
	virtual bool CanCheckAhead() const override { return false; }

protected:
	virtual bool CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const override;
	virtual FString DescribeRequirement() const override;

	/** How likely it is to pass: 0 never, 100 always. */
	UPROPERTY(EditAnywhere, Category = "Requirement", meta = (ClampMin = 0, ClampMax = 100, UIMin = 0, UIMax = 100, Units = "Percent"))
	float Chance = 50.f;
};

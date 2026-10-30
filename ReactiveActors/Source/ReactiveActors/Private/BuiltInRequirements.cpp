// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in requirements

#include "BuiltInRequirements.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "ReactionComponent.h"
#include "ReactiveVolume.h"

namespace BuiltInRequirements
{
	/**
	 * Shared all/any logic. Returns true if the requirement passes.
	 * Empty or unset entries are ignored; with nothing configured the requirement passes.
	 */
	template <typename EntryType, typename HasFunc, typename NameFunc>
	bool Evaluate(const AActor* Target, const TArray<EntryType>& Entries, bool bRequireAll,
	HasFunc Has, NameFunc GetEntryName, const FText& WhatIsMissing, FText& FailReason)
	{
		if (!Target)
		{
			FailReason = INVTEXT("No target actor.");
			return false;
		}

		TArray<FString> Missing;
		int32 Configured = 0;

		for (const EntryType& Entry : Entries)
		{
			const FString Name = GetEntryName(Entry);
			if (Name.IsEmpty())
			{
				continue; // unset entry
			}
			++Configured;

			if (Has(Entry))
			{
				if (!bRequireAll)
				{
					return true; // any one is enough
				}
			}
			else
			{
				Missing.Add(Name);
			}
		}

		if (Configured == 0 || (bRequireAll && Missing.IsEmpty()))
		{
			return true;
		}

		FailReason = FText::Format(INVTEXT("{0} is missing {1} ({2}): {3}"),
			FText::FromString(Target->GetActorNameOrLabel()),
			WhatIsMissing,
			bRequireAll ? INVTEXT("needs all") : INVTEXT("needs any"),
			FText::FromString(FString::Join(Missing, TEXT(", "))));
		return false;
	}
}

bool UREQ_ActorCheckBase::GetCheckedActor(const FReactionContext& Context, const AActor*& OutActor, FText& FailReason) const
{
	const bool bInstigator = Check == EReactiveRequirementSubject::Instigator;
	OutActor = bInstigator ? Context.Instigator.Get() : Context.Target.Get();
	if (!OutActor)
	{
		FailReason = bInstigator ? INVTEXT("No instigator to check.") : INVTEXT("No target actor.");
		return false;
	}
	return true;
}

bool UREQ_HasComponents::CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const
{
	const AActor* Actor = nullptr;
	if (!GetCheckedActor(Context, Actor, FailReason))
	{
		return false;
	}
	return BuiltInRequirements::Evaluate(Actor, Components, bRequireAll,
		[Actor](const TSubclassOf<UActorComponent>& Class) { return Actor->FindComponentByClass(Class) != nullptr; },
		[](const TSubclassOf<UActorComponent>& Class) { return Class ? Class->GetName() : FString(); },
		INVTEXT("component(s)"), FailReason);
}

bool UREQ_ImplementsInterfaces::CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const
{
	const AActor* Actor = nullptr;
	if (!GetCheckedActor(Context, Actor, FailReason))
	{
		return false;
	}
	return BuiltInRequirements::Evaluate(Actor, Interfaces, bRequireAll,
		[Actor](const TSubclassOf<UInterface>& Interface) { return Actor->GetClass()->ImplementsInterface(Interface); },
		[](const TSubclassOf<UInterface>& Interface) { return Interface ? Interface->GetName() : FString(); },
		INVTEXT("interface(s)"), FailReason);
}

bool UREQ_HasActorTags::CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const
{
	const AActor* Actor = nullptr;
	if (!GetCheckedActor(Context, Actor, FailReason))
	{
		return false;
	}
	return BuiltInRequirements::Evaluate(Actor, Tags, bRequireAll,
		[Actor](const FName& Tag) { return Actor->ActorHasTag(Tag); },
		[](const FName& Tag) { return Tag.IsNone() ? FString() : Tag.ToString(); },
		INVTEXT("actor tag(s)"), FailReason);
}

bool UREQ_IsClass::CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const
{
	const AActor* Actor = nullptr;
	if (!GetCheckedActor(Context, Actor, FailReason))
	{
		return false;
	}

	TArray<FString> Names;
	for (const TSubclassOf<AActor>& Class : Classes)
	{
		if (Class)
		{
			if (Actor->IsA(Class))
			{
				return true;
			}
			Names.Add(Class->GetDisplayNameText().ToString());
		}
	}
	if (Names.IsEmpty())
	{
		return true; // nothing configured
	}

	FailReason = FText::Format(INVTEXT("{0} is a {1}, not {2}."),
		FText::FromString(Actor->GetActorNameOrLabel()), Actor->GetClass()->GetDisplayNameText(),
		FText::FromString(FString::Join(Names, TEXT(" or "))));
	return false;
}

bool UREQ_HasIdentity::CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const
{
	const AActor* Actor = nullptr;
	if (!GetCheckedActor(Context, Actor, FailReason))
	{
		return false;
	}
	if (!Identity.IsValid())
	{
		return true; // nothing configured
	}

	TInlineComponentArray<UReactionComponent*> Components(Actor);
	for (const UReactionComponent* Component : Components)
	{
		// HasTag: the component's Actor.Kitchen.Lamp counts as Actor.Kitchen
		if (Component && Component->GetIdentityTags().HasTag(Identity))
		{
			return true;
		}
	}

	FailReason = FText::Format(INVTEXT("{0} doesn't have identity {1}."),
		FText::FromString(Actor->GetActorNameOrLabel()), FText::FromString(Identity.ToString()));
	return false;
}

UREQ_InVolume::UREQ_InVolume()
{
	// Usually about who caused it: "only if the player is in the kitchen"
	Check = EReactiveRequirementSubject::Instigator;
}

bool UREQ_InVolume::CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const
{
	const AActor* Actor = nullptr;
	if (!GetCheckedActor(Context, Actor, FailReason))
	{
		return false;
	}
	if (!Volume.IsValid())
	{
		return true; // nothing configured
	}

	bool bFoundVolume = false;
	if (AReactiveVolume::IsActorInVolumeWithIdentity(Actor, Volume, bFoundVolume))
	{
		return true;
	}

	FailReason = bFoundVolume
		? FText::Format(INVTEXT("{0} isn't inside {1}."), FText::FromString(Actor->GetActorNameOrLabel()), FText::FromString(Volume.ToString()))
		: FText::Format(INVTEXT("No Reactive Volume with identity {0} is loaded."), FText::FromString(Volume.ToString()));
	return false;
}

bool UREQ_RandomChance::CheckRequirement_Implementation(const FReactionContext& Context, FText& FailReason) const
{
	if (FMath::FRand() * 100.f < Chance)
	{
		return true;
	}
	FailReason = FText::Format(INVTEXT("Random Chance {0}% didn't come up."), FText::AsNumber(FMath::RoundToInt(Chance)));
	return false;
}

// ---- Summaries (the row titles in the editor) ---------------------------------------------------

FString UREQ_ActorCheckBase::DescribeRequirement() const
{
	TArray<FString> Entries;
	bool bRequireAll = true;
	DescribeEntries(Entries, bRequireAll);

	FString Out = GetClass()->GetDisplayNameText().ToString();
	if (!Entries.IsEmpty())
	{
		Out += TEXT(": ") + FString::Join(Entries, TEXT(", "));
		if (!bRequireAll && Entries.Num() > 1)
		{
			Out += TEXT(" (any)");
		}
	}
	if (Check == EReactiveRequirementSubject::Instigator)
	{
		Out += TEXT(" on Instigator");
	}
	return Out;
}

void UREQ_HasComponents::DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const
{
	for (const TSubclassOf<UActorComponent>& Class : Components)
	{
		if (Class)
		{
			OutEntries.Add(Class->GetDisplayNameText().ToString());
		}
	}
	bOutRequireAll = bRequireAll;
}

void UREQ_ImplementsInterfaces::DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const
{
	for (const TSubclassOf<UInterface>& Interface : Interfaces)
	{
		if (Interface)
		{
			OutEntries.Add(Interface->GetDisplayNameText().ToString());
		}
	}
	bOutRequireAll = bRequireAll;
}

void UREQ_HasActorTags::DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const
{
	for (const FName& Tag : Tags)
	{
		if (!Tag.IsNone())
		{
			OutEntries.Add(Tag.ToString());
		}
	}
	bOutRequireAll = bRequireAll;
}

void UREQ_IsClass::DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const
{
	for (const TSubclassOf<AActor>& Class : Classes)
	{
		if (Class)
		{
			OutEntries.Add(Class->GetDisplayNameText().ToString());
		}
	}
	bOutRequireAll = false; // any one
}

void UREQ_HasIdentity::DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const
{
	if (Identity.IsValid())
	{
		OutEntries.Add(Identity.ToString());
	}
}

void UREQ_InVolume::DescribeEntries(TArray<FString>& OutEntries, bool& bOutRequireAll) const
{
	if (Volume.IsValid())
	{
		OutEntries.Add(Volume.ToString());
	}
}

FString UREQ_RandomChance::DescribeRequirement() const
{
	return FString::Printf(TEXT("Random Chance %d%%"), FMath::RoundToInt(Chance));
}

// ---- Validate Settings (Data Validation and Map Check) ----------------------------------------

void UREQ_HasIdentity::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Identity.IsValid())
	{
		Warnings.Add(INVTEXT("Identity isn't set, so it always passes."));
	}
}

void UREQ_InVolume::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Volume.IsValid())
	{
		Warnings.Add(INVTEXT("Volume isn't set, so it always passes."));
	}
}

void UREQ_IsClass::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Classes.ContainsByPredicate([](const TSubclassOf<AActor>& Class) { return Class != nullptr; }))
	{
		Warnings.Add(INVTEXT("no classes listed, so it always passes."));
	}
}

void UREQ_HasComponents::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Components.ContainsByPredicate([](const TSubclassOf<UActorComponent>& Class) { return Class != nullptr; }))
	{
		Warnings.Add(INVTEXT("no components listed, so it always passes."));
	}
}

void UREQ_ImplementsInterfaces::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Interfaces.ContainsByPredicate([](const TSubclassOf<UInterface>& Class) { return Class != nullptr; }))
	{
		Warnings.Add(INVTEXT("no interfaces listed, so it always passes."));
	}
}

void UREQ_HasActorTags::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Tags.ContainsByPredicate([](const FName& Tag) { return !Tag.IsNone(); }))
	{
		Warnings.Add(INVTEXT("no tags listed, so it always passes."));
	}
}

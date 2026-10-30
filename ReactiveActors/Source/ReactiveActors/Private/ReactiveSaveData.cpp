// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveSaveData.h"
#include "ReactiveActorsLog.h"
#include "ReactiveObjectBase.h"
#include "Reaction.h"
#include "ReactiveTrigger.h"

#include "Engine/World.h"
#include "UObject/Package.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/CustomVersion.h"
#include "UObject/CoreRedirects.h"
#include "UObject/ObjectVersion.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "ReactiveRequirement.h"

namespace ReactiveSave
{
	bool HasSaveGameVariables(const UClass* Class)
	{
		if (!Class)
		{
			return false;
		}
		// Not cached: a Blueprint can gain a SaveGame variable while the editor is open, and saving is rare enough
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_SaveGame))
			{
				return true;
			}
		}
		return false;
	}

	FReactiveSavedObject SaveObject(UObject* Object, bool bKeepClassWhenEmpty)
	{
		FReactiveSavedObject Saved;
		if (!Object)
		{
			return Saved;
		}

		// Nothing to save: an empty entry keeps its place in the list (and its class, where it's used to match)
		if (!HasSaveGameVariables(Object->GetClass()))
		{
			if (bKeepClassWhenEmpty)
			{
				Saved.ClassPath = Object->GetClass()->GetPathName();
			}
			return Saved;
		}

		// Let it turn anything that can't be saved as-is into SaveGame variables first (Cooldown: world time -> time left)
		if (UReactiveObjectBase* Block = Cast<UReactiveObjectBase>(Object))
		{
			Block->PreSaveState();
		}

		Saved.ClassPath = Object->GetClass()->GetPathName();

		// Unreal's own tagged property saving, SaveGame variables only. Not "persistent", so SaveGame variables
		// that are also Transient (runtime counters that should never be baked into a level) are written too.
		// No delta: every SaveGame variable is written, even ones back at their default, so loading always
		// overwrites the live value.
		FMemoryWriter Writer(Saved.Data, /*bIsPersistent*/ false);
		FObjectAndNameAsStringProxyArchive Archive(Writer, /*bLoadIfFindFails*/ false);
		Archive.ArIsSaveGame = true;
		Archive.ArNoDelta = true;
		Object->Serialize(Archive);

		// The versions it was written with, so a later engine reads it the same way
		const FPackageFileVersion Version = Archive.UEVer();
		Saved.FileVersionUE4 = Version.FileVersionUE4;
		Saved.FileVersionUE5 = Version.FileVersionUE5;
		Saved.FileVersionLicensee = Archive.LicenseeUEVer();

		FCustomVersionContainer Used = Archive.GetCustomVersions();
		if (Used.GetAllVersions().Num() > 0)
		{
			FMemoryWriter VersionWriter(Saved.CustomVersions);
			Used.Serialize(VersionWriter);
		}
		return Saved;
	}

	bool IsSameClass(const UClass* Class, const FString& SavedPath)
	{
		if (!Class || SavedPath.IsEmpty())
		{
			return false;
		}
		const FString CurrentPath = Class->GetPathName();
		if (CurrentPath == SavedPath)
		{
			return true;
		}

		// A C++ class renamed with a Core Redirect
		const FCoreRedirectObjectName Redirected = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, FCoreRedirectObjectName(SavedPath));
		if (Redirected.ToString() == CurrentPath)
		{
			return true;
		}

		// A Blueprint class renamed or moved (it leaves a redirector behind)
		const UClass* Resolved = FSoftClassPath(SavedPath).ResolveClass();
		if (!Resolved)
		{
			Resolved = FSoftClassPath(SavedPath).TryLoadClass<UObject>();
		}
		return Resolved == Class;
	}

	bool LoadObject(UObject* Object, const FReactiveSavedObject& Saved)
	{
		if (!Object || Saved.ClassPath.IsEmpty())
		{
			return false; // nothing was saved for it
		}
		if (!IsSameClass(Object->GetClass(), Saved.ClassPath))
		{
			UE_LOG(LogReactiveActors, Verbose, TEXT("Restore: %s was saved as %s, so its saved state was skipped."),
				*Object->GetClass()->GetName(), *Saved.ClassPath);
			return false;
		}

		if (!Saved.Data.IsEmpty())
		{
			FMemoryReader Reader(Saved.Data, /*bIsPersistent*/ false);
			FObjectAndNameAsStringProxyArchive Archive(Reader, /*bLoadIfFindFails*/ true);
			Archive.ArIsSaveGame = true;
			Archive.ArNoDelta = true;

			// Read it the way it was written (an engine upgrade since may have changed how properties are stored).
			// Set on both: the proxy and the reader under it.
			if (Saved.FileVersionUE4 > 0)
			{
				const FPackageFileVersion Version(Saved.FileVersionUE4, static_cast<EUnrealEngineObjectUE5Version>(Saved.FileVersionUE5));
				Reader.SetUEVer(Version);
				Reader.SetLicenseeUEVer(Saved.FileVersionLicensee);
				Archive.SetUEVer(Version);
				Archive.SetLicenseeUEVer(Saved.FileVersionLicensee);
			}
			if (!Saved.CustomVersions.IsEmpty())
			{
				FCustomVersionContainer Versions;
				FMemoryReader VersionReader(Saved.CustomVersions);
				Versions.Serialize(VersionReader);
				Reader.SetCustomVersions(Versions);
				Archive.SetCustomVersions(Versions);
			}

			Object->Serialize(Archive);
		}

		if (UReactiveObjectBase* Block = Cast<UReactiveObjectBase>(Object))
		{
			Block->PostLoadState();
		}
		return true;
	}

	FString GetWorldKey(const UWorld* World)
	{
		if (!World)
		{
			return FString();
		}
		const UPackage* Package = World->GetOutermost();
		return UWorld::RemovePIEPrefix(Package ? Package->GetName() : World->GetName());
	}

#if WITH_EDITOR
	bool EnsureIds(TArray<FReaction>& Reactions, TArray<TObjectPtr<UReactiveTrigger>>& Triggers)
	{
		bool bChanged = false;

		TSet<FGuid> Seen;
		for (FReaction& Reaction : Reactions)
		{
			bool bAlreadySeen = false;
			if (Reaction.ReactionId.IsValid())
			{
				Seen.Add(Reaction.ReactionId, &bAlreadySeen);
			}
			if (!Reaction.ReactionId.IsValid() || bAlreadySeen)
			{
				Reaction.ReactionId = FGuid::NewGuid();
				Seen.Add(Reaction.ReactionId);
				bChanged = true;
			}
		}

		Seen.Reset();
		for (UReactiveTrigger* Trigger : Triggers)
		{
			if (!Trigger)
			{
				continue;
			}
			bool bAlreadySeen = false;
			if (Trigger->TriggerId.IsValid())
			{
				Seen.Add(Trigger->TriggerId, &bAlreadySeen);
			}
			if (!Trigger->TriggerId.IsValid() || bAlreadySeen)
			{
				Trigger->Modify();
				Trigger->TriggerId = FGuid::NewGuid();
				Seen.Add(Trigger->TriggerId);
				bChanged = true;
			}
		}
		return bChanged;
	}

	/** The property itself, or for an array / set / map, what it holds (a map: key and value). */
	static void ForEachHeld(const FProperty* Property, TFunctionRef<void(const FProperty*)> Visit)
	{
		if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			Visit(Array->Inner);
		}
		else if (const FSetProperty* Set = CastField<FSetProperty>(Property))
		{
			Visit(Set->ElementProp);
		}
		else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
		{
			Visit(Map->KeyProp);
			Visit(Map->ValueProp);
		}
		else
		{
			Visit(Property);
		}
	}

	void GatherSaveGameProblems(const UClass* Class, TFunctionRef<void(const FText&)> Warn)
	{
		if (!Class)
		{
			return;
		}

		const bool bRequirement = Class->IsChildOf(UReactiveRequirement::StaticClass());
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame))
			{
				continue;
			}
			const FText Name = Property->GetDisplayNameText();

			if (bRequirement)
			{
				Warn(FText::Format(INVTEXT("{0} has SaveGame ticked, but requirements are never saved (they only check the moment). Untick it, or keep this memory in a Rule instead."), Name));
				continue;
			}

			bool bActorReference = false;
			bool bTimer = false;
			ForEachHeld(Property, [&bActorReference, &bTimer](const FProperty* Held)
				{
					// Hard and weak references (soft ones are paths, and classes aren't actors)
					const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Held);
					if (Object && !CastField<FSoftObjectProperty>(Held) && Object->PropertyClass
						&& (Object->PropertyClass->IsChildOf(AActor::StaticClass()) || Object->PropertyClass->IsChildOf(UActorComponent::StaticClass())))
					{
						bActorReference = true;
					}
					const FStructProperty* Struct = CastField<FStructProperty>(Held);
					if (Struct && Struct->Struct == FTimerHandle::StaticStruct())
					{
						bTimer = true;
					}
				});

			if (bActorReference)
			{
				Warn(FText::Format(INVTEXT("{0} has SaveGame ticked but holds an actor (or component) reference. References don't reliably survive a load: actors in levels that aren't loaded yet, or spawned while playing, come back empty. Save the actor's Identity Tag in a Gameplay Tag variable instead, and find it again with Get Actors By Identity."), Name));
			}
			if (bTimer)
			{
				Warn(FText::Format(INVTEXT("{0} is a Timer Handle with SaveGame ticked. Timers aren't saved, so the handle means nothing after a load. Untick it and let the timer start again after loading, or use the built-in Cooldown rule for waits that should carry over a save."), Name));
			}
		}
	}
#endif
}

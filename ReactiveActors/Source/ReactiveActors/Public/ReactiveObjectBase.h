// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Abstract base for every object authored inline on an actor (actions, requirements, rules, triggers).
// Plumbing only: inline creation, owning actor, world context, designer note.
// Nothing gameplay-specific belongs here.
//
// Note: no DefaultToInstanced. Properties that should OWN these objects (create them inline)
// must be marked Instanced explicitly; any other property of these types is a plain reference.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ReactiveObjectBase.generated.h"

UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class REACTIVEACTORS_API UReactiveObjectBase : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * What owns this object, usually the Reaction Component its reaction is on, or, for Run Action and
	 * friends, the Outer it was made with (an actor, widget...). Cast it, or call an interface on it.
	 * Enclosing actions (Branch, Run On Targets) are skipped.
	 * Usually you want to call Get Target or Get Instigator instead if you're in a Reactive Action subclass.
	 */
	UFUNCTION(BlueprintPure, Category = "Reactive Actors", meta = (DisplayName = "Get Owner"))
	UObject* GetOwnerObject() const;

	/** C++ only: the actor this object lives on, found through its outer chain. Null for most Run Action objects. */
	AActor* GetOwningActor() const;

	/** Gives Blueprint subclasses world context (spawn, get actors, timers, etc.). */
	virtual UWorld* GetWorld() const override;

	/**
	 * One line describing this block as configured ("Delay 2.0s", "Broadcast Event.Door.Open -> World").
	 * Shown as the row title wherever it sits in an array. Override in C++ subclasses; the default is the
	 * class's display name. Blueprint subclasses override Get Summary instead.
	 */
	virtual FString BuildSummary() const;

	/**
	 * Your own row title for this block, e.g. "Flicker 3x over 2s". Return nothing to keep the built-in one.
	 * Called in the editor whenever the row refreshes (after load and after each edit), so only read this
	 * object's own settings here: no world, no other actors, nothing that changes over time.
	 * For live state such as countdowns, rules have Get Debug String, which the Reactions.Show overlay
	 * draws every frame.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Actors", meta = (DisplayName = "Get Summary"))
	FString GetSummary() const;
	virtual FString GetSummary_Implementation() const { return FString(); }

	/**
	 * Report problems with this block's settings, e.g. "Sound isn't set". Shown by Data Validation (on save, or
	 * right-click > Validate) and Map Check, labelled with where the block sits. Errors fail validation; warnings don't.
	 * Only read this object's own settings here: no world, no other actors. Not called on Class Defaults.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Reactive Actors", meta = (DisplayName = "Validate Settings"))
	void ValidateSettings(TArray<FText>& Errors, TArray<FText>& Warnings) const;
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const {}

	/** C++: Validate Settings, called safely (a Blueprint override only when Blueprint code can run). Editor use. */
	void CollectSettingsProblems(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	/**
	 * C++ only (rules, triggers, actions): just before this block's SaveGame variables are captured. For values that
	 * can't be saved as they are, e.g. a world time (world time starts again after a load): turn it into a duration
	 * held in a SaveGame variable. Blueprint blocks just tick SaveGame on plain values (counts, flags, durations).
	 */
	virtual void PreSaveState() {}

	/** C++ only: just after this block's SaveGame variables were restored. Undo what PreSaveState did. */
	virtual void PostLoadState() {}

protected:
	/** C++: a Blueprint subclass's Get Summary if it gives one (and it's safe to call now), otherwise Fallback. */
	FString ScriptSummaryOr(const FString& Fallback) const;

public:
#if WITH_EDITOR
	/** Rebuilds Summary (editor only). Called after load, after edits and on undo. */
	void RefreshSummary();

	/** Rebuilds Summary next frame, and the summaries of the blocks it sits in (a Blueprint Get Summary can't run while loading). */
	void RefreshSummarySoon();

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif

#if WITH_EDITORONLY_DATA
	/** What this block does, in one line. Used as its row title in arrays (editor only, not saved). */
	UPROPERTY(VisibleAnywhere, Transient, AdvancedDisplay, Category = "Summary")
	FString Summary;
#endif

};
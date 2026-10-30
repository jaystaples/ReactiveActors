// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactionSubsystem.h"
#include "EngineUtils.h"
#include "ReactiveActorsLog.h"
#include "ReactiveActorsSettings.h"
#include "ReactiveStateCache.h"
#include "TimerManager.h"
#include "ReactiveActorsStats.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ReactiveAction.h"
#include "ReactiveRule.h"
#include "ReactionComponent.h"
#include "Reaction.h"
#include "ActionRunner.h"
#include "ReactiveTrigger.h"
#include "ReactionPreset.h"
#include "GameFramework/Actor.h"

// =================================================================================================
// Registry and broadcasting
// =================================================================================================

bool UReactionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Only gameplay worlds (packaged game and PIE), not editor preview worlds
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UReactionSubsystem::RegisterEvent(UReactionComponent* Component, const FGameplayTagContainer& Tags)
{
	if (!Component)
	{
		return;
	}

	for (const FGameplayTag& Tag : Tags)
	{
		ComponentsByEvent.FindOrAdd(Tag).AddUnique(Component);
	}
}

void UReactionSubsystem::UnregisterEvent(UReactionComponent* Component, const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		if (TArray<TWeakObjectPtr<UReactionComponent>>* Found = ComponentsByEvent.Find(Tag))
		{
			Found->RemoveSwap(Component);
			if (Found->IsEmpty())
			{
				ComponentsByEvent.Remove(Tag);
			}
		}
	}
}

void UReactionSubsystem::RegisterIdentity(UReactionComponent* Component, const FGameplayTagContainer& IdentityTags)
{
	if (!Component)
	{
		return;
	}

	// Register under each tag and its parents, so a lookup on Actor.Kitchen finds Actor.Kitchen.Lamp
	for (const FGameplayTag& Tag : IdentityTags)
	{
		for (const FGameplayTag& Key : Tag.GetGameplayTagParents())
		{
			ComponentsByIdentity.FindOrAdd(Key).AddUnique(Component);
		}
	}
}

void UReactionSubsystem::UnregisterIdentity(UReactionComponent* Component, const FGameplayTagContainer& IdentityTags)
{
	for (const FGameplayTag& Tag : IdentityTags)
	{
		for (const FGameplayTag& Key : Tag.GetGameplayTagParents())
		{
			if (TArray<TWeakObjectPtr<UReactionComponent>>* Found = ComponentsByIdentity.Find(Key))
			{
				Found->RemoveSwap(Component);
				if (Found->IsEmpty())
				{
					ComponentsByIdentity.Remove(Key);
				}
			}
		}
	}
}

// Returns the actors whose Reaction Component has the given identity (or a child of it).
TArray<AActor*> UReactionSubsystem::GetActorsByIdentity(FGameplayTag Identity) const
{
	// A set inside so each actor is only added once (an actor can match through several of its tags),
	// returned as an array so Blueprint can For Each over it
	TSet<AActor*> Found;

	if (const TArray<TWeakObjectPtr<UReactionComponent>>* Components = ComponentsByIdentity.Find(Identity))
	{
		for (const TWeakObjectPtr<UReactionComponent>& Component : *Components)
		{
			if (!Component.IsValid())
			{
				continue; // destroyed or streamed out
			}
			if (AActor* Owner = Component->GetOwner())
			{
				Found.Add(Owner);
			}
		}
	}

	return Found.Array();
}

bool UReactionSubsystem::BroadcastEvent(FGameplayTag Event, AActor* Instigator)
{
	return BroadcastEventFrom(Event, Instigator, Instigator);
}

bool UReactionSubsystem::BroadcastEventFrom(FGameplayTag Event, AActor* Instigator, const AActor* Source)
{
	SCOPE_CYCLE_COUNTER(STAT_ReactionsSendEvent);
	if (!Event.IsValid() || bSuspended)
	{
		return false;
	}

	if (!Source)
	{
		Source = Instigator;
	}

	// Gather components registered under the event tag or any of its parents
	// (a reaction on Event.Photo should hear Event.Photo.Kitchen).
	// The set de-duplicates components that registered under several of those tags.
	TSet<UReactionComponent*> Targets;
	for (const FGameplayTag& Tag : Event.GetGameplayTagParents())
	{
		if (const TArray<TWeakObjectPtr<UReactionComponent>>* Found = ComponentsByEvent.Find(Tag))
		{
			for (const TWeakObjectPtr<UReactionComponent>& Weak : *Found)
			{
				if (UReactionComponent* Component = Weak.Get())
				{
					Targets.Add(Component);
				}
			}
		}
	}

	// Call from a snapshot: actions may spawn or destroy actors, which (un)registers components mid-loop
	bool bAnyFired = false;
	TArray<TPair<FString, bool>> Receivers;
	for (UReactionComponent* Component : Targets.Array())
	{
		if (!IsValid(Component))
		{
			continue;
		}

		// Call every component: don't stop at the first one that fires
		const bool bFired = Component->ReactTo(Event, Instigator);
		bAnyFired |= bFired;

		DrawDebugWire(Source, Component->GetOwner(), Event, bFired);
		Receivers.Emplace(GetNameSafe(Component->GetOwner()), bFired);
	}

	FReactiveEventInfo Info;
	Info.Event = Event;
	Info.Instigator = Instigator;
	Info.Source = const_cast<AActor*>(Source);
	Info.Route = EReactiveEventRoute::World;
	NotifyEventSent(Info, Receivers);
	return bAnyFired;
}

// =================================================================================================
// Suspend, cancel, Run Action
// =================================================================================================

void UReactionSubsystem::NotifyEventSent(const FReactiveEventInfo& Info, const TArray<TPair<FString, bool>>& Receivers)
{
	INC_DWORD_STAT(STAT_ReactionsEventsSent);

#if !UE_BUILD_SHIPPING
	RecordEventHistory(Info, Receivers);
#endif

	OnEventBroadcast.Broadcast(Info.Event);
	OnEventSent.Broadcast(Info);
}

bool UReactionSubsystem::EventMatches(const FGameplayTag& Sent, const FGameplayTag& ListenFor, bool bExactMatch)
{
	if (!Sent.IsValid() || !ListenFor.IsValid())
	{
		return false;
	}
	return bExactMatch ? Sent.MatchesTagExact(ListenFor) : Sent.MatchesTag(ListenFor);
}

void UReactionSubsystem::SetEventsSuspended(bool bSuspend)
{
	bSuspended = bSuspend;
}

TArray<UReactionComponent*> UReactionSubsystem::GetAllComponents() const
{
	// Every component with an enabled reaction is registered under an event tag, and every one
	// with an identity under that identity. A set, since most are in several lists.
	TSet<UReactionComponent*> Components;
	for (const auto* Registry : { &ComponentsByEvent, &ComponentsByIdentity })
	{
		for (const TPair<FGameplayTag, TArray<TWeakObjectPtr<UReactionComponent>>>& Pair : *Registry)
		{
			for (const TWeakObjectPtr<UReactionComponent>& Weak : Pair.Value)
			{
				if (UReactionComponent* Component = Weak.Get())
				{
					Components.Add(Component);
				}
			}
		}
	}
	return Components.Array();
}

int32 UReactionSubsystem::CancelOneShots(TFunctionRef<bool(const UReactionOneShot&)> ShouldCancel)
{
	int32 Cancelled = 0;

	// Copy: each run removes itself from OneShots when it completes
	const TArray<TObjectPtr<UReactionOneShot>> Runs = OneShots;
	for (UReactionOneShot* Run : Runs)
	{
		if (Run && Run->Sequence.IsRunning() && ShouldCancel(*Run))
		{
			Run->Sequence.Cancel();
			++Cancelled;
		}
	}
	return Cancelled;
}

int32 UReactionSubsystem::CancelAllReactions()
{
	int32 Cancelled = 0;

	// The list is a snapshot: rules are told about the cancel, and their Blueprints may spawn or destroy actors
	for (UReactionComponent* Component : GetAllComponents())
	{
		if (IsValid(Component))
		{
			Cancelled += Component->CancelReactions(FGameplayTag());
		}
	}

	// One-off runs (Run Action, Run Actions, Run Reaction)
	Cancelled += CancelOneShots([](const UReactionOneShot&) { return true; });
	return Cancelled;
}

int32 UReactionSubsystem::CancelReactionsForActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return 0;
	}

	int32 Cancelled = 0;
	TInlineComponentArray<UReactionComponent*> Components(Actor);
	for (UReactionComponent* Component : Components)
	{
		Cancelled += Component->CancelReactions(FGameplayTag());
	}

	// One-off runs aimed at this actor
	Cancelled += CancelOneShots([Actor](const UReactionOneShot& Run) { return Run.Sequence.GetContext().Target == Actor; });
	return Cancelled;
}

int32 UReactionSubsystem::CancelReactionsForIdentity(FGameplayTag Identity)
{
	int32 Cancelled = 0;
	for (AActor* Actor : GetActorsByIdentity(Identity))
	{
		Cancelled += CancelReactionsForActor(Actor);
	}
	return Cancelled;
}

int32 UReactionSubsystem::CancelReactionsForEvent(FGameplayTag Event)
{
	if (!Event.IsValid())
	{
		return 0;
	}

	int32 Cancelled = 0;
	for (UReactionComponent* Component : GetAllComponents())
	{
		if (IsValid(Component))
		{
			Cancelled += Component->CancelReactions(Event);
		}
	}

	// One-off runs started with this event (or a child of it)
	Cancelled += CancelOneShots([&Event](const UReactionOneShot& Run) { return Run.Sequence.GetContext().Event.MatchesTag(Event); });
	return Cancelled;
}

int32 UReactionSubsystem::ResetAllReactions()
{
	// One-off runs are cancelled; Run Reaction Preset's kept rules are reset
	int32 Reset = CancelOneShots([](const UReactionOneShot&) { return true; });
	Reset += ResetPresetSlots([](const FReactivePresetRunSlot&) { return true; });

	for (UReactionComponent* Component : GetAllComponents())
	{
		if (IsValid(Component))
		{
			Reset += Component->ResetReactions(FGameplayTag());
		}
	}
	OnReset.Broadcast(nullptr, FGameplayTag());
	return Reset;
}

int32 UReactionSubsystem::ResetReactionsForActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return 0;
	}

	int32 Reset = CancelOneShots([Actor](const UReactionOneShot& Run) { return Run.Sequence.GetContext().Target == Actor; });
	Reset += ResetPresetSlots([Actor](const FReactivePresetRunSlot& Slot) { return Slot.Target.Get() == Actor; });

	TInlineComponentArray<UReactionComponent*> Components(Actor);
	for (UReactionComponent* Component : Components)
	{
		Reset += Component->ResetReactions(FGameplayTag());
	}
	OnReset.Broadcast(Actor, FGameplayTag());
	return Reset;
}

int32 UReactionSubsystem::ResetReactionsForIdentity(FGameplayTag Identity)
{
	int32 Reset = 0;
	for (AActor* Actor : GetActorsByIdentity(Identity))
	{
		Reset += ResetReactionsForActor(Actor);
	}
	return Reset;
}

int32 UReactionSubsystem::ResetReactionsForEvent(FGameplayTag Event)
{
	if (!Event.IsValid())
	{
		return 0;
	}

	int32 Reset = CancelOneShots([&Event](const UReactionOneShot& Run) { return Run.Sequence.GetContext().Event.MatchesTag(Event); });
	Reset += ResetPresetSlots([&Event](const FReactivePresetRunSlot& Slot) { return Slot.EventTag.IsValid() && Slot.EventTag.MatchesTag(Event); });

	for (UReactionComponent* Component : GetAllComponents())
	{
		if (IsValid(Component))
		{
			Reset += Component->ResetReactions(Event);
		}
	}
	OnReset.Broadcast(nullptr, Event);
	return Reset;
}

bool UReactionSubsystem::BroadcastEventToActor(AActor* Actor, FGameplayTag Event, AActor* Instigator, const AActor* Source)
{
	SCOPE_CYCLE_COUNTER(STAT_ReactionsSendEvent);
	if (!IsValid(Actor) || !Event.IsValid() || bSuspended)
	{
		return false;
	}
	if (!Source)
	{
		Source = Instigator;
	}

	bool bAnyFired = false;
	TInlineComponentArray<UReactionComponent*> Components(Actor);
	for (UReactionComponent* Component : Components)
	{
		bAnyFired |= Component->ReactTo(Event, Instigator);
	}

	DrawDebugWire(Source, Actor, Event, bAnyFired);

	FReactiveEventInfo Info;
	Info.Event = Event;
	Info.Instigator = Instigator;
	Info.Source = const_cast<AActor*>(Source);
	Info.Route = EReactiveEventRoute::Actor;
	Info.TargetActor = Actor;

	TArray<TPair<FString, bool>> Receivers;
	if (Components.Num() > 0)
	{
		Receivers.Emplace(GetNameSafe(Actor), bAnyFired);
	}
	NotifyEventSent(Info, Receivers);
	return bAnyFired;
}

bool UReactionSubsystem::BroadcastEventToIdentity(FGameplayTag Identity, FGameplayTag Event, AActor* Instigator, const AActor* Source)
{
	SCOPE_CYCLE_COUNTER(STAT_ReactionsSendEvent);
	if (!Event.IsValid() || bSuspended)
	{
		return false;
	}
	if (!Source)
	{
		Source = Instigator;
	}

	bool bAnyFired = false;
	TArray<TPair<FString, bool>> Receivers;
	for (AActor* Actor : GetActorsByIdentity(Identity))
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		TInlineComponentArray<UReactionComponent*> Components(Actor);
		bool bActorFired = false;
		for (UReactionComponent* Component : Components)
		{
			bActorFired |= Component->ReactTo(Event, Instigator);
		}

		DrawDebugWire(Source, Actor, Event, bActorFired);
		bAnyFired |= bActorFired;
		Receivers.Emplace(GetNameSafe(Actor), bActorFired);
	}

	FReactiveEventInfo Info;
	Info.Event = Event;
	Info.Instigator = Instigator;
	Info.Source = const_cast<AActor*>(Source);
	Info.Route = EReactiveEventRoute::Identity;
	Info.TargetIdentity = Identity;
	NotifyEventSent(Info, Receivers);
	return bAnyFired;
}

bool UReactionSubsystem::RunAction(UReactiveAction* Action, AActor* Target, AActor* Instigator)
{
	if (!IsValid(Action) || Action->HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("Run Action: no action. Pick its class on the Run Action node."));
		return false;
	}

	if (!Action->IsAvailableInBlueprintRunAction())
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("Run Action: %s can only be used inside a reaction (Available in Blueprint Run Action is off)."),
			*Action->GetClass()->GetName());
		return false;
	}

	FReactionContext Context;
	Context.Target = Target;
	Context.Instigator = Instigator;

	TArray<TObjectPtr<UReactiveAction>> List;
	List.Add(Action);
	if (!StartOneShot(List, {}, Context, TEXT("Run Action")))
	{
		return false;
	}

	// False if a Class Requirement failed on the target (the action was skipped)
	return Action->DidLastExecuteRun();
}

bool UReactionSubsystem::RunActions(const TArray<UReactiveAction*>& Actions, AActor* Target, AActor* Instigator, FGameplayTag Event)
{
	FReactionContext Context;
	Context.Target = Target;
	Context.Instigator = Instigator;
	Context.Event = Event;

	TArray<TObjectPtr<UReactiveAction>> List;
	List.Reserve(Actions.Num());
	for (UReactiveAction* Action : Actions)
	{
		List.Add(Action);
	}
	return StartOneShot(List, {}, Context, TEXT("Run Actions")) != nullptr;
}

bool UReactionSubsystem::RunReaction(const FReaction& Reaction, AActor* Target, AActor* Instigator)
{
	if (!Reaction.bEnabled)
	{
		return false;
	}

	FReactionContext Context;
	Context.Target = Target;
	Context.Instigator = Instigator;
	Context.Event = Reaction.EventTag;

	return StartOneShot(Reaction.Actions, Reaction.Rules, Context, TEXT("Run Reaction")) != nullptr;
}

UReactionOneShot* UReactionSubsystem::StartOneShot(const TArray<TObjectPtr<UReactiveAction>>& Actions,
	const TArray<TObjectPtr<UReactiveRule>>& Rules, const FReactionContext& Context, const TCHAR* Label,
	TFunction<void(EReactionResult)> OnFinished)
{
	if (!IsValid(Context.Target))
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s: no Target. Actions need an actor to act on."), Label);
		return nullptr;
	}

	// Not from a construction script (or anything else before the target is playing): see ReactiveAction.h
	if (!Context.Target->HasActorBegunPlay())
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s: %s hasn't begun play yet (called from a construction script?). Run actions from Begin Play or later."),
			Label, *GetNameSafe(Context.Target));
		return nullptr;
	}

	bool bAnyAction = false;
	for (const UReactiveAction* Action : Actions)
	{
		if (!IsValid(Action))
		{
			continue; // empty slots are skipped, like in a reaction
		}
		bAnyAction = true;

		// The same object can't run twice at once (it holds its own run state)
		if (Action->IsRunning())
		{
			UE_LOG(LogReactiveActors, Warning, TEXT("%s: %s is already running. Make a new one, or wait for it to finish."),
				Label, *Action->GetClass()->GetName());
			return nullptr;
		}
	}
	if (!bAnyAction)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s: no actions to run."), Label);
		return nullptr;
	}

	// Any rule can block the run, as on a component
	for (const UReactiveRule* Rule : Rules)
	{
		if (Rule && Rule->ShouldBlock(Context))
		{
			UE_LOG(LogReactiveActors, Log, TEXT("%s on %s: blocked by %s"),
				Label, *GetNameSafe(Context.Target), *Rule->GetClass()->GetName());
			return nullptr;
		}
	}

	// The holder keeps the actions, rules and sequence alive until the run finishes
	UReactionOneShot* Run = NewObject<UReactionOneShot>(this);
	Run->Label = Label;
	if (const UWorld* World = GetWorld())
	{
		Run->StartTime = World->GetTimeSeconds();
	}
	Run->Actions = Actions;
	Run->Rules = Rules;
	OneShots.Add(Run);

	TWeakObjectPtr<UReactionOneShot> WeakRun = Run;

	// When it finishes (or is cancelled): tell the rules, unload the actions and let the holder go
	Run->Sequence.OnComplete = [this, WeakRun, OnFinished = MoveTemp(OnFinished)](EReactionResult Result, bool /*bStoppedEarly*/)
		{
			if (UReactionOneShot* Finished = WeakRun.Get())
			{
				const FReactionContext RunContext = Finished->Sequence.GetContext();
				for (UReactiveRule* Rule : Finished->Rules)
				{
					if (Rule)
					{
						Rule->OnFinish(RunContext, Result);
					}
				}
				for (UReactiveAction* Action : Finished->Actions)
				{
					if (Action)
					{
						Action->Cleanup();
					}
				}
				OneShots.Remove(Finished);

				// Run Reaction Preset: drop the slot's hold too, or the finished run (and its action copies) lingers
				// until that preset reaction runs on that target again
				for (FReactivePresetRunSlot& Slot : PresetSlots)
				{
					if (Slot.ActiveRun == Finished)
					{
						Slot.ActiveRun = nullptr;
					}
				}
			}

			// Last: the caller may start another run from here (Run Reaction Preset's On Finished)
			if (OnFinished)
			{
				OnFinished(Result);
			}
		};

	for (UReactiveRule* Rule : Run->Rules)
	{
		if (Rule)
		{
			Rule->OnStart(Context);
		}
	}

	Run->Sequence.Start(Run, Run->Actions, Context); // instant runs finish (and clean up) right here
	return Run;
}

int32 UReactionSubsystem::FindOrAddPresetSlot(const UReactionPreset* Preset, int32 ReactionIndex, AActor* Target)
{
	const int32 Found = PresetSlots.IndexOfByPredicate([Preset, ReactionIndex, Target](const FReactivePresetRunSlot& Slot)
		{
			return Slot.ReactionIndex == ReactionIndex && Slot.Preset.Get() == Preset && Slot.Target.Get() == Target;
		});
	if (Found != INDEX_NONE)
	{
		return Found;
	}

	const FReaction& Source = Preset->Reactions[ReactionIndex];
	FReactivePresetRunSlot Slot;
	Slot.Preset = Preset;
	Slot.Target = Target;
	Slot.ReactionIndex = ReactionIndex;
	Slot.EventTag = Source.EventTag;

	// The target's own copies (owned by it, so Get Owner resolves to it), never the asset's
	for (const UReactiveRule* Rule : Source.Rules)
	{
		if (Rule)
		{
			Slot.Rules.Add(DuplicateObject<UReactiveRule>(Rule, Target));
		}
	}
	return PresetSlots.Add(MoveTemp(Slot));
}

int32 UReactionSubsystem::RunReactionPreset(const UReactionPreset* Preset, FGameplayTag Event, AActor* Target, AActor* Instigator,
	TFunction<void(EReactionResult)> OnRunFinished, TArray<UReactionOneShot*>* OutRuns)
{
	if (!Preset)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("Run Reaction Preset: no Preset."));
		return 0;
	}
	if (!IsValid(Target))
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("Run Reaction Preset (%s): no Target. Actions need an actor to act on."), *Preset->GetName());
		return 0;
	}

	int32 Started = 0;
	for (int32 Index = 0; Index < Preset->Reactions.Num(); ++Index)
	{
		const FReaction& Source = Preset->Reactions[Index];
		if (!Source.bEnabled || (Event.IsValid() && !Source.Matches(Event)))
		{
			continue;
		}

		int32 SlotIndex = FindOrAddPresetSlot(Preset, Index, Target);

		// Still running on this target: Ignore skips it, Restart cancels the current run first
		if (UReactionOneShot* Running = PresetSlots[SlotIndex].ActiveRun; Running && Running->Sequence.IsRunning())
		{
			if (Source.Retrigger == EReactiveRetriggerPolicy::Ignore)
			{
				continue;
			}
			Running->Sequence.Cancel(); // counts as finishing: its rules and caller are told (Cancelled)
			if (!IsValid(Target))
			{
				break; // something on that run's way out destroyed the target
			}
			SlotIndex = FindOrAddPresetSlot(Preset, Index, Target); // the cancel may have added slots
		}

		// Fresh actions every run (they hold run state), owned by the target like a component's copies
		TArray<TObjectPtr<UReactiveAction>> Actions;
		Actions.Reserve(Source.Actions.Num());
		for (const UReactiveAction* Action : Source.Actions)
		{
			if (Action)
			{
				Actions.Add(DuplicateObject<UReactiveAction>(Action, Target));
			}
		}

		FReactionContext Context;
		Context.Target = Target;
		Context.Instigator = Instigator;
		Context.Event = Event.IsValid() ? Event : Source.EventTag;

		const TArray<TObjectPtr<UReactiveRule>> Rules = PresetSlots[SlotIndex].Rules;
		UReactionOneShot* Run = StartOneShot(Actions, Rules, Context, TEXT("Run Reaction Preset"), OnRunFinished);
		if (!Run)
		{
			continue; // a rule blocked it, or no actions
		}

		++Started;
		if (OutRuns)
		{
			OutRuns->Add(Run);
		}

		// Find it again: the run's actions may have added slots
		SlotIndex = FindOrAddPresetSlot(Preset, Index, Target);
		PresetSlots[SlotIndex].ActiveRun = Run->Sequence.IsRunning() ? Run : nullptr;

		if (!IsValid(Target))
		{
			break;
		}
	}
	return Started;
}

int32 UReactionSubsystem::ResetPresetSlots(TFunctionRef<bool(const FReactivePresetRunSlot&)> ShouldReset)
{
	int32 Reset = 0;
	for (const FReactivePresetRunSlot& Slot : PresetSlots)
	{
		if (!ShouldReset(Slot))
		{
			continue;
		}
		for (UReactiveRule* Rule : Slot.Rules)
		{
			if (Rule)
			{
				Rule->OnReset();
			}
		}
		++Reset;
	}
	return Reset;
}

void UReactionSubsystem::CancelOrphanedOneShots()
{
	// Run Reaction Preset slots for targets (or presets) that are gone
	if (!PresetSlots.IsEmpty())
	{
		PresetSlots.RemoveAll([](const FReactivePresetRunSlot& Slot) { return !Slot.Target.IsValid() || !Slot.Preset.IsValid(); });
	}

	if (OneShots.IsEmpty())
	{
		return;
	}

	// Copy: cancelling a run removes it from OneShots
	const TArray<TObjectPtr<UReactionOneShot>> Runs = OneShots;
	for (UReactionOneShot* Run : Runs)
	{
		if (!Run)
		{
			continue;
		}

		// Safety net: a finished run should already have removed itself
		if (!Run->Sequence.IsRunning())
		{
			OneShots.Remove(Run);
			continue;
		}

		if (!IsValid(Run->Sequence.GetContext().Target))
		{
			UE_LOG(LogReactiveActors, Log, TEXT("%s: its target was destroyed or streamed out mid-run, so it was cancelled."), *Run->Label);
			Run->Sequence.Cancel(); // counts as finishing: rules are told, actions cleaned up, holder released
		}
	}
}

void UReactionSubsystem::Deinitialize()
{
	// World ending: stop any one-off runs still going (timers, sequences) without finishing them
	for (UReactionOneShot* Run : OneShots)
	{
		if (Run)
		{
			Run->Sequence.Stop();
			for (UReactiveAction* Action : Run->Actions)
			{
				if (Action)
				{
					Action->Cleanup();
				}
			}
		}
	}
	OneShots.Reset();
	PresetSlots.Reset();
	HeldSends.Reset();
	bHolding = false;

	Super::Deinitialize();
}

// =================================================================================================
// Tick: cleans up orphaned one-off runs; in debug builds, draws the overlay (ReactionSubsystemDebug.cpp)
// =================================================================================================

void UReactionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Every build: don't let one-off runs aimed at a destroyed actor hang around until the level ends
	{
		SCOPE_CYCLE_COUNTER(STAT_ReactionsCleanup);
		CancelOrphanedOneShots();
	}
	SET_DWORD_STAT(STAT_ReactionsOneShots, OneShots.Num());

#if !UE_BUILD_SHIPPING
	{
		SCOPE_CYCLE_COUNTER(STAT_ReactionsDebug);
		TickDebug();
	}
#endif
}

// =================================================================================================
// Save and load
// =================================================================================================

void UReactionSubsystem::HoldUntilRestored()
{
	// This level already got its saved state as it started (Set World Reaction State before Open Level): nothing to wait for
	if (const UReactiveStateCache* Cache = UReactiveStateCache::Get(this); Cache && Cache->WasRestored(GetWorld()))
	{
		return;
	}
	bHolding = true;

	// Safety: a restore that never comes shouldn't freeze every trigger for good
	const float SafetySeconds = GetDefault<UReactiveActorsSettings>()->HoldSafetyRelease;
	if (UWorld* World = GetWorld(); World && SafetySeconds > 0.f)
	{
		World->GetTimerManager().SetTimer(HoldSafetyHandle, FTimerDelegate::CreateWeakLambda(this, [this, SafetySeconds]()
			{
				if (bHolding)
				{
					UE_LOG(LogReactiveActors, Warning, TEXT("Hold Reactions Until Restored: no restore after %.0fs, so %d held fire(s) went ahead without it. Call Set World Reaction State (or Release Held Reactions) after loading."),
						SafetySeconds, HeldSends.Num());
					ReleaseHeld();
				}
			}), SafetySeconds, false);
	}
}

void UReactionSubsystem::ReleaseHeld()
{
	bHolding = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldSafetyHandle);
	}

	// Swap out first: a send can start reactions that hold or send again
	TArray<TFunction<void()>> Sends = MoveTemp(HeldSends);
	HeldSends.Reset();
	for (TFunction<void()>& Send : Sends)
	{
		if (Send)
		{
			Send();
		}
	}
}

bool UReactionSubsystem::DeferIfHolding(TFunction<void()> Send)
{
	if (!bHolding)
	{
		return false;
	}
	HeldSends.Add(MoveTemp(Send));
	return true;
}

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactionComponent.h"
#include "ReactiveActorsLog.h"
#include "ReactiveActorsStats.h"
#include "Components/BillboardComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "HAL/IConsoleManager.h"
#include "ReactiveAction.h"
#include "ReactiveRule.h"
#include "ReactionPreset.h"
#include "ReactiveRequirement.h"
#include "ReactionSubsystem.h"
#include "ReactiveActorsSettings.h"
#include "ReactiveTrigger.h"
#include "ReactiveStateCache.h"

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "Misc/DataValidation.h"
#include "Misc/UObjectToken.h"
#include "ReactiveValidation.h"

void UReactionComponent::CheckForErrors()
{
	Super::CheckForErrors();

	AActor* Owner = GetOwner();
	FMessageLog MapCheck("MapCheck");

	// Gentle suggestion (not a warning): one component per actor is easier to follow.
	// Reported once, by the first component on the actor.
	if (Owner)
	{
		TInlineComponentArray<UReactionComponent*> Siblings(Owner);
		if (Siblings.Num() > 1 && Siblings[0] == this)
		{
			MapCheck.Info()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(
					INVTEXT("has {0} Reaction Components. One is usually enough: add more reactions or identity tags to it instead."),
					Siblings.Num())));
		}
	}

	// The same checks as Data Validation, plus requirements tested against this actor where possible
	GatherProblems(Owner, [&MapCheck, Owner](const FText& Message, bool bError)
		{
			TSharedRef<FTokenizedMessage> Line = bError ? MapCheck.Error() : MapCheck.Warning();
			Line->AddToken(FUObjectToken::Create(Owner));
			Line->AddToken(FTextToken::Create(Message));
		});
}

void UReactionComponent::GatherProblems(const AActor* OwnerForAheadChecks, ReactiveValidation::FReport Report) const
{
	// The Preset's own reactions and triggers are checked when the Preset asset is validated; Map Check
	// (which has an actor to test requirements against) checks them here too, labelled as the Preset's
	if (Preset && OwnerForAheadChecks)
	{
		const FText Prefix = FText::Format(INVTEXT("Preset {0} "), FText::FromString(Preset->GetName()));
		ReactiveValidation::CheckReactions(Preset->Reactions, Prefix, OwnerForAheadChecks, Report);
		ReactiveValidation::CheckTriggers(Preset->Triggers, Prefix, Report);
	}

	ReactiveValidation::CheckReactions(Reactions, FText::GetEmpty(), OwnerForAheadChecks, Report);
	ReactiveValidation::CheckTriggers(Triggers, FText::GetEmpty(), Report);
}

EDataValidationResult UReactionComponent::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);

	const AActor* Owner = GetOwner();
	const FText Where = FText::FromString(Owner ? Owner->GetActorNameOrLabel() : GetName());
	const EDataValidationResult Ours = ReactiveValidation::Validate(Context, Where,
		[this](ReactiveValidation::FReport Report) { GatherProblems(nullptr, Report); });

	return CombineDataValidationResults(Result, Ours);
}

void UReactionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Reactions and triggers added (or copied) get IDs, so saved games still match after they're reordered
	if (ReactiveSave::EnsureIds(Reactions, Triggers))
	{
		MarkPackageDirty();
	}
}
#endif

UReactionComponent::UReactionComponent()
{
	// Only ticks while a trigger needs it (switched on in Begin Play)
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (UReactiveTrigger* Trigger : ActiveTriggers)
	{
		if (Trigger && Trigger->IsActive() && Trigger->WantsTick())
		{
			Trigger->TickTrigger(DeltaTime);
		}
	}
}

void UReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Once only: Begin Play can run again on the same actor, and its state should carry over
	if (!bListsBuilt)
	{
		BuildActiveLists();
		bListsBuilt = true;
	}

	// Saved state: a Restore that came in before Begin Play, or what this component had when its level streamed out
	// (or a world restore made while it wasn't loaded). Before anything registers or fires, so Max Activations on a
	// Begin Play trigger, or a reaction switched off, already count.
	{
		FReactiveActorState Cached;
		UReactiveStateCache* Cache = IsPlacedInLevel() ? UReactiveStateCache::Get(this) : nullptr;
		if (Cache && Cache->TakeActor(GetWorld(), GetSaveKey(), Cached))
		{
			ApplyState(Cached, /*bLive*/ false);
		}
		if (bHasPendingState)
		{
			bHasPendingState = false;
			ApplyState(PendingState, /*bLive*/ false);
			PendingState = FReactiveActorState();
		}
	}

	for (int32 Index = 0; Index < ActiveReactions.Num(); ++Index)
	{
		FReaction& Reaction = ActiveReactions[Index];

		// Bound for every reaction, so one switched on later (Set Reaction Enabled) reports back too
		Reaction.Sequence.OnComplete = [this, Index](EReactionResult Result, bool /*bStoppedEarly*/) { HandleReactionComplete(Index, Result); };

		// Disabled reactions aren't registered or loaded now; switched on later, their actions load when they first run
		if (!Reaction.bEnabled)
		{
			continue;
		}

		for (UReactiveAction* Action : Reaction.Actions)
		{
			if (Action)
			{
				Action->Load(); // also loads nested actions
			}
		}
	}

	// Same list Get Event Tags returns, kept so EndPlay unregisters exactly what was registered
	RegisteredTags = GetEventTags();

	if (UWorld* World = GetWorld())
	{
		if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			Subsystem->RegisterEvent(this, RegisteredTags);
			Subsystem->RegisterIdentity(this, IdentityTags);
		}
	}

	// Triggers last: their events can reach this actor's own reactions, which are registered and loaded now
	bool bAnyTriggerTicks = false;
	for (UReactiveTrigger* Trigger : ActiveTriggers)
	{
		if (!Trigger || !Trigger->IsEnabled())
		{
			continue;
		}

		Trigger->Activate(this);
		bAnyTriggerTicks |= Trigger->WantsTick();

		if (!HasBegunPlay())
		{
			return; // a trigger's reaction ended this actor's play (destroyed it)
		}
	}
	SetComponentTickEnabled(bAnyTriggerTicks);
}

void UReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Streamed out (a streaming level, a World Partition cell) or its level ending: if anything happened here, write
	// what it remembers to the cache, so it carries on when it streams back in and the level's saved state includes it.
	// Not when it's destroyed: it isn't coming back.
	const bool bStreamedOut = EndPlayReason == EEndPlayReason::RemovedFromWorld;
	if (bListsBuilt && bStateChanged && EndPlayReason != EEndPlayReason::Destroyed && IsPlacedInLevel()
		&& (!bStreamedOut || GetDefault<UReactiveActorsSettings>()->bKeepStateWhenStreamedOut))
	{
		if (UReactiveStateCache* Cache = UReactiveStateCache::Get(this))
		{
			Cache->StoreActor(GetWorld(), CaptureReactionState());
		}
	}

	// Triggers first, so nothing fires into reactions that are being shut down
	for (UReactiveTrigger* Trigger : ActiveTriggers)
	{
		if (Trigger)
		{
			Trigger->Deactivate(EndPlayReason); // End Play triggers fire in here, before anything shuts down
		}
	}
	SetComponentTickEnabled(false);

	if (UWorld* World = GetWorld())
	{
		if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			Subsystem->UnregisterEvent(this, RegisteredTags);
			Subsystem->UnregisterIdentity(this, IdentityTags);
		}
	}
	RegisteredTags.Reset();

	for (FReaction& Reaction : ActiveReactions)
	{
		// Abandon any sequence still running (e.g. waiting on a latent action)
		Reaction.Sequence.Stop();

		for (UReactiveAction* Action : Reaction.Actions)
		{
			if (Action)
			{
				Action->Cleanup(); // also cleans up nested actions
			}
		}
	}

	// Don't empty ActiveReactions here: EndPlay can run in the middle of one of their sequences
	// (an action destroying its own actor), and freeing the array would pull it out from under it.
	// It's also kept on purpose: Begin Play can run again on this same actor (data layer Loaded and
	// back to Activated, a streaming level hidden and shown), and the rules keep their state.

	Super::EndPlay(EndPlayReason);
}

int32 UReactionComponent::SetReactionsEnabled(FGameplayTag Event, bool bEnabled)
{
	int32 Changed = 0;
	for (FReaction& Reaction : ActiveReactions)
	{
		if (Reaction.bEnabled == bEnabled)
		{
			continue;
		}
		if (Event.IsValid() && !(Reaction.EventTag.IsValid() && Reaction.EventTag.MatchesTag(Event)))
		{
			continue;
		}
		Reaction.bEnabled = bEnabled;
		++Changed;
	}
	bStateChanged |= Changed > 0;

	// Listen for what's now enabled: unregister the old set, register the new one
	if (Changed > 0 && HasBegunPlay())
	{
		if (UWorld* World = GetWorld())
		{
			if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
			{
				Subsystem->UnregisterEvent(this, RegisteredTags);
				RegisteredTags = GetEventTags();
				Subsystem->RegisterEvent(this, RegisteredTags);
			}
		}
	}
	return Changed;
}

FGameplayTagContainer UReactionComponent::GetEventTags() const
{
	FGameplayTagContainer Tags;
	auto AddFrom = [&Tags](const TArray<FReaction>& List)
		{
			for (const FReaction& Reaction : List)
			{
				if (Reaction.bEnabled && Reaction.EventTag.IsValid())
				{
					Tags.AddTag(Reaction.EventTag);
				}
			}
		};

	// While playing, the live copies (Set Reaction Enabled changes those); before that, the Preset and inline lists
	if (bListsBuilt)
	{
		AddFrom(ActiveReactions);
	}
	else
	{
		if (Preset)
		{
			AddFrom(Preset->Reactions);
		}
		AddFrom(Reactions);
	}

	// Triggers that listen for events reaching this actor (Event Received, Self).
	// Read from the Preset and inline arrays, so it works in the editor before anything is copied.
	auto AddHeard = [&Tags](const TArray<TObjectPtr<UReactiveTrigger>>& List)
		{
			for (const UReactiveTrigger* Trigger : List)
			{
				if (Trigger)
				{
					Trigger->GetHeardEvents(Tags);
				}
			}
		};

	if (bListsBuilt)
	{
		AddHeard(ActiveTriggers);
	}
	else
	{
		if (Preset)
		{
			AddHeard(Preset->Triggers);
		}
		AddHeard(Triggers);
	}
	return Tags;
}

void UReactionComponent::BuildActiveLists()
{
	ActiveReactions.Reset();
	ActiveTriggers.Reset();

	// Preset reactions and triggers first. They belong to the shared asset, so each actor gets its
	// own duplicates (owned by this component): per-actor state stays separate and Get Owner
	// resolves to this actor.
	if (Preset)
	{
		for (const FReaction& Source : Preset->Reactions)
		{
			FReaction& Copy = ActiveReactions.Add_GetRef(Source);
			Copy.Sequence = FActionRunner();

			for (TObjectPtr<UReactiveRule>& Rule : Copy.Rules)
			{
				if (Rule)
				{
					Rule = DuplicateObject<UReactiveRule>(Rule, this);
				}
			}
			for (TObjectPtr<UReactiveAction>& Action : Copy.Actions)
			{
				if (Action)
				{
					Action = DuplicateObject<UReactiveAction>(Action, this);
				}
			}
		}
	}
	NumPresetReactions = ActiveReactions.Num();

	// Then this actor's own reactions (already owned by this component)
	ActiveReactions.Append(Reactions);

	// Triggers the same way: the Preset's copied, then this actor's own
	if (Preset)
	{
		for (const UReactiveTrigger* Source : Preset->Triggers)
		{
			if (Source)
			{
				if (UReactiveTrigger* Copy = Source->DuplicateFor(this))
				{
					ActiveTriggers.Add(Copy);
				}
			}
		}
	}
	NumPresetTriggers = ActiveTriggers.Num();
	for (UReactiveTrigger* Trigger : Triggers)
	{
		if (Trigger)
		{
			ActiveTriggers.Add(Trigger);
		}
	}
}

bool UReactionComponent::ReactTo(FGameplayTag Event, AActor* Instigator)
{
	SCOPE_CYCLE_COUNTER(STAT_ReactionsReactTo);

	if (!Event.IsValid() || !HasBegunPlay())
	{
		return false;
	}

	// Suspend Events: events are ignored everywhere, including ones sent straight to this component
	if (const UWorld* World = GetWorld())
	{
		if (const UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			if (Subsystem->AreEventsSuspended())
			{
				return false;
			}
		}
	}

	bool bAnyFired = false;
	for (int32 Index = 0; Index < ActiveReactions.Num(); ++Index)
	{
		if (!HasBegunPlay())
		{
			break; // an action ended this component's play (e.g. destroyed the actor)
		}

		if (ActiveReactions[Index].Matches(Event) && TryTrigger(Index, Event, Instigator))
		{
			bAnyFired = true;
		}
	}

	// Triggers listening for events that reach this actor (Event Received, Self)
	for (UReactiveTrigger* Trigger : ActiveTriggers)
	{
		if (!HasBegunPlay())
		{
			break;
		}
		if (Trigger && Trigger->IsActive() && Trigger->HandleEventToActor(Event, Instigator))
		{
			bAnyFired = true;
		}
	}
	return bAnyFired;
}

bool UReactionComponent::TryTrigger(int32 ReactionIndex, const FGameplayTag& Event, AActor* Instigator)
{
	FReaction& Reaction = ActiveReactions[ReactionIndex];

	if (!Reaction.bEnabled)
	{
		return false;
	}

	// Already running: Ignore (the default) drops the event; Restart cancels the run and starts again
	const bool bRestart = Reaction.Sequence.IsRunning();
	if (bRestart && Reaction.Retrigger == EReactiveRetriggerPolicy::Ignore)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	// The context this run would have (rules see it in Should Block, On Start and On Finish)
	FReactionContext Context;
	Context.Target = GetOwner();
	Context.Instigator = Instigator;
	Context.Event = Event;

	// Any rule can block this activation (checked before a restart cancels anything)
	for (const UReactiveRule* Rule : Reaction.Rules)
	{
		if (Rule && Rule->ShouldBlock(Context))
		{
			// Record the block for debugging.
			Reaction.LastBlockedTime = Now;
			Reaction.LastBlockedBy = Rule->GetClass()->GetDisplayNameText().ToString();

			UE_LOG(LogReactiveActors, Log, TEXT("%s: reaction %d (%s) blocked by %s"),
				*GetNameSafe(GetOwner()), ReactionIndex, *Event.ToString(), *Reaction.LastBlockedBy);
			return false;
		}
	}

	if (bRestart)
	{
		// Cancel the current run now. It counts as finishing (On Finish, Cancelled), so a Cooldown restarts
		// its timer; the new run goes ahead anyway, since the block check above already passed.
		Reaction.Sequence.Cancel();
		if (!HasBegunPlay())
		{
			return false; // a rule's On Finish ended this actor's play
		}

		// Called from inside an action (an event chain)? The reaction being restarted may be on this
		// call stack, so starting it again right here isn't safe: start the new run next frame instead.
		if (FActionRunner::IsAnyExecuting() && World)
		{
			// Weak: the instigator may be destroyed before next frame
			TWeakObjectPtr<AActor> WeakInstigator = Instigator;
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
				[this, ReactionIndex, Event, WeakInstigator]()
				{
					// Something else may have started it in the meantime, or play may have ended
					if (HasBegunPlay() && ActiveReactions.IsValidIndex(ReactionIndex)
						&& !ActiveReactions[ReactionIndex].Sequence.IsRunning())
					{
						FReactionContext Deferred;
						Deferred.Target = GetOwner();
						Deferred.Instigator = WeakInstigator.Get();
						Deferred.Event = Event;
						BeginRun(ReactionIndex, Deferred);
					}
				}));
			return true;
		}
	}

	BeginRun(ReactionIndex, Context);
	return true;
}

void UReactionComponent::BeginRun(int32 ReactionIndex, const FReactionContext& Context)
{
	INC_DWORD_STAT(STAT_ReactionsStarted);
	bStateChanged = true;

	FReaction& Reaction = ActiveReactions[ReactionIndex];

	for (UReactiveRule* Rule : Reaction.Rules)
	{
		if (Rule)
		{
			Rule->OnStart(Context);
		}
	}

	// Record the fire for debugging.
	const UWorld* World = GetWorld();
	Reaction.LastFiredTime = World ? World->GetTimeSeconds() : 0.0;
	Reaction.LastFiredEvent = Context.Event;

	// Notify any listeners that this reaction is about to fire (e.g. for UI feedback)
	OnReactionStarted.Broadcast(Context.Event, ReactionIndex);

	Reaction.Sequence.Start(this, Reaction.Actions, Context);
}

void UReactionComponent::HandleReactionComplete(int32 ReactionIndex, EReactionResult Result)
{
	if (!ActiveReactions.IsValidIndex(ReactionIndex))
	{
		return;
	}

	// Same context as the run's start
	FReaction& Reaction = ActiveReactions[ReactionIndex];
	const FReactionContext Context = Reaction.Sequence.GetContext();

	// For the debug overlay's "failed" line
	if (Result == EReactionResult::Failed)
	{
		const UWorld* World = GetWorld();
		Reaction.LastFailedTime = World ? World->GetTimeSeconds() : 0.0;
		const UReactiveAction* Failed = Reaction.Sequence.GetFailedAction();
		Reaction.LastFailedBy = Failed ? Failed->GetClass()->GetDisplayNameText().ToString() : FString(TEXT("an action"));

		if (GetDefault<UReactiveActorsSettings>()->bLogFailedReactions)
		{
			UE_LOG(LogReactiveActors, Log, TEXT("%s: reaction %s failed at %s."),
				*GetNameSafe(GetOwner()), *Reaction.EventTag.ToString(), *Reaction.LastFailedBy);
		}
	}

	for (UReactiveRule* Rule : Reaction.Rules)
	{
		if (Rule)
		{
			Rule->OnFinish(Context, Result);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			Subsystem->RecordReactionFinished(GetOwner(), Reaction.EventTag, Result, Reaction.LastFailedBy);
		}
	}

	OnReactionFinished.Broadcast(ReactionIndex, Result);
}

int32 UReactionComponent::CancelReactions(FGameplayTag Event)
{
	int32 Cancelled = 0;
	for (FReaction& Reaction : ActiveReactions)
	{
		if (!Reaction.Sequence.IsRunning())
		{
			continue;
		}

		// With an event: reactions listening for it (or a child), or running because of it right now
		if (Event.IsValid()
			&& !(Reaction.EventTag.IsValid() && Reaction.EventTag.MatchesTag(Event))
			&& !Reaction.Sequence.GetContext().Event.MatchesTag(Event))
		{
			continue;
		}

		Reaction.Sequence.Cancel(); // aborts its latent action, then finishes the run (rules notified)
		++Cancelled;
	}
	return Cancelled;
}

int32 UReactionComponent::ResetReactions(FGameplayTag Event)
{
	// Cancel first: a cancel counts as finishing, which would start a Cooldown. The reset then clears it.
	CancelReactions(Event);
	bStateChanged = true; // back to defaults, which may differ from what's cached

	int32 Reset = 0;
	for (FReaction& Reaction : ActiveReactions)
	{
		if (!Reaction.bEnabled)
		{
			continue;
		}
		if (Event.IsValid() && !(Reaction.EventTag.IsValid() && Reaction.EventTag.MatchesTag(Event)))
		{
			continue;
		}

		for (UReactiveRule* Rule : Reaction.Rules)
		{
			if (Rule)
			{
				Rule->OnReset();
			}
		}

		for (UReactiveAction* Action : Reaction.Actions)
		{
			if (Action)
			{
				Action->ResetAction(); // also resets actions nested inside it
			}
		}

		// Clear the debug overlay's fired / blocked lines too
		Reaction.LastFiredTime = -1.0;
		Reaction.LastBlockedTime = -1.0;
		Reaction.LastFailedTime = -1.0;
		++Reset;
	}

	// Triggers: with an event, only those sending it (or a child of it)
	for (UReactiveTrigger* Trigger : ActiveTriggers)
	{
		if (!Trigger)
		{
			continue;
		}
		if (Event.IsValid() && !(Trigger->GetEvent().IsValid() && Trigger->GetEvent().MatchesTag(Event)))
		{
			continue;
		}

		Trigger->ResetTrigger(); // its own On Reset, then its rules'
		++Reset;
	}
	return Reset;
}

FBox UReactionComponent::GetDebugBounds(const AActor* Actor)
{
	FBox Box(ForceInit);
	if (!Actor)
	{
		return Box;
	}

	// Only the parts you can see: editor icons (including our own), arrows and other editor-only bits are left out
	Actor->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors*/ false, [&Box](UPrimitiveComponent* Primitive)
		{
			if (Primitive->IsRegistered() && !Primitive->IsVisualizationComponent() && !Primitive->IsEditorOnly())
			{
				Box += Primitive->Bounds.GetBox();
			}
		});

	if (!Box.IsValid)
	{
		const FVector Location = Actor->GetActorLocation();
		Box = FBox(Location, Location);
	}
	return Box;
}

void UReactionComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	UWorld* World = GetWorld();

	// Editor worlds only: in PIE the debug overlay takes over (and the icon would make the actor's bounds taller)
	if (!EditorSprite && Root && World && !World->IsGameWorld() && GIsEditor && !IsRunningCommandlet())
	{
		EditorSprite = NewObject<UBillboardComponent>(Owner, NAME_None, RF_Transient | RF_TextExportTransient);
		EditorSprite->Sprite = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/S_Trigger.S_Trigger"));
		EditorSprite->bIsScreenSizeScaled = true;
		EditorSprite->SetHiddenInGame(true);
		EditorSprite->SetIsVisualizationComponent(true); // editor only: never saved, never in game
		EditorSprite->CreationMethod = EComponentCreationMethod::Instance;

		// Placed in world space (above the bounds, world up), so it ignores the actor's rotation and scale
		EditorSprite->SetUsingAbsoluteLocation(true);
		EditorSprite->SetUsingAbsoluteRotation(true);
		EditorSprite->SetUsingAbsoluteScale(true);
		EditorSprite->SetWorldScale3D(FVector(0.5f)); // half the engine's default icon size

		EditorSprite->SetupAttachment(Root);
		EditorSprite->RegisterComponentWithWorld(World);

		// Follow the actor when it's moved in the editor
		SpriteRoot = Root;
		RootTransformHandle = Root->TransformUpdated.AddUObject(this, &UReactionComponent::HandleRootTransformUpdated);

		UpdateEditorSpriteLocation();

		// The actor's other components may register after this one: place it again next frame, once they have bounds
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
			{
				UpdateEditorSpriteLocation();
				return false; // run once
			}));
	}
#endif
}

void UReactionComponent::OnUnregister()
{
#if WITH_EDITOR
	if (USceneComponent* Root = SpriteRoot.Get())
	{
		Root->TransformUpdated.Remove(RootTransformHandle);
	}
	SpriteRoot.Reset();
	RootTransformHandle.Reset();

	if (EditorSprite)
	{
		EditorSprite->DestroyComponent();
		EditorSprite = nullptr;
	}
#endif
	Super::OnUnregister();
}

#if WITH_EDITOR
void UReactionComponent::UpdateEditorSpriteLocation()
{
	if (!EditorSprite || !EditorSprite->IsRegistered())
	{
		return;
	}

	// Same height setting as the debug text (Reactions.Show.Offset), so both sit in the same place
	float Offset = 10.f;
	if (const IConsoleVariable* OffsetVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Reactions.Show.Offset")))
	{
		Offset = OffsetVar->GetFloat();
	}

	const FBox Bounds = GetDebugBounds(GetOwner());
	const FVector Center = Bounds.GetCenter();
	EditorSprite->SetWorldLocation(FVector(Center.X, Center.Y, Bounds.Max.Z + Offset));
}

void UReactionComponent::HandleRootTransformUpdated(USceneComponent* /*UpdatedComponent*/, EUpdateTransformFlags /*Flags*/, ETeleportType /*Teleport*/)
{
	UpdateEditorSpriteLocation();
}
#endif

// =================================================================================================
// Save and load
// =================================================================================================

TArray<UReactiveAction*> UReactionComponent::GatherActions(const FReaction& Reaction)
{
	// Depth first: each action, then the actions nested inside it (Branch, Random Action, Run On Targets)
	TArray<UReactiveAction*> Result;
	TFunction<void(UReactiveAction*, int32)> Add = [&Result, &Add](UReactiveAction* Action, int32 Depth)
		{
			if (!Action || Depth > 16)
			{
				return;
			}
			Result.Add(Action);
			TArray<UReactiveAction*> Nested;
			Action->GetNestedActions(Nested);
			for (UReactiveAction* Child : Nested)
			{
				Add(Child, Depth + 1);
			}
		};
	for (UReactiveAction* Action : Reaction.Actions)
	{
		Add(Action, 0);
	}
	return Result;
}

FReactiveActorState UReactionComponent::CaptureReactionState()
{
	FReactiveActorState State;
	State.Key = GetSaveKey();
	if (!bListsBuilt)
	{
		return State;
	}

	auto SaveRules = [](const TArray<TObjectPtr<UReactiveRule>>& Rules, TArray<FReactiveSavedObject>& Out)
		{
			for (UReactiveRule* Rule : Rules)
			{
				Out.Add(ReactiveSave::SaveObject(Rule)); // an empty slot saves as empty, keeping positions lined up
			}
		};

	for (int32 Index = 0; Index < ActiveReactions.Num(); ++Index)
	{
		const FReaction& Reaction = ActiveReactions[Index];
		FReactiveSavedReaction& Saved = State.Reactions.AddDefaulted_GetRef();
		Saved.ReactionId = Reaction.ReactionId;
		Saved.bFromPreset = Index < NumPresetReactions;
		Saved.Index = Saved.bFromPreset ? Index : Index - NumPresetReactions;
		Saved.EventTag = Reaction.EventTag;
		Saved.bEnabled = Reaction.bEnabled;
		SaveRules(Reaction.Rules, Saved.Rules);
		for (UReactiveAction* Action : GatherActions(Reaction))
		{
			Saved.Actions.Add(ReactiveSave::SaveObject(Action)); // empty for actions with nothing to save
		}
	}

	for (int32 Index = 0; Index < ActiveTriggers.Num(); ++Index)
	{
		UReactiveTrigger* Trigger = ActiveTriggers[Index];
		FReactiveSavedTrigger& Saved = State.Triggers.AddDefaulted_GetRef();
		Saved.bFromPreset = Index < NumPresetTriggers;
		Saved.Index = Saved.bFromPreset ? Index : Index - NumPresetTriggers;
		if (Trigger)
		{
			Saved.TriggerId = Trigger->TriggerId;
			Saved.Trigger = ReactiveSave::SaveObject(Trigger, /*bKeepClassWhenEmpty*/ true); // its class matches it up without an ID
			SaveRules(Trigger->GetRules(), Saved.Rules);
		}
	}
	return State;
}

void UReactionComponent::RestoreReactionState(const FReactiveActorState& State)
{

	if (!bListsBuilt || !HasBegunPlay())
	{
		// Too early: apply it at Begin Play, before triggers fire
		PendingState = State;
		bHasPendingState = true;
		return;
	}
	ApplyState(State, /*bLive*/ true);
}

void UReactionComponent::ApplyState(const FReactiveActorState& State, bool bLive)
{
	bStateChanged = true; // it now holds saved state, which belongs in the cache when it streams out

	if (bLive)
	{
		CancelReactions(FGameplayTag()); // a cancel counts as finishing (Cooldown starts); the restore below overwrites it
	}

	// Where a saved entry sits now: its position in the same list, if that still matches; otherwise the only
	// entry in that list that does (it moved); otherwise nowhere (removed or changed: skipped)
	auto FindSlot = [](int32 First, int32 End, int32 SavedIndex, TFunctionRef<bool(int32)> Matches) -> int32
		{
			const int32 AtSamePlace = First + SavedIndex;
			if (SavedIndex >= 0 && AtSamePlace < End && Matches(AtSamePlace))
			{
				return AtSamePlace;
			}
			int32 Found = INDEX_NONE;
			for (int32 Index = First; Index < End; ++Index)
			{
				if (Matches(Index))
				{
					if (Found != INDEX_NONE)
					{
						return INDEX_NONE; // more than one candidate: don't guess
					}
					Found = Index;
				}
			}
			return Found;
		};

	auto LoadRules = [](const TArray<TObjectPtr<UReactiveRule>>& Rules, const TArray<FReactiveSavedObject>& Saved)
		{
			for (int32 Index = 0; Index < Rules.Num() && Index < Saved.Num(); ++Index)
			{
				if (Rules[Index] && !Saved[Index].ClassPath.IsEmpty())
				{
					ReactiveSave::LoadObject(Rules[Index], Saved[Index]); // skipped if it's a different kind of rule now
				}
			}
		};

	bool bEnabledChanged = false;
	for (const FReactiveSavedReaction& Saved : State.Reactions)
	{
		const int32 First = Saved.bFromPreset ? 0 : NumPresetReactions;
		const int32 End = Saved.bFromPreset ? NumPresetReactions : ActiveReactions.Num();
		// By ID when both have one (survives reordering); otherwise by position, checked against the event tag
		int32 Slot = INDEX_NONE;
		if (Saved.ReactionId.IsValid())
		{
			Slot = ActiveReactions.IndexOfByPredicate([&Saved](const FReaction& Reaction) { return Reaction.ReactionId == Saved.ReactionId; });
		}
		if (Slot == INDEX_NONE)
		{
			Slot = FindSlot(First, End, Saved.Index, [this, &Saved](int32 Index)
				{
					return !(Saved.ReactionId.IsValid() && ActiveReactions[Index].ReactionId.IsValid()) // both have IDs and they differ: not it
						&& ActiveReactions[Index].EventTag == Saved.EventTag;
				});
		}
		if (Slot == INDEX_NONE)
		{
			UE_LOG(LogReactiveActors, Verbose, TEXT("%s: saved reaction %s (%d) no longer matches, skipped."),
				*GetNameSafe(GetOwner()), *Saved.EventTag.ToString(), Saved.Index);
			continue;
		}

		FReaction& Reaction = ActiveReactions[Slot];
		bEnabledChanged |= Reaction.bEnabled != Saved.bEnabled;
		Reaction.bEnabled = Saved.bEnabled;
		LoadRules(Reaction.Rules, Saved.Rules);

		// Actions by position (nested after their parent), each checked against its class
		const TArray<UReactiveAction*> Actions = GatherActions(Reaction);
		for (int32 Index = 0; Index < Actions.Num() && Index < Saved.Actions.Num(); ++Index)
		{
			ReactiveSave::LoadObject(Actions[Index], Saved.Actions[Index]); // skipped if empty or a different kind now
		}
	}

	for (const FReactiveSavedTrigger& Saved : State.Triggers)
	{
		const int32 First = Saved.bFromPreset ? 0 : NumPresetTriggers;
		const int32 End = Saved.bFromPreset ? NumPresetTriggers : ActiveTriggers.Num();
		int32 Slot = INDEX_NONE;
		if (Saved.TriggerId.IsValid())
		{
			Slot = ActiveTriggers.IndexOfByPredicate([&Saved](const UReactiveTrigger* Trigger) { return Trigger && Trigger->TriggerId == Saved.TriggerId; });
		}
		if (Slot == INDEX_NONE)
		{
			Slot = FindSlot(First, End, Saved.Index, [this, &Saved](int32 Index)
				{
					const UReactiveTrigger* Trigger = ActiveTriggers[Index];
					return Trigger && !(Saved.TriggerId.IsValid() && Trigger->TriggerId.IsValid())
						&& ReactiveSave::IsSameClass(Trigger->GetClass(), Saved.Trigger.ClassPath);
				});
		}
		if (Slot == INDEX_NONE)
		{
			continue;
		}

		UReactiveTrigger* Trigger = ActiveTriggers[Slot];
		ReactiveSave::LoadObject(Trigger, Saved.Trigger);
		LoadRules(Trigger->GetRules(), Saved.Rules);
	}

	// Playing already: listen for what's enabled now (as Set Reactions Enabled does)
	if (bLive && bEnabledChanged)
	{
		if (UReactionSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UReactionSubsystem>() : nullptr)
		{
			Subsystem->UnregisterEvent(this, RegisteredTags);
			RegisteredTags = GetEventTags();
			Subsystem->RegisterEvent(this, RegisteredTags);
		}
	}
}

bool UReactionComponent::IsPlacedInLevel() const
{
	// Loaded with its level (including streamed and World Partition levels), not spawned after: Unreal marks these
	// as "net startup" actors when the level is added to the world, in every net mode
	const AActor* Owner = GetOwner();
	return Owner && Owner->IsNetStartupActor();
}

FString UReactionComponent::GetSaveKey() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return GetName();
	}

	// World Partition actor names are unique across the whole world. Otherwise two levels can each have a BP_Lamp_C_1,
	// so the actor's package (its level, or its own file with One File Per Actor) goes first, without the PIE prefix.
	const UWorld* World = GetWorld();
	FString ActorPart;
	if (World && World->IsPartitionedWorld())
	{
		ActorPart = Owner->GetName();
	}
	else
	{
		const UPackage* Package = Owner->GetPackage();
		ActorPart = FString::Printf(TEXT("%s:%s"), Package ? *UWorld::RemovePIEPrefix(Package->GetName()) : TEXT(""), *Owner->GetName());
	}
	return FString::Printf(TEXT("%s.%s"), *ActorPart, *GetName());
}

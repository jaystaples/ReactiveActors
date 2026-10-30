// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions: event

#include "Actions/EventActions.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ReactionComponent.h"
#include "ReactionSubsystem.h"
#include "ReactiveActorsLog.h"

// ---- Broadcast Event ----------------------------------------------------------------------

void UACT_BroadcastEvent::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	AActor* Target = Context.Target; // never null here
	if (!Event.IsValid())
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: no Event set."), *GetClass()->GetName(), *GetNameSafe(Target));
		FinishAction();
		return;
	}

	UWorld* World = GetWorld();
	UReactionSubsystem* Subsystem = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
	if (!Subsystem)
	{
		FinishAction();
		return;
	}

	// Through the subsystem in every scope, so Event Received triggers (World) hear it too
	switch (Scope)
	{
	case EReactiveScopePolicy::World:
		// The target is the source, so the debug wiring lines start from it
		Subsystem->BroadcastEventFrom(Event, GetInstigator(), Target);
		break;

	case EReactiveScopePolicy::Self:
		Subsystem->BroadcastEventToActor(Target, Event, GetInstigator(), Target);
		break;

	case EReactiveScopePolicy::Identity:
		if (TargetIdentity.IsValid())
		{
			Subsystem->BroadcastEventToIdentity(TargetIdentity, Event, GetInstigator(), Target);
		}
		else
		{
			UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: Identity scope but no Target Identity set."),
				*GetClass()->GetName(), *GetNameSafe(Target));
		}
		break;
	}

	// Sending is instant (the reactions it starts run on their own)
	FinishAction();
}

// ---- Set Reaction Enabled -----------------------------------------------------------------------

void UACT_SetReactionEnabled::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	UReactionComponent* Component = Context.Target ? Context.Target->FindComponentByClass<UReactionComponent>() : nullptr;
	if (!Component)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s: %s has no Reaction Component."), *GetClass()->GetName(), *GetNameSafe(Context.Target));
		FinishAction();
		return;
	}
	Component->SetReactionsEnabled(Event, bEnableReactions);
	FinishAction();
}

FString UACT_SetReactionEnabled::DescribeAction() const
{
	return FString::Printf(TEXT("%s %s"), bEnableReactions ? TEXT("Enable") : TEXT("Disable"),
		Event.IsValid() ? *FString::Printf(TEXT("reactions to %s"), *Event.ToString()) : TEXT("all reactions"));
}

// ---- Reset Reactions ----------------------------------------------------------------------------

void UACT_ResetReactions::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	UReactionComponent* Component = Context.Target ? Context.Target->FindComponentByClass<UReactionComponent>() : nullptr;
	if (!Component)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s: %s has no Reaction Component."), *GetClass()->GetName(), *GetNameSafe(Context.Target));
		FinishAction();
		return;
	}

	// If this action is itself inside one of the reactions being reset, that reaction is cancelled here,
	// and this action with it: Finish Action is then ignored.
	Component->ResetReactions(Event);
	if (IsRunning())
	{
		FinishAction();
	}
}

FString UACT_ResetReactions::DescribeAction() const
{
	return Event.IsValid() ? FString::Printf(TEXT("Reset Reactions to %s"), *Event.ToString()) : FString(TEXT("Reset all Reactions"));
}

// ---- Summaries (the row titles in the editor) ---------------------------------------------------

FString UACT_BroadcastEvent::DescribeAction() const
{
	return FString::Printf(TEXT("Broadcast %s -> %s"), *ReactiveSummary::Tag(Event), *ReactiveSummary::Scope(Scope, TargetIdentity));
}

// ---- Validate Settings (Data Validation and Map Check) ----------------------------------------

void UACT_BroadcastEvent::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Event.IsValid())
	{
		Errors.Add(INVTEXT("Event isn't set, so it sends nothing."));
	}
	if (Scope == EReactiveScopePolicy::Identity && !TargetIdentity.IsValid())
	{
		Errors.Add(INVTEXT("Identity scope but no Target Identity, so the event goes nowhere."));
	}
}

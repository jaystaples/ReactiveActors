// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// The Reaction Subsystem's debug tools, kept apart from its event routing (ReactionSubsystem.cpp):
// console commands, the Reactions.Show overlay, wiring lines and the event history.
// Almost everything here is compiled out of shipping builds.

#include "ReactionSubsystem.h"

#include "ActionRunner.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Reaction.h"
#include "ReactionComponent.h"
#include "ReactiveAction.h"
#include "ReactiveRule.h"
#include "ReactiveTrigger.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

// =================================================================================================
// Debug settings (not compiled into shipping builds)
//
// Console commands (defined further down):
//   Reactions.DumpListeners [Tag]       Every event tag being listened for, and which actors listen.
//                                         Optional tag filter (shows that tag, its parents and children).
//   Reactions.DumpActors                Every actor with a Reaction Component and its reactions.
//   Reactions.Broadcast <Tag>           Broadcasts an event, as if Broadcast Event was called.
//   Reactions.FindIdentity <Tag>        Lists the actors with that identity (or a child of it).
//   Reactions.Suspend [0|1]             Suspends (1, default) or resumes (0) all events.
//   Reactions.CancelAll                 Cancels every running reaction and Run Action.
//   Reactions.ResetAll                  Resets every reaction (cancels, then rules get On Reset).
//   Reactions.DumpRuns                  Lists the one-off runs still going (Run Action and friends).
//
// Overlay above actors (console variables):
//   Reactions.Show 1                    Each actor's identity, and each reaction's status.
//   Reactions.Show.Distance <cm>        Only for actors this close to the camera (default 3000).
//   Reactions.Show.Offset <cm>          Height above the actor's bounds for the text and editor icon (default 10).
//   Reactions.Show.Linger <s>           How long 'fired', 'blocked' and 'skipped' lines stay (default 3, 0 = off).
//   Reactions.Show.StuckTime <s>        An action that hasn't called Finish Action after this long is flagged
//                                         (default 15, 0 = never): a long wait, or a forgotten Finish Action.
//   Reactions.Show.Rules 1              Each rule's live state, e.g. Max Activations 2/3 (default on).
//   Reactions.Show.Filter <Tag>         Only actors listening for, or identified by, this tag (or a child).
//                                         Reactions.Show.Filter None clears it.
//
// Wiring lines (independent of Reactions.Show):
//   Reactions.Show.WiringLines 1        Arrow from each event's source to every actor it reached
//                                         (green = fired, red = heard but didn't fire). Run On Targets too.
//   Reactions.Show.WiringTime <s>       How long each line stays (default 3).
//
// History:
//   Reactions.History [N]               Prints the last N events sent (who sent them, who they reached,
//                                         who fired) and how each reaction ended. Default 20.
//   Reactions.Show.History <N>          The same, live on screen: the last N lines (0 = off).
//   Reactions.History.Size <N>          How many lines are kept (default 100).
// =================================================================================================

#if !UE_BUILD_SHIPPING

static TAutoConsoleVariable<int32> CVarReactionsShow(
	TEXT("Reactions.Show"), 0,
	TEXT("Draw reaction status above actors. 0 = off, 1 = on."));

static TAutoConsoleVariable<float> CVarReactionsShowDistance(
	TEXT("Reactions.Show.Distance"), 3000.f,
	TEXT("Max distance (cm) from the camera to draw reaction status."));

static TAutoConsoleVariable<float> CVarReactionsShowOffset(
	TEXT("Reactions.Show.Offset"), 10.f,
	TEXT("Height (cm) above the actor's bounds for the debug text and the editor icon."));

static TAutoConsoleVariable<float> CVarReactionsShowLinger(
	TEXT("Reactions.Show.Linger"), 3.f,
	TEXT("Seconds 'fired', 'blocked' and 'skipped' lines stay on the overlay. 0 or less = off."));

static TAutoConsoleVariable<float> CVarReactionsShowStuckTime(
	TEXT("Reactions.Show.StuckTime"), 15.f,
	TEXT("Seconds an action can go without calling Finish Action before the overlay flags it as stuck. 0 or less = never."));

static TAutoConsoleVariable<int32> CVarReactionsShowRules(
	TEXT("Reactions.Show.Rules"), 1,
	TEXT("Show each rule's live state on the overlay (Get Debug String). 0 = off, 1 = on."));

static TAutoConsoleVariable<FString> CVarReactionsShowFilter(
	TEXT("Reactions.Show.Filter"), TEXT(""),
	TEXT("Only show actors listening for, or identified by, this tag (or a child of it). None = no filter."));

static TAutoConsoleVariable<int32> CVarReactionsShowWiringLines(
	TEXT("Reactions.Show.WiringLines"), 0,
	TEXT("Draw a line from each event's source to every actor it reached (green = fired, red = didn't). 0 = off, 1 = on."));

static TAutoConsoleVariable<float> CVarReactionsShowWiringTime(
	TEXT("Reactions.Show.WiringTime"), 3.f,
	TEXT("Seconds each wiring line stays on screen."));

static TAutoConsoleVariable<int32> CVarReactionsShowHistory(
	TEXT("Reactions.Show.History"), 0,
	TEXT("Show the last events and reaction results on screen (works without Reactions.Show). 0 = off, N = that many lines."));

static TAutoConsoleVariable<int32> CVarReactionsHistorySize(
	TEXT("Reactions.History.Size"), 100,
	TEXT("How many lines the event history keeps (Reactions.History)."));

namespace ReactionsDebug
{
	/** The filter tag, if one is set. bOutBadTag = something was typed that isn't a gameplay tag. */
	static FGameplayTag GetFilterTag(bool& bOutBadTag)
	{
		bOutBadTag = false;
		const FString Value = CVarReactionsShowFilter.GetValueOnGameThread().TrimStartAndEnd();
		if (Value.IsEmpty() || Value.Equals(TEXT("None"), ESearchCase::IgnoreCase) || Value == TEXT("\"\""))
		{
			return FGameplayTag();
		}

		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Value), /*ErrorIfNotFound*/ false);
		bOutBadTag = !Tag.IsValid();
		return Tag;
	}

	/** True if the component listens for Filter (or a child, or a parent that would hear it) or has it as an identity. */
	static bool PassesFilter(const UReactionComponent* Component, const FGameplayTag& Filter)
	{
		if (!Filter.IsValid())
		{
			return true;
		}
		if (Component->GetIdentityTags().HasTag(Filter))
		{
			return true; // Actor.Kitchen.Lamp passes a filter of Actor.Kitchen
		}
		for (const FReaction& Reaction : Component->GetActiveReactions())
		{
			if (Reaction.bEnabled && Reaction.EventTag.IsValid()
				&& (Reaction.EventTag.MatchesTag(Filter) || Filter.MatchesTag(Reaction.EventTag)))
			{
				return true;
			}
		}
		return false;
	}
}

#endif // !UE_BUILD_SHIPPING

// =================================================================================================
// History and wiring lines
// =================================================================================================

void UReactionSubsystem::RecordReactionFinished(const AActor* Actor, const FGameplayTag& EventTag, EReactionResult Result, const FString& FailedAt)
{
#if !UE_BUILD_SHIPPING
	FString Text = FString::Printf(TEXT("    %s: reaction %s "), *GetNameSafe(Actor), *EventTag.ToString());
	FColor Color = FColor(140, 200, 140);
	switch (Result)
	{
	case EReactionResult::Failed:
		Text += FString::Printf(TEXT("FAILED at %s"), *FailedAt);
		Color = FColor::Red;
		break;
	case EReactionResult::Cancelled:
		Text += TEXT("cancelled");
		Color = FColor::Orange;
		break;
	default:
		Text += TEXT("succeeded");
		break;
	}
	AddHistory(MoveTemp(Text), Color);
#endif
}

#if !UE_BUILD_SHIPPING
void UReactionSubsystem::AddHistory(FString Text, FColor Color)
{
	const UWorld* World = GetWorld();
	FHistoryLine& Line = History.AddDefaulted_GetRef();
	Line.Time = World ? World->GetTimeSeconds() : 0.0;
	Line.Text = MoveTemp(Text);
	Line.Color = Color;

	const int32 MaxLines = FMath::Max(1, CVarReactionsHistorySize.GetValueOnGameThread());
	if (History.Num() > MaxLines)
	{
		History.RemoveAt(0, History.Num() - MaxLines);
	}
}
#endif

#if !UE_BUILD_SHIPPING
void UReactionSubsystem::RecordEventHistory(const FReactiveEventInfo& Info, const TArray<TPair<FString, bool>>& Receivers)
{
	// History line: "Event.Door.Open  from Switch (by Player) -> World: Door (fired), Lamp (no reaction)"
	{
		FString Route;
		switch (Info.Route)
		{
		case EReactiveEventRoute::Actor:    Route = FString::Printf(TEXT("to %s"), *GetNameSafe(Info.TargetActor)); break;
		case EReactiveEventRoute::Identity: Route = FString::Printf(TEXT("to identity %s"), *Info.TargetIdentity.ToString()); break;
		default:                            Route = TEXT("to World"); break;
		}

		TArray<FString> Parts;
		bool bAnyFired = false;
		for (const TPair<FString, bool>& Receiver : Receivers)
		{
			Parts.Add(FString::Printf(TEXT("%s (%s)"), *Receiver.Key, Receiver.Value ? TEXT("fired") : TEXT("no reaction")));
			bAnyFired |= Receiver.Value;
		}

		FString From;
		if (Info.Source)
		{
			From = FString::Printf(TEXT("  from %s"), *GetNameSafe(Info.Source));
		}
		if (Info.Instigator && Info.Instigator != Info.Source)
		{
			From += FString::Printf(TEXT(" (by %s)"), *GetNameSafe(Info.Instigator));
		}

		AddHistory(FString::Printf(TEXT("%s%s  %s: %s"), *Info.Event.ToString(), *From, *Route,
			Parts.IsEmpty() ? TEXT("nobody listening") : *FString::Join(Parts, TEXT(", "))),
			bAnyFired ? FColor::Green : FColor(180, 180, 180));
	}
}
#endif

void UReactionSubsystem::DrawDebugWire(const AActor* From, const AActor* To, const FGameplayTag& Event, bool bFired) const
{
#if !UE_BUILD_SHIPPING
	// No source (console, a Blueprint broadcast with no Instigator) is fine: we draw a drop-in arrow instead.
	// Same actor at both ends: nothing to draw (the overlay already shows it fired).
	if (CVarReactionsShowWiringLines.GetValueOnGameThread() == 0 || !IsValid(To) || From == To)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Respect the overlay filter: the event, or the actor it reached, must match
	bool bBadTag = false;
	const FGameplayTag Filter = ReactionsDebug::GetFilterTag(bBadTag);
	if (Filter.IsValid() && !Event.MatchesTag(Filter))
	{
		const UReactionComponent* ToComponent = To->FindComponentByClass<UReactionComponent>();
		if (!ToComponent || !ReactionsDebug::PassesFilter(ToComponent, Filter))
		{
			return;
		}
	}

	const float Time = FMath::Max(CVarReactionsShowWiringTime.GetValueOnGameThread(), 0.1f);
	const FBox ToBounds = UReactionComponent::GetDebugBounds(To);
	const FVector End = ToBounds.GetCenter();

	// From the source if there is one; otherwise straight down onto the actor from above its bounds
	const FVector Start = IsValid(From)
		? UReactionComponent::GetDebugBounds(From).GetCenter()
		: FVector(End.X, End.Y, ToBounds.Max.Z + 150.f);
	const FColor Color = bFired ? FColor::Green : FColor::Red;

	DrawDebugDirectionalArrow(World, Start, End, /*ArrowSize*/ 40.f, Color, /*bPersistent*/ false, Time, /*DepthPriority*/ 0, /*Thickness*/ 2.f);
	DrawDebugString(World, (Start + End) * 0.5f, Event.IsValid() ? Event.ToString() : FString(TEXT("<no event>")),
		/*TestBaseActor*/ nullptr, Color, Time, /*bDrawShadow*/ true);
#endif
}

// =================================================================================================
// Console commands (not compiled into shipping builds)
// =================================================================================================

#if !UE_BUILD_SHIPPING

namespace ReactionsDebug
{
	static UReactionSubsystem* GetSubsystem(UWorld* World, FOutputDevice& Ar)
	{
		UReactionSubsystem* Subsystem = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
		if (!Subsystem)
		{
			Ar.Log(TEXT("Reactions: no Reaction Subsystem in this world. Run this while playing (PIE or game)."));
		}
		return Subsystem;
	}

	static FString OwnerName(const UActorComponent* Component)
	{
		const AActor* Owner = Component ? Component->GetOwner() : nullptr;
		return Owner ? Owner->GetActorNameOrLabel() : FString(TEXT("<no owner>"));
	}

	/** Parses a tag argument; logs and returns an invalid tag if it doesn't exist. */
	static FGameplayTag ParseTag(const FString& TagString, FOutputDevice& Ar)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagString), /*ErrorIfNotFound*/ false);
		if (!Tag.IsValid())
		{
			Ar.Logf(TEXT("Reactions: '%s' is not a registered gameplay tag."), *TagString);
		}
		return Tag;
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsDumpListeners(
	TEXT("Reactions.DumpListeners"),
	TEXT("Lists every event tag being listened for and the actors listening. Optional: a tag to filter by."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			const UReactionSubsystem* Subsystem = ReactionsDebug::GetSubsystem(World, Ar);
			if (!Subsystem)
			{
				return;
			}

			FGameplayTag Filter;
			if (Args.Num() > 0)
			{
				Filter = ReactionsDebug::ParseTag(Args[0], Ar);
				if (!Filter.IsValid())
				{
					return;
				}
			}

			const auto& Registry = Subsystem->GetComponentsByEvent();

			TArray<FGameplayTag> Tags;
			Registry.GetKeys(Tags);
			Tags.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.ToString() < B.ToString(); });

			int32 ShownTags = 0;
			Ar.Log(TEXT("---- Reactions: listened event tags ----"));
			for (const FGameplayTag& Tag : Tags)
			{
				// With a filter: show the filter tag, its parents (they'd hear it) and its children
				if (Filter.IsValid() && !Filter.MatchesTag(Tag) && !Tag.MatchesTag(Filter))
				{
					continue;
				}

				TArray<FString> Names;
				for (const TWeakObjectPtr<UReactionComponent>& Weak : Registry[Tag])
				{
					if (const UReactionComponent* Component = Weak.Get())
					{
						Names.Add(ReactionsDebug::OwnerName(Component));
					}
				}
				Names.Sort();

				Ar.Logf(TEXT("%s  (%d)"), *Tag.ToString(), Names.Num());
				for (const FString& Name : Names)
				{
					Ar.Logf(TEXT("    %s"), *Name);
				}
				++ShownTags;
			}
			Ar.Logf(TEXT("---- %d tag(s) ----"), ShownTags);
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsDumpActors(
	TEXT("Reactions.DumpActors"),
	TEXT("Lists every actor with a Reaction Component, with its reactions and actions."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			if (!World)
			{
				return;
			}

			TArray<const UReactionComponent*> Components;
			for (TObjectIterator<UReactionComponent> It; It; ++It)
			{
				if (It->GetWorld() == World && It->HasBegunPlay())
				{
					Components.Add(*It);
				}
			}
			Components.Sort([](const UReactionComponent& A, const UReactionComponent& B)
				{
					return ReactionsDebug::OwnerName(&A) < ReactionsDebug::OwnerName(&B);
				});

			Ar.Log(TEXT("---- Reactions: actors with Reaction Components ----"));
			for (const UReactionComponent* Component : Components)
			{
				const TArray<FReaction>& Reactions = Component->GetActiveReactions();
				const FGameplayTagContainer Identity = Component->GetIdentityTags();
				Ar.Logf(TEXT("%s  (%d reaction(s))%s%s"), *ReactionsDebug::OwnerName(Component), Reactions.Num(),
					Identity.IsEmpty() ? TEXT("") : TEXT("  Identity: "),
					Identity.IsEmpty() ? TEXT("") : *Identity.ToStringSimple());

				for (int32 Index = 0; Index < Reactions.Num(); ++Index)
				{
					const FReaction& Reaction = Reactions[Index];

					TArray<FString> ActionNames;
					for (const UReactiveAction* Action : Reaction.Actions)
					{
						ActionNames.Add(Action ? Action->GetClass()->GetDisplayNameText().ToString() : FString(TEXT("<empty>")));
					}

					TArray<FString> RuleNames;
					for (const UReactiveRule* Rule : Reaction.Rules)
					{
						RuleNames.Add(Rule ? Rule->GetClass()->GetDisplayNameText().ToString() : FString(TEXT("<empty>")));
					}

					Ar.Logf(TEXT("    [%d] %s%s%s"),
						Index,
						Reaction.EventTag.IsValid() ? *Reaction.EventTag.ToString() : TEXT("<no tag>"),
						Reaction.bEnabled ? TEXT("") : TEXT("  (disabled)"),
						Reaction.Sequence.IsRunning() ? TEXT("  (running)") : TEXT(""));

					if (RuleNames.Num() > 0)
					{
						Ar.Logf(TEXT("        Rules: %s"), *FString::Join(RuleNames, TEXT(", ")));
					}
					Ar.Logf(TEXT("        Actions:   %s"), ActionNames.Num() > 0 ? *FString::Join(ActionNames, TEXT(", ")) : TEXT("<none>"));
				}

				for (const UReactiveTrigger* Trigger : Component->GetTriggers())
				{
					if (Trigger)
					{
						Ar.Logf(TEXT("    trigger %s -> %s%s"),
							*Trigger->GetClass()->GetDisplayNameText().ToString(),
							Trigger->GetEvent().IsValid() ? *Trigger->GetEvent().ToString() : TEXT("<no event>"),
							Trigger->IsEnabled() ? TEXT("") : TEXT("  (disabled)"));
					}
				}
			}
			Ar.Logf(TEXT("---- %d actor(s) ----"), Components.Num());
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsBroadcast(
	TEXT("Reactions.Broadcast"),
	TEXT("Broadcasts an event to every listening component. Usage: Reactions.Broadcast <Tag>"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			if (Args.Num() == 0)
			{
				Ar.Log(TEXT("Usage: Reactions.Broadcast <Tag>"));
				return;
			}

			UReactionSubsystem* Subsystem = ReactionsDebug::GetSubsystem(World, Ar);
			const FGameplayTag Tag = ReactionsDebug::ParseTag(Args[0], Ar);
			if (!Subsystem || !Tag.IsValid())
			{
				return;
			}

			const bool bAnyFired = Subsystem->BroadcastEvent(Tag);
			Ar.Logf(TEXT("Reactions: broadcast %s -> %s"), *Tag.ToString(),
				bAnyFired ? TEXT("at least one reaction fired") : TEXT("nothing fired"));
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsFindIdentity(
	TEXT("Reactions.FindIdentity"),
	TEXT("Lists the actors whose Reaction Component has this identity (or a child of it). Usage: Reactions.FindIdentity <Tag>"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			if (Args.Num() == 0)
			{
				Ar.Log(TEXT("Usage: Reactions.FindIdentity <Tag>"));
				return;
			}

			const UReactionSubsystem* Subsystem = ReactionsDebug::GetSubsystem(World, Ar);
			const FGameplayTag Tag = ReactionsDebug::ParseTag(Args[0], Ar);
			if (!Subsystem || !Tag.IsValid())
			{
				return;
			}

			const TArray<AActor*> Actors = Subsystem->GetActorsByIdentity(Tag);
			Ar.Logf(TEXT("---- Reactions: identity %s (%d) ----"), *Tag.ToString(), Actors.Num());
			for (const AActor* Actor : Actors)
			{
				Ar.Logf(TEXT("    %s"), *Actor->GetActorNameOrLabel());
			}
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsSuspend(
	TEXT("Reactions.Suspend"),
	TEXT("Suspends (1, the default) or resumes (0) all events. Usage: Reactions.Suspend [0|1]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			UReactionSubsystem* Subsystem = ReactionsDebug::GetSubsystem(World, Ar);
			if (!Subsystem)
			{
				return;
			}

			const bool bSuspend = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			Subsystem->SetEventsSuspended(bSuspend);
			Ar.Logf(TEXT("Reactions: events %s."), bSuspend ? TEXT("suspended") : TEXT("resumed"));
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsCancelAll(
	TEXT("Reactions.CancelAll"),
	TEXT("Cancels every running reaction and every action started with Run Action."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			UReactionSubsystem* Subsystem = ReactionsDebug::GetSubsystem(World, Ar);
			if (!Subsystem)
			{
				return;
			}

			const int32 Cancelled = Subsystem->CancelAllReactions();
			Ar.Logf(TEXT("Reactions: cancelled %d run(s)."), Cancelled);
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsResetAll(
	TEXT("Reactions.ResetAll"),
	TEXT("Resets every reaction as if it had never fired: cancels running ones, then rules get On Reset."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			UReactionSubsystem* Subsystem = ReactionsDebug::GetSubsystem(World, Ar);
			if (!Subsystem)
			{
				return;
			}

			const int32 Reset = Subsystem->ResetAllReactions();
			Ar.Logf(TEXT("Reactions: reset %d reaction(s)."), Reset);
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsHistory(
	TEXT("Reactions.History"),
	TEXT("Prints the last N events sent (who sent them, who they reached, who fired) and how each reaction ended. Usage: Reactions.History [N] (default 20)"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			const UReactionSubsystem* Subsystem = ReactionsDebug::GetSubsystem(World, Ar);
			if (!Subsystem)
			{
				return;
			}

			const int32 Count = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 20;
			const auto& History = Subsystem->GetHistory();
			const int32 First = FMath::Max(0, History.Num() - Count);
			Ar.Logf(TEXT("---- Reactions: last %d of %d history lines (now %.1fs) ----"), History.Num() - First, History.Num(), World->GetTimeSeconds());
			for (int32 Index = First; Index < History.Num(); ++Index)
			{
				Ar.Logf(TEXT("%7.2fs  %s"), History[Index].Time, *History[Index].Text);
			}
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GReactionsDumpRuns(
	TEXT("Reactions.DumpRuns"),
	TEXT("Lists the one-off runs still going (Run Action, Run Actions, Run Reaction): what, on which target, and what they're waiting on."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			const UReactionSubsystem* Subsystem = ReactionsDebug::GetSubsystem(World, Ar);
			if (!Subsystem)
			{
				return;
			}

			const double Now = World->GetTimeSeconds();
			const TArray<TObjectPtr<UReactionOneShot>>& Runs = Subsystem->GetOneShots();
			Ar.Logf(TEXT("---- Reactions: one-off runs (%d) ----"), Runs.Num());

			for (const UReactionOneShot* Run : Runs)
			{
				if (!Run)
				{
					continue;
				}

				TArray<FString> ActionNames;
				for (const UReactiveAction* Action : Run->Actions)
				{
					ActionNames.Add(Action ? Action->GetClass()->GetDisplayNameText().ToString() : FString(TEXT("<empty>")));
				}

				const AActor* Target = Run->Sequence.GetContext().Target.Get();
				Ar.Logf(TEXT("%s  [%s]  target %s  running %.1fs"),
					*Run->Label, *FString::Join(ActionNames, TEXT(", ")),
					IsValid(Target) ? *Target->GetActorNameOrLabel() : TEXT("<gone>"),
					Run->StartTime >= 0.0 ? Now - Run->StartTime : 0.0);

				if (const UReactiveAction* Waiting = Run->Sequence.GetWaitingOn())
				{
					const double WaitStart = Run->Sequence.GetWaitStartTime();
					Ar.Logf(TEXT("    step %d/%d, waiting on %s for %.1fs"),
						Run->Sequence.GetCurrentStep(), Run->Sequence.GetNumSteps(),
						*Waiting->GetClass()->GetDisplayNameText().ToString(),
						WaitStart >= 0.0 ? Now - WaitStart : 0.0);
				}
			}
		}));

// =================================================================================================
// Overlay drawing (not compiled into shipping builds)
// =================================================================================================

namespace ReactionsDebug
{
	/** Adds Action and everything nested inside it (Branch, Run On Targets) to Out. */
	static void GatherActions(const UReactiveAction* Action, TArray<const UReactiveAction*>& Out, int32 Depth = 0)
	{
		if (!Action || Depth > 16)
		{
			return;
		}
		Out.Add(Action);

		TArray<UReactiveAction*> Nested;
		Action->GetNestedActions(Nested);
		for (const UReactiveAction* Child : Nested)
		{
			GatherActions(Child, Out, Depth + 1);
		}
	}

	/**
	 * "2/4 Branch > 1/2 Delay": the step each running sequence is on, following latent actions that run
	 * their own sequence. A sequence running on another actor (Run On Targets) shows that actor's name.
	 * OutWaitStart = when the deepest latent action started waiting (-1 if none).
	 */
	static FString DescribeRunning(const FActionRunner& Sequence, const AActor* Owner, double& OutWaitStart)
	{
		FString Out;
		OutWaitStart = -1.0;

		const FActionRunner* Current = &Sequence;
		for (int32 Depth = 0; Current && Depth < 8; ++Depth)
		{
			if (Depth > 0)
			{
				Out += TEXT(" > ");
				const AActor* SequenceTarget = Current->GetContext().Target.Get();
				if (IsValid(SequenceTarget) && SequenceTarget != Owner)
				{
					Out += FString::Printf(TEXT("[%s] "), *SequenceTarget->GetActorNameOrLabel());
				}
			}

			Out += FString::Printf(TEXT("%d/%d"), Current->GetCurrentStep(), Current->GetNumSteps());

			const UReactiveAction* Waiting = Current->GetWaitingOn();
			if (!Waiting)
			{
				break;
			}
			Out += TEXT(" ") + Waiting->GetClass()->GetDisplayNameText().ToString();
			OutWaitStart = Current->GetWaitStartTime();

			Current = Waiting->GetActiveInnerSequence();
		}
		return Out;
	}

	/** "  [Max Activations 2/3, Cooldown 1.4s left]", or empty if no rule has anything to say. */
	static FString DescribeRules(const TArray<TObjectPtr<UReactiveRule>>& Rules)
	{
		TArray<FString> Parts;
		for (const UReactiveRule* Rule : Rules)
		{
			if (!Rule)
			{
				continue;
			}
			const FString Detail = Rule->GetDebugString();
			if (!Detail.IsEmpty())
			{
				Parts.Add(FString::Printf(TEXT("%s %s"), *Rule->GetClass()->GetDisplayNameText().ToString(), *Detail));
			}
		}
		return Parts.IsEmpty() ? FString() : FString::Printf(TEXT("  [%s]"), *FString::Join(Parts, TEXT(", ")));
	}

	/** Draws one actor's identity and the status of each enabled reaction above it, for one frame. */
	static void DrawComponentStatus(UWorld* World, const UReactionComponent* Component)
	{
		AActor* Owner = Component->GetOwner();
		const double Now = World->GetTimeSeconds();
		const float Linger = CVarReactionsShowLinger.GetValueOnGameThread();
		const float StuckTime = CVarReactionsShowStuckTime.GetValueOnGameThread();
		const bool bShowRules = CVarReactionsShowRules.GetValueOnGameThread() != 0;

		// True if Time was set and is within the linger window
		auto IsRecent = [Now, Linger](double Time) { return Linger > 0.f && Time >= 0.0 && (Now - Time) <= Linger; };

		FString Text = OwnerName(Component);
		const FGameplayTagContainer Identity = Component->GetIdentityTags();
		if (!Identity.IsEmpty())
		{
			Text += FString::Printf(TEXT("  [%s]"), *Identity.ToStringSimple());
		}

		bool bAnyStuck = false;
		bool bAnyRunning = false;
		bool bAnyFired = false;
		bool bAllBlocked = true;
		int32 Shown = 0;

		for (const FReaction& Reaction : Component->GetActiveReactions())
		{
			if (!Reaction.bEnabled)
			{
				continue; // disabled: not shown, not counted
			}
			++Shown;

			FString Status = TEXT("waiting");
			bool bBlocked = false;

			if (Reaction.Sequence.IsRunning())
			{
				bAnyRunning = true;

				double WaitStart = -1.0;
				Status = TEXT("running: ") + DescribeRunning(Reaction.Sequence, Owner, WaitStart);
				if (WaitStart >= 0.0)
				{
					const double Waited = Now - WaitStart;
					Status += FString::Printf(TEXT(" (%.1fs)"), Waited);
					if (StuckTime > 0.f && Waited >= StuckTime)
					{
						bAnyStuck = true;
						Status += TEXT("  STUCK? never finished");
					}
				}
			}
			else if (IsRecent(Reaction.LastFailedTime) && Reaction.LastFailedTime >= Reaction.LastFiredTime)
			{
				// Ran, but an action finished with Success off (or a Stop Reaction ended it as Failed)
				bAnyFired = true;
				Status = FString::Printf(TEXT("failed at %s (%.1fs ago)"), *Reaction.LastFailedBy, Now - Reaction.LastFailedTime);
			}
			else if (IsRecent(Reaction.LastFiredTime))
			{
				bAnyFired = true;
				Status = FString::Printf(TEXT("fired (%.1fs ago)"), Now - Reaction.LastFiredTime);

				// Only name the event when it's a child of the reaction's tag (Event.Photo heard Event.Photo.Kitchen)
				if (Reaction.LastFiredEvent != Reaction.EventTag)
				{
					Status += FString::Printf(TEXT(" by %s"), *Reaction.LastFiredEvent.ToString());
				}
			}
			else
			{
				// Live check, C++ rules only: cheap and side-effect free.
				// Blueprint rules are only reported from real attempts (below), never called from here.
				FReactionContext Probe;
				Probe.Target = Owner;
				Probe.Event = Reaction.EventTag;
				for (const UReactiveRule* Rule : Reaction.Rules)
				{
					if (Rule && Rule->GetClass()->HasAnyClassFlags(CLASS_Native) && Rule->ShouldBlock(Probe))
					{
						Status = FString::Printf(TEXT("blocked: %s"), *Rule->GetClass()->GetDisplayNameText().ToString());
						bBlocked = true;
						break;
					}
				}

				if (!bBlocked && IsRecent(Reaction.LastBlockedTime))
				{
					Status = FString::Printf(TEXT("blocked: %s (%.1fs ago)"),
						*Reaction.LastBlockedBy, Now - Reaction.LastBlockedTime);
					bBlocked = true;
				}
			}

			if (!bBlocked)
			{
				bAllBlocked = false;
			}

			Text += FString::Printf(TEXT("\n%s   %s"),
				Reaction.EventTag.IsValid() ? *Reaction.EventTag.ToString() : TEXT("<no tag>"), *Status);

			if (bShowRules)
			{
				Text += DescribeRules(Reaction.Rules);
			}

			// Recently skipped actions (a requirement failed), newest first, up to 3
			if (Linger > 0.f)
			{
				TArray<const UReactiveAction*> AllActions;
				for (const UReactiveAction* Action : Reaction.Actions)
				{
					GatherActions(Action, AllActions);
				}

				TArray<const UReactiveAction*> Skipped;
				for (const UReactiveAction* Action : AllActions)
				{
					if (IsRecent(Action->GetLastSkippedTime()))
					{
						Skipped.Add(Action);
					}
				}
				Skipped.Sort([](const UReactiveAction& A, const UReactiveAction& B) { return A.GetLastSkippedTime() > B.GetLastSkippedTime(); });

				for (int32 Index = 0; Index < FMath::Min(Skipped.Num(), 3); ++Index)
				{
					const UReactiveAction* Action = Skipped[Index];
					Text += FString::Printf(TEXT("\n    skipped: %s (%s) %.1fs ago"),
						*Action->GetClass()->GetDisplayNameText().ToString(), *Action->GetLastSkippedReason(),
						Now - Action->GetLastSkippedTime());
				}
			}
		}

		// Triggers: one line each, "trigger Overlap -> Event.Kitchen.Enter   fired (1.2s ago)"
		for (const UReactiveTrigger* Trigger : Component->GetTriggers())
		{
			if (!Trigger || !Trigger->IsEnabled())
			{
				continue;
			}

			FString Status = Trigger->IsActive() ? TEXT("watching") : TEXT("inactive");
			if (IsRecent(Trigger->GetLastFiredTime()))
			{
				bAnyFired = true;
				Status = FString::Printf(TEXT("fired (%.1fs ago)"), Now - Trigger->GetLastFiredTime());
			}
			else if (IsRecent(Trigger->GetLastBlockedTime()))
			{
				Status = FString::Printf(TEXT("blocked: %s (%.1fs ago)"), *Trigger->GetLastBlockedBy(), Now - Trigger->GetLastBlockedTime());
			}

			Text += FString::Printf(TEXT("\ntrigger %s -> %s   %s"),
				*Trigger->GetClass()->GetDisplayNameText().ToString(),
				Trigger->GetEvent().IsValid() ? *Trigger->GetEvent().ToString() : TEXT("<no event>"),
				*Status);

			if (bShowRules)
			{
				Text += DescribeRules(Trigger->GetRules());
			}
		}

		// Red: stuck. Yellow: running. Green: fired recently. Grey: every reaction blocked. Cyan: waiting for events.
		const FColor Color = bAnyStuck ? FColor::Red
			: bAnyRunning ? FColor::Yellow
			: bAnyFired ? FColor::Green
			: (Shown > 0 && bAllBlocked) ? FColor(150, 150, 150)
			: FColor::Cyan;

		// Above the actor's visible bounds, straight up in world space (ignores the actor's rotation)
		const FBox Bounds = UReactionComponent::GetDebugBounds(Owner);
		const FVector Center = Bounds.GetCenter();
		const FVector Location(Center.X, Center.Y, Bounds.Max.Z + CVarReactionsShowOffset.GetValueOnGameThread());

		DrawDebugString(World, Location, Text, /*TestBaseActor*/ nullptr, Color, /*Duration: one frame*/ 0.f, /*bDrawShadow*/ true);
	}
}

#endif // !UE_BUILD_SHIPPING

// =================================================================================================
// Overlay and on-screen history, drawn every frame while switched on
// =================================================================================================

#if !UE_BUILD_SHIPPING
void UReactionSubsystem::TickDebug()
{
	// Reactions.Show.History N: the last N history lines on screen (the engine puts the newest on top)
	const int32 HistoryLines = CVarReactionsShowHistory.GetValueOnGameThread();
	if (HistoryLines > 0 && GEngine)
	{
		const UWorld* HistoryWorld = GetWorld();
		const double Now = HistoryWorld ? HistoryWorld->GetTimeSeconds() : 0.0;
		const int32 First = FMath::Max(0, History.Num() - HistoryLines);
		const uint64 BaseKey = static_cast<uint64>(GetUniqueID()) * 1000 + 100;

		for (int32 Index = First; Index < History.Num(); ++Index)
		{
			const FHistoryLine& Line = History[Index];
			GEngine->AddOnScreenDebugMessage(BaseKey + (Index - First), /*Time*/ 0.f, Line.Color,
				FString::Printf(TEXT("[%.1fs ago] %s"), Now - Line.Time, *Line.Text));
		}
	}

	if (CVarReactionsShow.GetValueOnGameThread() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bSuspended && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(/*Key*/ static_cast<uint64>(GetUniqueID()) + 1, /*Time*/ 0.f, FColor::Orange,
			TEXT("Reactive Actors SUSPENDED: events are being ignored."));
	}

	bool bBadTag = false;
	const FGameplayTag Filter = ReactionsDebug::GetFilterTag(bBadTag);
	if (bBadTag && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(/*Key*/ static_cast<uint64>(GetUniqueID()), /*Time*/ 0.f, FColor::Orange,
			FString::Printf(TEXT("Reactions.Show.Filter: '%s' is not a gameplay tag (showing everything)."),
				*CVarReactionsShowFilter.GetValueOnGameThread()));
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	const APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	const float MaxDistanceSq = FMath::Square(CVarReactionsShowDistance.GetValueOnGameThread());

	// Debug only (and only while switched on), so walking every component each frame is fine
	for (TObjectIterator<UReactionComponent> It; It; ++It)
	{
		const UReactionComponent* Component = *It;
		if (!IsValid(Component) || Component->GetWorld() != World || !Component->HasBegunPlay() || !Component->GetOwner())
		{
			continue;
		}
		if (PC && FVector::DistSquared(ViewLocation, Component->GetOwner()->GetActorLocation()) > MaxDistanceSq)
		{
			continue;
		}
		if (!ReactionsDebug::PassesFilter(Component, Filter))
		{
			continue;
		}
		ReactionsDebug::DrawComponentStatus(World, Component);
	}
}
#endif

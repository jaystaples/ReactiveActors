// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - automated tests
//
// Each test builds a small world, places actors with reactions made of probe actions, sends events and
// checks what ran. Run them from Tools > Session Frontend > Automation (filter "ReactiveActors"), or headless:
//   UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests ReactiveActors;Quit" -unattended -nullrhi
//
// Groups: Events (sending and matching), Chain (order, failure, cancel), Rules (Retrigger, Max Activations,
// Cooldown, Reset), Flow (Delay, Branch, Random, Run On Targets, Wait For Event), Control (enable,
// suspend, Run Reaction Preset), Presets, Validation.

#include "ReactiveTestHelpers.h"

#include "Actions/FlowActions.h"
#include "BuiltInRequirements.h"
#include "BuiltInRules.h"
#include "BuiltInTriggers.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "ReactionComponent.h"
#include "ReactionPreset.h"
#include "ReactionSubsystem.h"
#include "ReactiveStateCache.h"
#include "ReactiveTestProbes.h"
#include "Actions/PresentationActions.h"
#include "Actions/WorldActions.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

#define REACTIVE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

using namespace ReactiveTests;

namespace ReactiveTests
{
	/** An actor with one Reaction Component holding one reaction, started. Returns the component. */
	static UReactionComponent* ActorWithReaction(const FReactiveTestWorld& TestWorld, AActor*& OutActor,
		TFunctionRef<FReaction(UReactionComponent*)> Build, const FGameplayTagContainer& Identity = FGameplayTagContainer())
	{
		OutActor = TestWorld.SpawnActor();
		UReactionComponent* Component = TestWorld.AddComponent(OutActor, Identity);
		TestWorld.AddReaction(Component, Build(Component));
		TestWorld.Start(Component);
		return Component;
	}

	static FGameplayTagContainer Tags(const FGameplayTag& Tag)
	{
		return FGameplayTagContainer(Tag);
	}
}

// =================================================================================================
// Events
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_BroadcastFires, "ReactiveActors.Events.BroadcastFiresReaction", REACTIVE_TEST_FLAGS)
bool FReactiveTest_BroadcastFires::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			return MakeReaction(Ping(), { Probe });
		});

	TestTrue(TEXT("Broadcast reports a reaction fired"), TestWorld.Subsystem()->BroadcastEvent(Ping()));
	TestEqual(TEXT("Probe ran once"), Probe->ExecuteCount, 1);
	TestFalse(TEXT("Another event doesn't fire it"), TestWorld.Subsystem()->BroadcastEvent(Pong()));
	TestEqual(TEXT("Still once"), Probe->ExecuteCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_ParentTagMatches, "ReactiveActors.Events.ParentTagHearsChild", REACTIVE_TEST_FLAGS)
bool FReactiveTest_ParentTagMatches::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* ParentListener = nullptr;
	AActor* ChildListener = nullptr;
	UReactiveTestProbe* ParentProbe = nullptr;
	UReactiveTestProbe* ChildProbe = nullptr;
	ActorWithReaction(TestWorld, ParentListener, [&](UReactionComponent* C)
		{
			ParentProbe = MakeProbe(C, TEXT("Parent"));
			return MakeReaction(Ping(), { ParentProbe });
		});
	ActorWithReaction(TestWorld, ChildListener, [&](UReactionComponent* C)
		{
			ChildProbe = MakeProbe(C, TEXT("Child"));
			return MakeReaction(PingChild(), { ChildProbe });
		});

	TestWorld.Subsystem()->BroadcastEvent(PingChild());
	TestEqual(TEXT("Listening for Ping hears Ping.Child"), ParentProbe->ExecuteCount, 1);
	TestEqual(TEXT("Listening for Ping.Child hears Ping.Child"), ChildProbe->ExecuteCount, 1);

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Listening for Ping hears Ping"), ParentProbe->ExecuteCount, 2);
	TestEqual(TEXT("Listening for Ping.Child doesn't hear its parent"), ChildProbe->ExecuteCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_IdentityBroadcast, "ReactiveActors.Events.BroadcastToIdentity", REACTIVE_TEST_FLAGS)
bool FReactiveTest_IdentityBroadcast::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* ActorA = nullptr;
	AActor* ActorB = nullptr;
	UReactiveTestProbe* ProbeA = nullptr;
	UReactiveTestProbe* ProbeB = nullptr;
	ActorWithReaction(TestWorld, ActorA, [&](UReactionComponent* C)
		{
			ProbeA = MakeProbe(C, TEXT("A"));
			return MakeReaction(Ping(), { ProbeA });
		}, Tags(IdentityA()));
	ActorWithReaction(TestWorld, ActorB, [&](UReactionComponent* C)
		{
			ProbeB = MakeProbe(C, TEXT("B"));
			return MakeReaction(Ping(), { ProbeB });
		}, Tags(IdentityB()));

	TestWorld.Subsystem()->BroadcastEventToIdentity(IdentityA(), Ping());
	TestEqual(TEXT("Identity A heard it"), ProbeA->ExecuteCount, 1);
	TestEqual(TEXT("Identity B didn't"), ProbeB->ExecuteCount, 0);

	TestWorld.Subsystem()->BroadcastEventToActor(ActorB, Ping());
	TestEqual(TEXT("Sent straight to B"), ProbeB->ExecuteCount, 1);
	TestEqual(TEXT("A unaffected"), ProbeA->ExecuteCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_InstigatorPassed, "ReactiveActors.Events.InstigatorReachesActions", REACTIVE_TEST_FLAGS)
bool FReactiveTest_InstigatorPassed::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			return MakeReaction(Ping(), { Probe });
		});
	AActor* Player = TestWorld.SpawnActor();

	TestWorld.Subsystem()->BroadcastEvent(PingChild(), Player);
	TestTrue(TEXT("Target is the reacting actor"), Probe->LastContext.Target == Actor);
	TestTrue(TEXT("Instigator is who sent it"), Probe->LastContext.Instigator == Player);
	TestTrue(TEXT("Event is the one sent (the child)"), Probe->LastContext.Event == PingChild());
	return true;
}

// =================================================================================================
// Chain
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_ChainOrder, "ReactiveActors.Chain.WaitsForFinishAction", REACTIVE_TEST_FLAGS)
bool FReactiveTest_ChainOrder::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* First = nullptr;
	UReactiveTestProbe* Second = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			First = MakeProbe(C, TEXT("First"), /*bFinishNow*/ false);
			Second = MakeProbe(C, TEXT("Second"));
			return MakeReaction(Ping(), { First, Second });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("First ran"), First->ExecuteCount, 1);
	TestEqual(TEXT("Second waits for First's Finish Action"), Second->ExecuteCount, 0);

	First->FinishAction(true);
	TestEqual(TEXT("Second runs once First finishes"), Second->ExecuteCount, 1);
	const TArray<FName> Expected = { TEXT("First"), TEXT("Second") };
	TestTrue(TEXT("Ran in order: First, then Second"), UReactiveTestProbe::RunLog() == Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_FailureStops, "ReactiveActors.Chain.FailureStopsTheChain", REACTIVE_TEST_FLAGS)
bool FReactiveTest_FailureStops::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Second = nullptr;
	UReactiveTestProbeRule* Rule = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Rule = NewObject<UReactiveTestProbeRule>(C);
			Second = MakeProbe(C, TEXT("Second"));
			return MakeReaction(Ping(), { MakeProbe(C, TEXT("Fails"), true, /*bSucceed*/ false), Second }, { Rule });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Nothing after a failed action runs"), Second->ExecuteCount, 0);
	TestEqual(TEXT("Rules hear the run finish"), Rule->FinishCount, 1);
	TestTrue(TEXT("Result is Failed"), Rule->LastResult == EReactionResult::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SkippedContinues, "ReactiveActors.Chain.SkippedActionLetsChainContinue", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SkippedContinues::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Skipped = nullptr;
	UReactiveTestProbe* After = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Skipped = MakeProbe(C, TEXT("Skipped"));
			UREQ_HasActorTags* NeedsTag = NewObject<UREQ_HasActorTags>(Skipped);
			Prop<TArray<FName>>(NeedsTag, TEXT("Tags")).Add(TEXT("NotOnThisActor"));
			Prop<TArray<TObjectPtr<UReactiveRequirement>>>(Skipped, TEXT("ExtraRequirements")).Add(NeedsTag);
			After = MakeProbe(C, TEXT("After"));
			return MakeReaction(Ping(), { Skipped, After });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Action whose requirement fails is skipped"), Skipped->ExecuteCount, 0);
	TestEqual(TEXT("The chain carries on after a skip"), After->ExecuteCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_StopIfNotMet, "ReactiveActors.Chain.StopReactionIfNotMet", REACTIVE_TEST_FLAGS)
bool FReactiveTest_StopIfNotMet::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Before = nullptr;
	UReactiveTestProbe* Guarded = nullptr;
	UReactiveTestProbe* After = nullptr;
	UReactiveTestProbeRule* Rule = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Rule = NewObject<UReactiveTestProbeRule>(C);
			Before = MakeProbe(C, TEXT("Before"));
			Guarded = MakeProbe(C, TEXT("Guarded"));
			UREQ_HasActorTags* NeedsTag = NewObject<UREQ_HasActorTags>(Guarded);
			Prop<TArray<FName>>(NeedsTag, TEXT("Tags")).Add(TEXT("NotOnThisActor"));
			Prop<TArray<TObjectPtr<UReactiveRequirement>>>(Guarded, TEXT("ExtraRequirements")).Add(NeedsTag);
			Prop<bool>(Guarded, TEXT("bStopReactionIfNotMet")) = true;
			After = MakeProbe(C, TEXT("After"));
			return MakeReaction(Ping(), { Before, Guarded, After }, { Rule });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Actions before it still run"), Before->ExecuteCount, 1);
	TestEqual(TEXT("The guarded action doesn't run"), Guarded->ExecuteCount, 0);
	TestEqual(TEXT("Nothing after it runs"), After->ExecuteCount, 0);
	TestEqual(TEXT("Rules hear the run finish"), Rule->FinishCount, 1);
	TestTrue(TEXT("Result is Failed"), Rule->LastResult == EReactionResult::Failed);

	// Requirement met: it runs and the chain carries on as normal
	Actor->Tags.Add(TEXT("NotOnThisActor"));
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Met: the guarded action runs"), Guarded->ExecuteCount, 1);
	TestEqual(TEXT("Met: the chain carries on"), After->ExecuteCount, 1);
	TestTrue(TEXT("Met: result is Succeeded"), Rule->LastResult == EReactionResult::Succeeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_CancelMidRun, "ReactiveActors.Chain.CancelStopsRunAndTellsRules", REACTIVE_TEST_FLAGS)
bool FReactiveTest_CancelMidRun::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Waiting = nullptr;
	UReactiveTestProbe* After = nullptr;
	UReactiveTestProbeRule* Rule = nullptr;
	UReactionComponent* Component = ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Waiting = MakeProbe(C, TEXT("Waiting"), /*bFinishNow*/ false);
			After = MakeProbe(C, TEXT("After"));
			Rule = NewObject<UReactiveTestProbeRule>(C);
			return MakeReaction(Ping(), { Waiting, After }, { Rule });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("One reaction cancelled"), Component->CancelReactions(FGameplayTag()), 1);
	TestEqual(TEXT("The waiting action got On Cancel"), Waiting->CancelCount, 1);
	TestFalse(TEXT("It's no longer running"), Waiting->IsRunning());

	Waiting->FinishAction(true); // a late finish after a cancel is ignored
	TestEqual(TEXT("Nothing after it runs"), After->ExecuteCount, 0);
	TestTrue(TEXT("Rules hear Cancelled"), Rule->LastResult == EReactionResult::Cancelled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_DestroyMidRun, "ReactiveActors.Chain.DestroyingActorStopsRun", REACTIVE_TEST_FLAGS)
bool FReactiveTest_DestroyMidRun::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Waiting = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Waiting = MakeProbe(C, TEXT("Waiting"), /*bFinishNow*/ false);
			return MakeReaction(Ping(), { Waiting });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestTrue(TEXT("Running before"), Waiting->IsRunning());
	// The actor leaving play (destroyed, streamed out) runs End Play on its components. Routed directly rather than
	// with Destroy, which in the editor also records an undo step against a temporary package.
	Actor->RouteEndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("Not running once its actor leaves play"), Waiting->IsRunning());
	TestFalse(TEXT("Broadcasting afterwards is safe and reaches nobody"), TestWorld.Subsystem()->BroadcastEvent(Ping()));
	return true;
}

// =================================================================================================
// Rules
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_RetriggerIgnore, "ReactiveActors.Rules.RetriggerIgnore", REACTIVE_TEST_FLAGS)
bool FReactiveTest_RetriggerIgnore::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"), /*bFinishNow*/ false);
			return MakeReaction(Ping(), { Probe }, {}, EReactiveRetriggerPolicy::Ignore);
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("The second event is ignored while running"), Probe->ExecuteCount, 1);
	TestEqual(TEXT("Nothing was cancelled"), Probe->CancelCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_RetriggerRestart, "ReactiveActors.Rules.RetriggerRestart", REACTIVE_TEST_FLAGS)
bool FReactiveTest_RetriggerRestart::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"), /*bFinishNow*/ false);
			return MakeReaction(Ping(), { Probe }, {}, EReactiveRetriggerPolicy::Restart);
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("The first run was cancelled"), Probe->CancelCount, 1);
	TestEqual(TEXT("And started again"), Probe->ExecuteCount, 2);
	TestTrue(TEXT("Running again"), Probe->IsRunning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_MaxActivationsAndReset, "ReactiveActors.Rules.MaxActivationsAndReset", REACTIVE_TEST_FLAGS)
bool FReactiveTest_MaxActivationsAndReset::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	UReactionComponent* Component = ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			URULE_MaxActivations* Once = NewObject<URULE_MaxActivations>(C);
			Prop<int32>(Once, TEXT("MaxActivations")) = 1;
			return MakeReaction(Ping(), { Probe }, { Once });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Max Activations 1: fires once"), Probe->ExecuteCount, 1);

	Component->ResetReactions(FGameplayTag());
	TestEqual(TEXT("Actions get On Reset"), Probe->ResetCount, 1);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("After a reset it fires again"), Probe->ExecuteCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_Cooldown, "ReactiveActors.Rules.Cooldown", REACTIVE_TEST_FLAGS)
bool FReactiveTest_Cooldown::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			URULE_Cooldown* Cooldown = NewObject<URULE_Cooldown>(C);
			Prop<float>(Cooldown, TEXT("CooldownSeconds")) = 1.f;
			return MakeReaction(Ping(), { Probe }, { Cooldown });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestWorld.Tick(0.5f);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Blocked during the cooldown"), Probe->ExecuteCount, 1);

	TestWorld.Tick(0.7f);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Fires again once it's over"), Probe->ExecuteCount, 2);
	return true;
}

// =================================================================================================
// Save and load
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveMaxActivations, "ReactiveActors.Save.MaxActivationsRoundTrip", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveMaxActivations::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	auto Build = [](UReactionComponent* C, UReactiveTestProbe*& OutProbe)
		{
			OutProbe = MakeProbe(C, TEXT("P"));
			URULE_MaxActivations* Once = NewObject<URULE_MaxActivations>(C);
			Prop<int32>(Once, TEXT("MaxActivations")) = 1;
			return MakeReaction(Ping(), { OutProbe }, { Once });
		};

	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	UReactionComponent* Component = ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C) { return Build(C, Probe); });

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	const FReactiveActorState Saved = Component->CaptureReactionState();
	TestEqual(TEXT("Captures the reaction"), Saved.Reactions.Num(), 1);

	Component->ResetReactions(FGameplayTag());
	Component->RestoreReactionState(Saved);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Restored count still blocks it"), Probe->ExecuteCount, 1);

	// Restored before Begin Play (a save system loading the level): applied as it begins play
	AActor* Other = TestWorld.SpawnActor();
	UReactionComponent* Fresh = TestWorld.AddComponent(Other);
	UReactiveTestProbe* FreshProbe = nullptr;
	TestWorld.AddReaction(Fresh, Build(Fresh, FreshProbe));
	Fresh->RestoreReactionState(Saved);
	TestWorld.Start(Fresh);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Restored before Begin Play: already used up"), FreshProbe->ExecuteCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveCooldown, "ReactiveActors.Save.CooldownKeepsTimeLeft", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveCooldown::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	UReactionComponent* Component = ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			URULE_Cooldown* Cooldown = NewObject<URULE_Cooldown>(C);
			Prop<float>(Cooldown, TEXT("CooldownSeconds")) = 2.f;
			return MakeReaction(Ping(), { Probe }, { Cooldown });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestWorld.Tick(0.5f);
	const FReactiveActorState Saved = Component->CaptureReactionState(); // 1.5s left

	Component->ResetReactions(FGameplayTag());
	TestWorld.Tick(1.f); // time passes before the load: world time isn't what was saved
	Component->RestoreReactionState(Saved);

	TestWorld.Tick(1.f);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("1s after restoring 1.5s left: still cooling down"), Probe->ExecuteCount, 1);

	TestWorld.Tick(0.7f);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Once the time left has passed: fires"), Probe->ExecuteCount, 2);
	return true;
}

namespace ReactiveTests
{
	/** An actor (not started) with a Begin Play trigger (Max Activations 1) sending Pong to itself, and a probe reacting to Pong. */
	static UReactionComponent* BeginPlayOnce(const FReactiveTestWorld& TestWorld, UReactiveTestProbe*& OutProbe)
	{
		AActor* Actor = TestWorld.SpawnActor();
		UReactionComponent* Component = TestWorld.AddComponent(Actor);
		OutProbe = MakeProbe(Component, TEXT("OnBeginPlay"));
		TestWorld.AddReaction(Component, MakeReaction(Pong(), { OutProbe }));

		UTRG_BeginPlay* Trigger = NewObject<UTRG_BeginPlay>(Component);
		Prop<FGameplayTag>(Trigger, TEXT("Event")) = Pong();
		Prop<EReactiveScopePolicy>(Trigger, TEXT("Scope")) = EReactiveScopePolicy::Self;
		URULE_MaxActivations* Once = NewObject<URULE_MaxActivations>(Trigger);
		Prop<int32>(Once, TEXT("MaxActivations")) = 1;
		Prop<TArray<TObjectPtr<UReactiveRule>>>(Trigger, TEXT("Rules")).Add(Once);
		Prop<TArray<TObjectPtr<UReactiveTrigger>>>(Component, TEXT("Triggers")).Add(Trigger);
		return Component;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveBeginPlayLate, "ReactiveActors.Save.BeginPlayWaitsForSameFrameRestore", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveBeginPlayLate::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;

	// A first playthrough: the Begin Play trigger fires once
	UReactiveTestProbe* First = nullptr;
	UReactionComponent* Played = BeginPlayOnce(TestWorld, First);
	TestWorld.Start(Played);
	TestWorld.Tick(0.1f);
	TestEqual(TEXT("Begin Play trigger fired"), First->ExecuteCount, 1);
	const FReactiveActorState Saved = Played->CaptureReactionState();

	// Loading: the actor begins play, then the restore arrives in the same frame (after Begin Play)
	UReactiveTestProbe* Loaded = nullptr;
	UReactionComponent* Fresh = BeginPlayOnce(TestWorld, Loaded);
	TestWorld.Start(Fresh);
	Fresh->RestoreReactionState(Saved);
	TestWorld.Tick(0.1f);
	TestEqual(TEXT("Restored after Begin Play, same frame: the trigger sees its saved count and doesn't fire"), Loaded->ExecuteCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveHold, "ReactiveActors.Save.HoldUntilRestored", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveHold::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;

	UReactiveTestProbe* First = nullptr;
	UReactionComponent* Played = BeginPlayOnce(TestWorld, First);
	TestWorld.Start(Played);
	TestWorld.Tick(0.1f);
	const FReactiveActorState Saved = Played->CaptureReactionState();

	// A save system that restores a few frames late: hold first
	TestWorld.Subsystem()->HoldUntilRestored();
	UReactiveTestProbe* Held = nullptr;
	UReactionComponent* Late = BeginPlayOnce(TestWorld, Held);
	UReactiveTestProbe* Other = nullptr;
	UReactionComponent* NotInSave = BeginPlayOnce(TestWorld, Other);
	TestWorld.Start(Late);
	TestWorld.Start(NotInSave);
	TestWorld.Tick(0.5f);
	TestEqual(TEXT("Held: nothing fires while waiting for the restore"), Held->ExecuteCount + Other->ExecuteCount, 0);

	FReactiveWorldState SaveData;
	SaveData.WorldKey = ReactiveSave::GetWorldKey(TestWorld.GetWorld());
	FReactiveActorState& Entry = SaveData.Actors.Add_GetRef(Saved);
	Entry.Key = Late->GetSaveKey();
	UReactiveStateCache::Get(TestWorld.GetWorld())->SetWorldState(TestWorld.GetWorld(), SaveData);
	TestWorld.Tick(0.1f);
	TestEqual(TEXT("Released: the restored one stays used up"), Held->ExecuteCount, 0);
	TestEqual(TEXT("Released: one the save doesn't mention fires now"), Other->ExecuteCount, 1);
	TestFalse(TEXT("No longer holding"), TestWorld.Subsystem()->IsHolding());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveWorld, "ReactiveActors.Save.WorldStateRoundTrip", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveWorld::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	UReactiveStateCache* Cache = UReactiveStateCache::Get(World);
	if (!TestNotNull(TEXT("There's a Reactive State Cache"), Cache))
	{
		return false;
	}

	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	UReactionComponent* Component = ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			return MakeReaction(Ping(), { Probe });
		});
	AActor* Untouched = nullptr;
	UReactiveTestProbe* UntouchedProbe = nullptr;
	UReactionComponent* Quiet = ActorWithReaction(TestWorld, Untouched, [&](UReactionComponent* C)
		{
			UntouchedProbe = MakeProbe(C, TEXT("Quiet"));
			return MakeReaction(Pong(), { UntouchedProbe });
		});

	Component->SetReactionsEnabled(FGameplayTag(), false);
	const FReactiveWorldState Saved = Cache->GetWorldState(World);
	TestEqual(TEXT("Saved under this level"), Saved.WorldKey, ReactiveSave::GetWorldKey(World));
	TestTrue(TEXT("The changed component is in it"), Saved.Actors.ContainsByPredicate(
		[Component](const FReactiveActorState& Entry) { return Entry.Key == Component->GetSaveKey(); }));
	TestFalse(TEXT("One where nothing happened isn't"), Saved.Actors.ContainsByPredicate(
		[Quiet](const FReactiveActorState& Entry) { return Entry.Key == Quiet->GetSaveKey(); }));

	// Loading into the level that's playing: applied straight away
	Component->SetReactionsEnabled(FGameplayTag(), true);
	Cache->SetWorldState(World, Saved);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Switched off again by the restore, and not listening"), Probe->ExecuteCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveActions, "ReactiveActors.Save.ActionVariablesRoundTrip", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveActions::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	UReactionComponent* Component = ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			return MakeReaction(Ping(), { Probe });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	const FReactiveActorState Saved = Component->CaptureReactionState();
	TestEqual(TEXT("The action's state is captured"), Saved.Reactions.Num() == 1 ? Saved.Reactions[0].Actions.Num() : 0, 1);

	Prop<int32>(Probe, TEXT("SavedRuns")) = 0;
	Component->RestoreReactionState(Saved);
	TestEqual(TEXT("An action's SaveGame variable comes back"), Prop<int32>(Probe, TEXT("SavedRuns")), 2);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveValidation, "ReactiveActors.Validation.SaveGameVariablesThatDontSurvive", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveValidation::RunTest(const FString& Parameters)
{
	TArray<FText> Warnings;
	ReactiveSave::GatherSaveGameProblems(UReactiveTestBadSaveRule::StaticClass(), [&Warnings](const FText& Message) { Warnings.Add(Message); });
	TestEqual(TEXT("Warns about the actor reference and the timer, not the count"), Warnings.Num(), 2);
	TestTrue(TEXT("Suggests Identity Tags for the reference"), Warnings.ContainsByPredicate(
		[](const FText& Message) { return Message.ToString().Contains(TEXT("Identity Tag")); }));

	Warnings.Reset();
	ReactiveSave::GatherSaveGameProblems(UReactiveTestProbe::StaticClass(), [&Warnings](const FText& Message) { Warnings.Add(Message); });
	TestEqual(TEXT("A plain SaveGame count is fine"), Warnings.Num(), 0);
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveSpawnedSkipped, "ReactiveActors.Save.SpawnedActorsNotCached", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveSpawnedSkipped::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	UReactiveStateCache* Cache = UReactiveStateCache::Get(World);
	if (!TestNotNull(TEXT("There's a Reactive State Cache"), Cache))
	{
		return false;
	}

	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	UReactionComponent* Component = ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			return MakeReaction(Ping(), { Probe });
		});
	Actor->bNetStartup = false; // spawned while playing, not placed in the level

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestTrue(TEXT("Something happened on it"), Component->HasStateChanged());
	TestFalse(TEXT("But a spawned actor isn't in the level's state"), Cache->GetWorldState(World).Actors.ContainsByPredicate(
		[Component](const FReactiveActorState& Entry) { return Entry.Key == Component->GetSaveKey(); }));
	TestEqual(TEXT("Capture Reaction State still works for it"), Component->CaptureReactionState().Reactions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SaveCacheBeginPlay, "ReactiveActors.Save.CachedStateAppliedAtBeginPlay", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SaveCacheBeginPlay::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	UReactiveStateCache* Cache = UReactiveStateCache::Get(World);
	if (!TestNotNull(TEXT("There's a Reactive State Cache"), Cache))
	{
		return false;
	}

	// A Begin Play trigger that has already fired once, in the save
	UReactiveTestProbe* First = nullptr;
	UReactionComponent* Played = BeginPlayOnce(TestWorld, First);
	TestWorld.Start(Played);
	TestWorld.Tick(0.1f);
	const FReactiveActorState Saved = Played->CaptureReactionState();

	// An actor that isn't loaded yet (it streams in later): its state waits in the cache
	UReactiveTestProbe* Streamed = nullptr;
	UReactionComponent* Later = BeginPlayOnce(TestWorld, Streamed);
	FReactiveWorldState SaveData;
	SaveData.WorldKey = ReactiveSave::GetWorldKey(World);
	FReactiveActorState& Entry = SaveData.Actors.Add_GetRef(Saved);
	Entry.Key = Later->GetSaveKey();
	Cache->SetWorldState(World, SaveData);

	TestWorld.Start(Later);
	TestWorld.Tick(0.1f);
	TestEqual(TEXT("Streamed in with its cached state: the Begin Play trigger stays used up"), Streamed->ExecuteCount, 0);
	return true;
}

// =================================================================================================
// Flow actions
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_Delay, "ReactiveActors.Flow.Delay", REACTIVE_TEST_FLAGS)
bool FReactiveTest_Delay::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* After = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			UACT_Delay* Delay = NewObject<UACT_Delay>(C);
			Prop<float>(Delay, TEXT("Duration")) = 0.5f;
			After = MakeProbe(C, TEXT("After"));
			return MakeReaction(Ping(), { Delay, After });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Waits for the Delay"), After->ExecuteCount, 0);
	TestWorld.Tick(0.3f);
	TestEqual(TEXT("Still waiting part way"), After->ExecuteCount, 0);
	TestWorld.Tick(0.4f);
	TestEqual(TEXT("Runs once the Delay is over"), After->ExecuteCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_Branch, "ReactiveActors.Flow.BranchAndNot", REACTIVE_TEST_FLAGS)
bool FReactiveTest_Branch::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* OnTrue = nullptr;
	UReactiveTestProbe* OnFalse = nullptr;
	UREQ_HasActorTags* IsLit = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			UACT_Branch* Branch = NewObject<UACT_Branch>(C);
			IsLit = NewObject<UREQ_HasActorTags>(Branch);
			Prop<TArray<FName>>(IsLit, TEXT("Tags")).Add(TEXT("Lit"));
			Prop<TArray<TObjectPtr<UReactiveRequirement>>>(Branch, TEXT("Conditions")).Add(IsLit);
			OnTrue = MakeProbe(Branch, TEXT("True"));
			OnFalse = MakeProbe(Branch, TEXT("False"));
			Prop<TArray<TObjectPtr<UReactiveAction>>>(Branch, TEXT("TrueActions")).Add(OnTrue);
			Prop<TArray<TObjectPtr<UReactiveAction>>>(Branch, TEXT("FalseActions")).Add(OnFalse);
			return MakeReaction(Ping(), { Branch });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("No tag: False actions"), OnFalse->ExecuteCount, 1);
	TestEqual(TEXT("No tag: not True"), OnTrue->ExecuteCount, 0);

	Actor->Tags.Add(TEXT("Lit"));
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Tag: True actions"), OnTrue->ExecuteCount, 1);

	IsLit->bInvert = true; // "Not Has Actor Tags: Lit"
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Not flips it: False actions"), OnFalse->ExecuteCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_RandomWeights, "ReactiveActors.Flow.RandomActionRespectsWeights", REACTIVE_TEST_FLAGS)
bool FReactiveTest_RandomWeights::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			UACT_RandomAction* Random = NewObject<UACT_RandomAction>(C);
			TArray<FReactiveRandomOption>& Options = Prop<TArray<FReactiveRandomOption>>(Random, TEXT("Options"));
			FReactiveRandomOption& Never = Options.AddDefaulted_GetRef();
			Never.Weight = 0.f;
			Never.Actions.Add(MakeProbe(Random, TEXT("Never")));
			FReactiveRandomOption& Always = Options.AddDefaulted_GetRef();
			Always.Weight = 1.f;
			Always.Actions.Add(MakeProbe(Random, TEXT("Always")));
			return MakeReaction(Ping(), { Random });
		});

	for (int32 Index = 0; Index < 10; ++Index)
	{
		TestWorld.Subsystem()->BroadcastEvent(Ping());
	}
	TestEqual(TEXT("Weight 0 is never picked"), UReactiveTestProbe::CountRuns(TEXT("Never")), 0);
	TestEqual(TEXT("The other option runs every time"), UReactiveTestProbe::CountRuns(TEXT("Always")), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_RunOnTargets, "ReactiveActors.Flow.RunOnTargets", REACTIVE_TEST_FLAGS)
bool FReactiveTest_RunOnTargets::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;

	// Two targets with identity A, one with B
	for (const FGameplayTag& Identity : { IdentityA(), IdentityA(), IdentityB() })
	{
		AActor* Target = TestWorld.SpawnActor();
		TestWorld.Start(TestWorld.AddComponent(Target, Tags(Identity)));
	}

	AActor* Source = nullptr;
	ActorWithReaction(TestWorld, Source, [&](UReactionComponent* C)
		{
			UACT_RunOnTargets* RunOn = NewObject<UACT_RunOnTargets>(C);
			Prop<FGameplayTag>(RunOn, TEXT("TargetIdentity")) = IdentityA();
			Prop<TArray<TObjectPtr<UReactiveAction>>>(RunOn, TEXT("Actions")).Add(MakeProbe(RunOn, TEXT("OnTarget")));
			return MakeReaction(Ping(), { RunOn });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Runs once per actor with identity A"), UReactiveTestProbe::CountRuns(TEXT("OnTarget")), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_WaitForEvent, "ReactiveActors.Flow.WaitForEvent", REACTIVE_TEST_FLAGS)
bool FReactiveTest_WaitForEvent::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* After = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			UACT_WaitForEvent* Wait = NewObject<UACT_WaitForEvent>(C);
			Prop<FGameplayTag>(Wait, TEXT("Event")) = Pong();
			After = MakeProbe(C, TEXT("After"));
			return MakeReaction(Ping(), { Wait, After });
		});

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Waits for Pong"), After->ExecuteCount, 0);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Another Ping doesn't release it"), After->ExecuteCount, 0);
	TestWorld.Subsystem()->BroadcastEvent(Pong());
	TestEqual(TEXT("Pong releases it"), After->ExecuteCount, 1);
	return true;
}

// =================================================================================================
// Control
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_SetEnabled, "ReactiveActors.Control.SetReactionsEnabled", REACTIVE_TEST_FLAGS)
bool FReactiveTest_SetEnabled::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	UReactionComponent* Component = ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			return MakeReaction(Ping(), { Probe });
		});

	TestEqual(TEXT("One reaction switched off"), Component->SetReactionsEnabled(Ping(), false), 1);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Disabled: doesn't fire"), Probe->ExecuteCount, 0);

	Component->SetReactionsEnabled(Ping(), true);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Enabled again: fires"), Probe->ExecuteCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_Suspend, "ReactiveActors.Control.SuspendEvents", REACTIVE_TEST_FLAGS)
bool FReactiveTest_Suspend::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Actor = nullptr;
	UReactiveTestProbe* Probe = nullptr;
	ActorWithReaction(TestWorld, Actor, [&](UReactionComponent* C)
		{
			Probe = MakeProbe(C, TEXT("P"));
			return MakeReaction(Ping(), { Probe });
		});

	TestWorld.Subsystem()->SetEventsSuspended(true);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestWorld.Subsystem()->BroadcastEventToActor(Actor, Ping());
	TestEqual(TEXT("Suspended: every event ignored"), Probe->ExecuteCount, 0);

	TestWorld.Subsystem()->SetEventsSuspended(false);
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Resumed: fires"), Probe->ExecuteCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_RunReactionPreset, "ReactiveActors.Control.RunReactionPresetKeepsRuleState", REACTIVE_TEST_FLAGS)
bool FReactiveTest_RunReactionPreset::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;
	AActor* Target = TestWorld.SpawnActor();

	UReactionPreset* Preset = NewObject<UReactionPreset>(GetTransientPackage());
	URULE_MaxActivations* Once = NewObject<URULE_MaxActivations>(Preset);
	Prop<int32>(Once, TEXT("MaxActivations")) = 1;
	Preset->Reactions.Add(MakeReaction(Ping(), { MakeProbe(Preset, TEXT("FromPreset")) }, { Once }));
	Preset->Reactions.Add(MakeReaction(Pong(), { MakeProbe(Preset, TEXT("OtherEvent")) }));

	int32 Finished = 0;
	auto Count = [&Finished](EReactionResult) { ++Finished; };

	TestEqual(TEXT("First run starts the Ping reaction only"), TestWorld.Subsystem()->RunReactionPreset(Preset, Ping(), Target, nullptr, Count), 1);
	TestEqual(TEXT("Its action ran"), UReactiveTestProbe::CountRuns(TEXT("FromPreset")), 1);
	TestEqual(TEXT("The Pong reaction didn't"), UReactiveTestProbe::CountRuns(TEXT("OtherEvent")), 0);
	TestEqual(TEXT("Told when it finished"), Finished, 1);

	TestEqual(TEXT("Max Activations remembered between runs"), TestWorld.Subsystem()->RunReactionPreset(Preset, Ping(), Target, nullptr, Count), 0);

	TestWorld.Subsystem()->ResetReactionsForActor(Target);
	TestEqual(TEXT("Reset clears it"), TestWorld.Subsystem()->RunReactionPreset(Preset, Ping(), Target, nullptr, Count), 1);
	return true;
}

// =================================================================================================
// Presets
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_PresetPerActor, "ReactiveActors.Presets.EachActorGetsItsOwnCopy", REACTIVE_TEST_FLAGS)
bool FReactiveTest_PresetPerActor::RunTest(const FString& Parameters)
{
	ResetLog();
	FReactiveTestWorld TestWorld;

	UReactionPreset* Preset = NewObject<UReactionPreset>(GetTransientPackage());
	URULE_MaxActivations* Once = NewObject<URULE_MaxActivations>(Preset);
	Prop<int32>(Once, TEXT("MaxActivations")) = 1;
	Preset->Reactions.Add(MakeReaction(Ping(), { MakeProbe(Preset, TEXT("Shared")) }, { Once }));

	for (int32 Index = 0; Index < 2; ++Index)
	{
		AActor* Actor = TestWorld.SpawnActor();
		UReactionComponent* Component = TestWorld.AddComponent(Actor);
		Prop<TObjectPtr<UReactionPreset>>(Component, TEXT("Preset")) = Preset;
		TestWorld.Start(Component);
	}

	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestWorld.Subsystem()->BroadcastEvent(Ping());
	TestEqual(TEXT("Each actor fires once: their Max Activations aren't shared"), UReactiveTestProbe::CountRuns(TEXT("Shared")), 2);
	TestEqual(TEXT("The asset's own rule is untouched"), Prop<int32>(Once, TEXT("ActivationCount")), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_LaunchDirection, "ReactiveActors.Actions.LaunchActorDirection", REACTIVE_TEST_FLAGS)
bool FReactiveTest_LaunchDirection::RunTest(const FString& Parameters)
{
	// Target at the origin facing +X; the player 100 units behind it (-X), and 50 units higher
	const FTransform Target(FRotator::ZeroRotator, FVector::ZeroVector);
	const FVector Player(-100.f, 0.f, 50.f);
	const FVector Relative(1.f, 0.f, 0.f);
	const FVector World(0.f, 0.f, 1.f);

	const FVector AwayFlat = UACT_LaunchActor::ComputeDirection(EReactiveLaunchDirection::AwayFromInstigator, Target, &Player, Relative, World, 0.f);
	TestTrue(TEXT("Away, no lift: straight away along the ground (the height difference is ignored)"), AwayFlat.Equals(FVector(1.f, 0.f, 0.f), 0.001f));

	const FVector AwayLifted = UACT_LaunchActor::ComputeDirection(EReactiveLaunchDirection::AwayFromInstigator, Target, &Player, Relative, World, 45.f);
	TestTrue(TEXT("Away, 45 lift: up and away equally"), AwayLifted.Equals(FVector(0.7071f, 0.f, 0.7071f), 0.001f));

	const FVector Towards = UACT_LaunchActor::ComputeDirection(EReactiveLaunchDirection::TowardsInstigator, Target, &Player, Relative, World, 0.f);
	TestTrue(TEXT("Towards: back at the player"), Towards.Equals(FVector(-1.f, 0.f, 0.f), 0.001f));

	const FVector NoInstigator = UACT_LaunchActor::ComputeDirection(EReactiveLaunchDirection::AwayFromInstigator, Target, nullptr, Relative, World, 0.f);
	TestTrue(TEXT("No instigator: the target's forward"), NoInstigator.Equals(FVector(1.f, 0.f, 0.f), 0.001f));

	const FTransform Turned(FRotator(0.f, 90.f, 0.f), FVector::ZeroVector);
	const FVector Rel = UACT_LaunchActor::ComputeDirection(EReactiveLaunchDirection::TargetRelative, Turned, nullptr, Relative, World, 0.f);
	TestTrue(TEXT("Relative turns with the target"), Rel.Equals(FVector(0.f, 1.f, 0.f), 0.001f));
	return true;
}

// =================================================================================================
// Validation
// =================================================================================================

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReactiveTest_PresetValidation, "ReactiveActors.Validation.PresetProblemsReported", REACTIVE_TEST_FLAGS)
bool FReactiveTest_PresetValidation::RunTest(const FString& Parameters)
{
	UReactionPreset* Preset = NewObject<UReactionPreset>(GetTransientPackage());

	// No event (warning), an empty action slot (warning), Play Sound with no Sound (error)
	FReaction Broken;
	Broken.Actions.Add(nullptr);
	Broken.Actions.Add(NewObject<UACT_PlaySound>(Preset));
	Preset->Reactions.Add(Broken);

	FDataValidationContext Context;
	const EDataValidationResult Result = Preset->IsDataValid(Context);
	TestTrue(TEXT("Invalid: Play Sound has no Sound"), Result == EDataValidationResult::Invalid);
	TestTrue(TEXT("At least one error"), Context.GetNumErrors() >= 1);
	TestTrue(TEXT("Warnings for the missing event and the empty slot"), Context.GetNumWarnings() >= 2);

	// A clean preset passes
	UReactionPreset* Clean = NewObject<UReactionPreset>(GetTransientPackage());
	Clean->Reactions.Add(MakeReaction(Ping(), { MakeProbe(Clean, TEXT("Fine")) }));
	FDataValidationContext CleanContext;
	TestTrue(TEXT("A good preset is valid"), Clean->IsDataValid(CleanContext) != EDataValidationResult::Invalid);
	TestEqual(TEXT("With no warnings"), CleanContext.GetNumWarnings(), 0);
	return true;
}
#endif

#endif // WITH_DEV_AUTOMATION_TESTS

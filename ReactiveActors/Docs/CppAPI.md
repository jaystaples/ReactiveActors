# Reactive Actors: C++ API

What's meant for you to extend or call from C++, what's internal, and the rules the plugin enforces.
Blueprint users can skip this page.

## Modules

| Module | Type | What's in it |
|---|---|---|
| `ReactiveActors` | Runtime | Everything that runs in game. Depend on this one. |
| `ReactiveActorsNodes` | UncookedOnly | Custom Blueprint nodes (Run Action, hidden from the menu for now). Loaded wherever Blueprints are compiled, never in a packaged game. |
| `ReactiveActorsTests` | Editor | Automated tests (Tools > Session Frontend > Automation, filter ReactiveActors). Never shipped. |
| `ReactiveActorsEditor` | Editor | Icons, asset shortcuts, the Finish Action compile check and validator, tag filters, Place Actors > Reactive. |

In your `.Build.cs`: `PublicDependencyModuleNames.Add("ReactiveActors");` (plus `"GameplayTags"`).

## Extend these

Subclass in C++ (or Blueprint). Override the `_Implementation` of the events, or the plain C++ virtuals listed.

**`UReactiveAction`** (`ReactiveAction.h`): one step of a reaction.
- `OnExecuteAction_Implementation(Context)`: do the work, then call `FinishAction(bSuccess)` now or later. **Always.**
- `OnCancel_Implementation()`: stop timers and bindings when the run is cancelled. Don't call Finish Action here.
- `OnLoad_Implementation()`, `OnCleanup_Implementation()`, `OnReset_Implementation()`: setup, teardown, clear state between runs.
- `DescribeAction()`: the row title ("Delay 2.0s").
- Actions that hold other actions (like Branch): `GetNestedActions`, `NestedActionsKeepTarget`, `GetActiveInnerSequence`,
  and run them with an `FActionRunner`. Set `bAvailableInBlueprintRunAction = false` in the constructor if a Run Action node's pins
  can't fill them in.
- `bAllowExtraRequirements = false` in the constructor to hide Extra Requirements.

**`UReactiveRequirement`** (`ReactiveRequirement.h`): a check about the moment.
- `CheckRequirement_Implementation(Context, FailReason)`: write the plain check; Not is applied for you. Must be stateless.
- `DescribeRequirement()`, `ChecksInstigator()`, `CanCheckAhead()` (false if Map Check can't test it ahead of time).
- Built-in style with a Target/Instigator choice: subclass `UREQ_ActorCheckBase` and use `GetCheckedActor`.

**`UReactiveRule`** (`ReactiveRule.h`): memory across runs (Max Activations, Cooldown).
- `ShouldBlock_Implementation`, `OnStart_Implementation`, `OnFinish_Implementation(Context, Result)`,
  `OnReset_Implementation`, `GetDebugString_Implementation` (live state for the overlay).

**`UReactiveTrigger`** (`ReactiveTrigger.h`): watches for something and sends an event.
- `OnActivate_Implementation` / `OnDeactivate_Implementation`: bind and unbind. Call `FireTrigger(Instigator)` when it happens.
- `OnTick_Implementation` with `bTickInCpp = true` in the constructor, for polled conditions.
- Event-driven triggers: `GetHeardEvents` and `HandleEventToActor`.

All four: `GetSummary` (Blueprint row title), `ValidateSettings_Implementation(Errors, Warnings)` (problems with
this block's settings, for Data Validation and Map Check) and `GetOwnerObject` / `GetOwningActor` come from
`UReactiveObjectBase`.

## Call these

- **`UReactionSubsystem`** (`ReactionSubsystem.h`), one per game world:
  `BroadcastEvent`, `BroadcastEventFrom`, `BroadcastEventToActor`, `BroadcastEventToIdentity`,
  `SetEventsSuspended`, `CancelAllReactions` and the `Cancel/Reset...For...` family,
  `RunAction`, `RunReactionPreset`, `GetActorsByIdentity`.
  `OnEventSent` (C++ delegate) is the one hook for "an event happened"; `EventMatches` and `EventSentToIdentity`
  are the matching rules everything uses.
- **`UReactionComponent`** (`ReactionComponent.h`): `ReactTo`, `CancelReactions`, `ResetReactions`,
  `SetReactionsEnabled`, `GetEventTags`, `GetIdentityTags`, `OnReactionStarted`, `OnReactionFinished`.
- **`UReactiveActorsLibrary`**: the Blueprint versions of the subsystem calls.
- **`AReactiveVolume`**: `IsActorInside`, `GetActorsInside`, `IsActorInVolumeWithIdentity`.

## Save and load

- **`UReactiveStateCache`** (`ReactiveStateCache.h`, Game Instance subsystem): the in-memory cache of what changed in
  the current level. `GetWorldState`, `SetWorldState`, `ClearAll`, and `OnWorldReactionStateExpiring` (fired from
  `FWorldDelegates::OnWorldCleanup` after the level's components have written their state). Components call
  `StoreActor` in EndPlay (any reason but Destroyed; streaming out only with Keep State When Streamed Out) and
  `TakeActor` in BeginPlay, before triggers activate. Only components with `HasStateChanged()` are stored.
- **Structs** (`ReactiveSaveData.h`): `FReactiveWorldState` (one per level, keyed by `ReactiveSave::GetWorldKey`, the
  map package name without the PIE prefix) holding `FReactiveActorState` per changed component (keyed by
  `UReactionComponent::GetSaveKey`). Reactions and triggers are matched by `FReaction::ReactionId` /
  `UReactiveTrigger::TriggerId` (assigned in `PostEditChangeProperty` on components and presets, duplicates
  re-rolled), falling back to list position plus event tag or class for content saved before IDs existed.
- **Per component**: `CaptureReactionState` / `RestoreReactionState` (`FReactiveActorState`), for actors spawned at
  runtime, which the cache leaves out (`UReactionComponent::IsPlacedInLevel`, i.e. `AActor::IsNetStartupActor`).
  A restore before Begin Play is applied at Begin Play.
- **Versions and renames**: each `FReactiveSavedObject` records `UEVer`, licensee version and the custom versions its
  properties used, and is read back with them. `ReactiveSave::IsSameClass` matches the saved class path directly,
  through `FCoreRedirects`, or by resolving the path (Blueprint redirectors).
- Rule and trigger state is their `SaveGame` properties, written with Unreal's tagged property serialisation
  (`ReactiveSave::SaveObject` / `LoadObject`: SaveGame archive, no delta, not persistent, so Transient SaveGame
  runtime counters are included). Override `PreSaveState` / `PostLoadState` (C++ only) to convert values that don't
  survive a load as they are (see `URULE_Cooldown`). Actions are saved too (`UReactionComponent::GatherActions`:
  depth first, nested after their parent). `ReactiveSave::GatherSaveGameProblems` (editor) flags SaveGame actor or
  component references and Timer Handles, and SaveGame on requirements; `UReactiveActionValidator` runs it on every
  Reactive Actors Blueprint.
- **Load timing**: Begin Play triggers fire on the next tick. `UReactionSubsystem::HoldUntilRestored` queues trigger
  fires and Reactive Volume sends (`DeferIfHolding`) until `SetWorldState` or `ReleaseHeld`, with a safety release.

## Internal (may change without notice)

`FActionRunner`'s internals beyond `Start`, `Cancel`, `IsRunning` and `GetContext`; `UReactionOneShot`;
`FReactivePresetRunSlot`; `UReactiveTargetRun`; `UReactiveActorsLibrary::CreateActionForRun` and `RunAction`
(they back the Run Action node); `UK2Node_RunReactiveAction`; everything in `ReactionSubsystemDebug.cpp`;
`UReactiveRunReactionPreset` beyond its Blueprint node.

## Rules the plugin enforces

| Rule | What happens if it's broken |
|---|---|
| Every action calls Finish Action once per run | Blueprint: compile warning if it's never called. Calling it twice in one run: a warning (Blueprint) or an `ensure` with a call stack (C++), once per class. A Finish Action after a cancel is expected and ignored. |
| The same action object never runs twice at once | Run Action and the runners refuse it with a warning. Calling `Execute` on a running action from C++ hits an `ensure`. |
| Actions with Available in Blueprint Run Action off aren't used with Run Action | Compile error on the node; refused at runtime. |
| Nothing runs from a construction script | Run Action and friends refuse with a warning. |

## Validation

- `IsDataValid` on `UReactionComponent`, `UReactionPreset` and `AReactiveVolume` reports through Data Validation;
  `CheckForErrors` (Map Check) reports the same problems. Both walk the blocks with the internal
  `ReactiveValidation` helpers and call each block's `ValidateSettings`.
- `UReactiveActionValidator` (editor) flags action Blueprints that never call Finish Action.

## Logging and performance

- Log category **`LogReactiveActors`** (`ReactiveActorsLog.h`). Console: `Log LogReactiveActors Verbose` for more detail.
- **`stat Reactions`**: time spent sending events, reacting, executing actions, checking requirements, firing
  triggers and drawing debug, plus events, reactions and actions per frame and one-off runs going. The same
  scopes appear in Unreal Insights captures.
- Debug tools (`Reactions.*` console commands, the overlay, wiring lines, history) are compiled out of shipping builds.

## Tests

`Source/ReactiveActorsTests`: each test builds a small game world (`FReactiveTestWorld`), adds actors with reactions
made of probe actions (`UReactiveTestProbe` logs each run; finish it yourself or straight away) and checks what ran.
Copy an existing test to add one. Run them before every change you send out:

```
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests ReactiveActors;Quit" -unattended -nullrhi
```

## Before a release

Package the plugin on its own, which builds every file separately and catches missing includes:

```
RunUAT.bat BuildPlugin -Plugin="<path>\ReactiveActors.uplugin" -Package="<output folder>" -TargetPlatforms=Win64
```

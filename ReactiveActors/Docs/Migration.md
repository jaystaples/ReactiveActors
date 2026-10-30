# Composable Game Actions → Reactive Actors

This version renames the plugin, its modules, files, classes and Blueprint nodes. Existing assets,
levels and Blueprints keep loading through the CoreRedirects in `Docs/CoreRedirects.ini`.

It also includes the earlier changes that weren't on disk yet: Triggers3 (requirement Check, triggers
with no event, On Fired), Reaction Preset with triggers, and the trigger instigator default.

## Steps

1. **Close the editor and back up** the project (or commit).
2. **Rename the plugin folder**: `Plugins/ComposableGameActions` → `Plugins/ReactiveActors`.
3. **Delete from it**: `Source/`, `ComposableGameActions.uplugin`, `Config/Tags/GameActionsTags.ini`,
   `Binaries/` and `Intermediate/`. Keep `Content/` and `Resources/`.
4. **Unzip** this archive into `Plugins/`, so its files land in `Plugins/ReactiveActors`.
5. **Project files outside the plugin**:
   - `BeyondTheFrame.uproject`: if it lists `ComposableGameActions` under Plugins, change it to `ReactiveActors`.
   - Any game module `.Build.cs` that depends on `ComposableGameActions`: change it to `ReactiveActors`,
     and update any C++ includes of the old headers (file map below).
   - `Config/DefaultEngine.ini`: paste the `[CoreRedirects]` block from `Docs/CoreRedirects.ini`,
     replacing the earlier GameActionPreset / ComposableGameActions redirect lines.
   - `Config/DefaultGameplayTags.ini`: add the three tag redirects at the bottom of `CoreRedirects.ini`.
   - Any config that sets `GameActions.*` console variables: rename them to `Reactions.*`.
6. **Regenerate project files** and do a full build (not Live Coding).
7. **In the editor**:
   - Right-click the plugin's Content folder → **Fix Up Redirectors**, then **File → Save All**.
     Re-save every level that uses Reaction Components too.
   - Rename the demo assets you want to (see the sheet), then Fix Up Redirectors again.
   - Blueprints: **Run Reaction**'s struct pin is now called *Reaction* (was *Protocol*): reconnect it if a
     wire dropped. Bound **On Reaction Started / Finished** events may need Refresh Nodes.
8. Keep the redirects until everything has been re-saved. They're safe to leave in.

## File map

| Old | New |
|---|---|
| ComposableGameActions.h / .cpp (module) | ReactiveActors.h / .cpp |
| ComposableGameActionObjectBase | ReactiveObjectBase |
| GameAction | ReactiveAction |
| GameActionBranch | UACT_Branch, now in BuiltInActions (ReactiveBranch.h/.cpp removed) |
| GameActionRequirement | ReactiveRequirement |
| GameActionBehavior | ReactiveRule |
| GameActionTrigger | ReactiveTrigger |
| GameActionProtocol | Reaction |
| GameActionComponent | ReactionComponent |
| GameActionPreset (was GameActionProtocolData) | ReactionPreset |
| GameActionSubsystem | ReactionSubsystem |
| GameActionLibrary | ReactiveActorsLibrary |
| GameActionContext | ReactionContext (now also holds EReactiveScopePolicy) |
| GameActionSequence | ActionRunner |
| BuiltInBehaviors | BuiltInRules |
| BuiltInActions, BuiltInRequirements, BuiltInTriggers | unchanged |

## Behaviour changes in this pass

- **One scope enum**, `EReactiveScopePolicy` (World, Self, Identity), replaces the trigger scope,
  Broadcast Event's scope and Event Received's listen scope.
  - **Broadcast Event** gains **Identity** (with Target Identity). "Target Only" is now **Self**.
    All three scopes go through the subsystem, so World-listening Event Received triggers hear them.
  - **Event Received**: "This Actor" is now **Self**. Identity is hidden for listening.
- Categories: Reactive Actors (component, library, subsystem, context), Reactive Action,
  Reactive Rule, Reactive Requirement, Reactive Trigger. Built-in property categories stay short
  (Action, Rule, Requirement, Trigger).

## Finish Action and results (second pass)

- **Every action calls Finish Action** when it's done, with **Success** (on by default). The Latent
  setting is gone. An action that doesn't implement Execute Action finishes straight away.
  Compiling a Blueprint action whose Execute Action never calls Finish Action gives a warning.
- **Finish Action (Success off)** fails the run: the rest of the actions are skipped, and any Branch
  around it and the reaction fail too. Inside Run On Targets, only that target's actions fail.
- **Results**: rules' On Finish and the component's On Reaction Finished now get a Result
  (Succeeded, Failed, Cancelled) instead of the Cancelled bool. The overlay shows "failed at <action>".
- **Stop Reaction** replaces the Cancel Reaction action, with a Result (Succeeded or Failed).
  The Blueprint function Cancel Reaction on actions is gone: use Finish Action (Success off).
- **Execute Action** has a single **Context** pin (Target, Instigator, Event). Context's Target is never empty there.
- **Run On Targets** runs every target at once, each with its own copy of the actions.
- **Requirements**: Check Requirement always gets the real context. The Check (Target / Instigator)
  setting moved from the base class onto the built-in requirements.

Existing Blueprints to touch up after this pass:
- Actions: re-open each and refresh its **Execute Action** event (new Context pin), then make sure
  it calls Finish Action; replace any **Cancel Reaction** call with Finish Action (Success off).
- Rules: re-add **On Finish** (the Cancelled pin is now Result).
- Anything bound to **On Reaction Finished**: refresh the node (new Result pin).

## Branch (third pass)

- **Branch** is now a built-in like the others (`UACT_Branch`, in BuiltInActions) and can't be subclassed.
  Its logic is its Conditions only: for custom checks, make a Reactive Requirement and add it there.
  Extra Requirements are hidden (and ignored) on Branch, so Conditions are its only checks.
- Delete `ReactiveBranch.h` and `ReactiveBranch.cpp` from the plugin's Source if they're still there.
- Any Blueprint child of Branch stops loading as a Branch: rebuild it with a Reactive Requirement.

## Editor quality of life (fourth pass)

- **Row titles**: every action, rule, requirement and trigger shows a one-line summary as its row title in
  arrays ("Delay 2.0s", "Broadcast Event.Door.Open -> World"). The same text is under Advanced > Summary.
- **New action Blueprints** start with Execute Action wired to Finish Action.
- **Project Settings > Plugins > Reactive Actors**: tag roots for event and identity pickers, logging
  switches, and the debug overlay defaults (applied to the Reactions.* console variables).
- Tooltip pass across the built-ins and base classes.

## Events: waiting and history (fifth pass)

- **One hook for every listener**: the subsystem's `OnEventSent` now carries an `FReactiveEventInfo`
  (event, instigator, source, route, target actor or identity), and `EventPasses` is the one filter.
  The Wait For World / Identity Event nodes, the Wait For Event action and the Event Received trigger
  all use them. `FReactiveEventInfo` is C++ only.
- **Wait For World Event / Wait For Identity Event (Blueprint nodes)**: from any Blueprint, fire On Event
  (Event, Instigator) for each matching event. World hears the event however it's sent; Identity only
  when it's sent to that identity (or a parent of it). Only Once and Exact Match options; Cancel it from
  its Async Action pin.
- **Wait For Event (action)**: waits inside a reaction until the event comes, World or Identity scope.
  No timeout: it waits until the reaction is cancelled or the actor ends play.
- **History**: `Reactions.History [N]` prints the last events (sender, receivers, who fired) and how each
  reaction ended; `Reactions.Show.History N` shows them on screen.
- C++ only: `BroadcastEventToActor` and `BroadcastEventToIdentity` take an optional Source.

## Sound, VFX and more requirements (sixth pass)

- **Niagara**: the plugin now depends on it (`ReactiveActors.uplugin` and `ReactiveActors.Build.cs`).
  Regenerate project files and do a full build.
- **Play Sound** (action): 2D, At Target or Attached to Target (with Socket); Volume, Pitch, Attenuation.
  Wait Until Finished holds the reaction until the sound ends (looping sounds never hold it), and
  cancelling then stops the sound.
- **Spawn VFX** (action): a Niagara system at the target, or attached to it (Socket), with Offset,
  Rotation and Scale. Always carries on straight away.
- **Is Class**, **Has Identity**, **Random Chance** (requirements).
- **Not** on every requirement: passes when its check fails. Summaries show it ("Not Is Class: BP_Door").
  C++ requirements that override `BuildSummary` should override `DescribeRequirement` instead.
- C++: `UReactiveRequirement::CanCheckAhead()` decides what Map Check tests ahead of time
  (false for instigator checks and Random Chance).
- New action Blueprints no longer get a comment bubble on Execute Action.

## Random Action, reaction switches and summaries (seventh pass)

- **Random Action**: picks one of its Options by Weight and runs its actions in its place (Avoid Repeat
  optional). Use Extra Requirements to gate it.
- **Set Reaction Enabled** and **Reset Reactions** actions, aimed at the target's Reaction Component.
  New Blueprint function on the component: **Set Reactions Enabled** (Event optional). Reactions disabled
  in the editor can now be switched on in play; Get Event Tags reflects the live state while playing.
- **Spawn VFX**: Offset, Rotation and Scale are one **Transform** now. Re-enter any you'd set.
- **Event Scope**: the trigger's and Broadcast Event's Scope is shown as Event Scope (same property, no redirect).
- **Get Summary**: override it in a Blueprint action, requirement, rule or trigger for your own row title.
  C++ subclasses keep using BuildSummary / DescribeAction / DescribeRequirement / DescribeTrigger.
- **Persistent** is hidden from the Details panel until save and load exists.
- New action Blueprints: the engine's "This node is disabled" comment bubble is cleared too.
- **Class icons**: Reactive Action, Requirement, Rule and Trigger have their own icons (class pickers, the
  Blueprint parent class, Content Browser list and tiles), from SVGs in `Resources/Icons`. Every built-in
  and Blueprint subclass picks up its base class's icon. The editor module now depends on Projects,
  Slate and SlateCore.

## Pass 8: running without a component, Reactive Volume

New:
- **Run Action** node (editor): pick the action class on the node; its Instance Editable + Expose on Spawn
  variables appear as pins, like Spawn Actor from Class. Outputs the Action and Started. Replaces
  Construct Object from Class + Run Action. Built-in actions show their settings as pins too (not nested
  action or requirement lists: Branch, Random Action and Run On Targets are for reactions).
- **Run Reaction Preset** (async node, event graphs only): takes a Reaction Context (Target, Instigator, Event;
  Make Reaction Context or split the pin) and runs a Preset's reactions on its Target. Event picks which reactions
  (empty: all). Then fires straight away, On Finished (Result) when they've all ended, On Blocked if nothing
  started. Rules keep state per target between calls; Retrigger applies; Reset All Reactions clears them.
- **Reactive Volume** (Place Actors > Reactive): a box (Size, or the scale tool) with the trigger icon.
  Volume Identity Tags (what In Volume asks for), Target Actor Identity (only actors with it count, for
  events and In Volume), Enter and Leave events to World or an Identity, Conditions to narrow the events.
- **In Volume** requirement: the instigator (or target) is inside a Reactive Volume with that identity.

Changed:
- The old **Run Action** node (taking an action object) is no longer in the menu: it's now internal to the new
  Run Action node. Existing nodes keep compiling; replace them with the new node (pick the class on it).
- **Run Actions** and **Run Reaction** are deprecated: Blueprints using them still work but show a warning.
  Swap them for Run Action / Run Reaction Preset when convenient. They're hidden from the node menu (unless
  Editor Preferences > Blueprint Editor > Expose Deprecated Functions is on).
- **Reaction Context** members are now Blueprint read-write, so Blueprints can make one.
- Built-in actions' settings are Blueprint read-only (Expose on Spawn), so the Run Action node shows them.
- C++: `UReactionSubsystem::StartOneShot` returns the run (or null) and takes an optional OnFinished callback.

## Pass 9: code quality

Files to **delete** before unzipping (they moved):
- `Source/ReactiveActors/Private/BuiltInActions.cpp` (now `Private/Actions/*.cpp`)
- `Source/ReactiveActorsEditor/K2Node_RunReactiveAction.h` and `.cpp` (now in `Source/ReactiveActorsNodes/`)

Then regenerate project files and do a full build.

- **New module `ReactiveActorsNodes`** (UncookedOnly) holds the Run Action node, so Blueprints using it also load
  when the game runs from project files. Existing nodes follow automatically through the redirect in the plugin's
  new `Config/DefaultReactiveActors.ini` (also listed in `Docs/CoreRedirects.ini`).
- **Built-in actions split** into `Actions/FlowActions`, `EventActions`, `WorldActions` and `PresentationActions`.
  Class names and asset paths are unchanged; `BuiltInActions.h` still includes them all.
- **Subsystem split**: event routing stays in `ReactionSubsystem.cpp`; console commands, the overlay, wiring lines
  and history moved to `ReactionSubsystemDebug.cpp`.
- **One log category**, `LogReactiveActors`, for every runtime log line (was LogTemp and five per-file categories).
- **`stat Reactions`** and Unreal Insights scopes on the hot paths.
- **Contracts**: a second Finish Action in one run warns (Blueprint) or ensures (C++) once per class; Execute on a
  running action ensures.
- **Available in Blueprint Run Action** (Class Defaults, advanced): off on Branch, Random Action and Run On Targets. The Run Action
  node refuses those with a compile error.
- `Docs/CppAPI.md`: what's for extending, what's internal, the enforced rules, and the release packaging command.

## Pass 10: Data Validation

- **Data Validation** (on save, right-click > Validate, and before submitting) now checks Reaction Components
  (in levels and Blueprints), Reaction Presets and Reactive Volumes: empty slots, reactions with no event or no
  actions, triggers with nothing to do, and each block's own settings. Errors fail validation; warnings don't.
- **Validate Settings** on every action, requirement, rule and trigger: override it (C++ or Blueprint) to report
  problems with that block's settings. The built-ins do: Play Sound with no Sound, Broadcast Event with no Event,
  Branch with no conditions, Random Action where every Weight is 0, Has Identity with no Identity, and so on.
- **Map Check** uses the same checks (plus requirements tested against the actor), so the two always agree.
- **Action Blueprints that never call Finish Action** are flagged by Data Validation too, not only when compiling.
- The editor module depends on the **DataValidation** plugin (enabled by default in the engine; now listed in the
  .uplugin).
- **Run Action node hidden** from the node menu for now; placed ones still work. **Wait For Event** is no longer
  available in it either.

## Pass 11: automated tests

- New editor-only module **ReactiveActorsTests** (never in a packaged game): 23 tests covering event matching,
  identity and actor sends, instigators, chain order, failure, skipping, cancel, destroying mid-run, Retrigger
  Ignore and Restart, Max Activations and Reset, Cooldown, Delay, Branch and Not, Random Action weights,
  Run On Targets, Wait For Event, Set Reactions Enabled, Suspend Events, Run Reaction Preset, Presets copied per
  actor, and Data Validation of a Preset.
- Run them from **Tools > Session Frontend > Automation**, filter `ReactiveActors`, or headless:
  `UnrealEditor-Cmd.exe BeyondTheFrame.uproject -ExecCmds="Automation RunTests ReactiveActors;Quit" -unattended -nullrhi`
- The tests register their own gameplay tags under `ReactiveTest.*` (Ping, Ping.Child, Pong, Identity.A and B).
  They show in the editor's tag pickers; ignore them.

## Pass 12: Event Relay, Gate and Counter

New placeable logic actors under **Place Actors > Reactive**, next to the Reactive Volume. Invisible in game; in the
editor each shows the trigger icon and an orange label saying what it does. All hear events wherever they're sent,
send their own **Send Event** to an **Event Scope** (World or Identity), and pass the instigator on.
- **Reactive Event Relay**: When (an event) -> Send Event, with optional Delay, Chance and Max Times.
- **Reactive Gate**: sends once it has heard every event in Needs, in Any Order or In Order (hearing one out of turn
  starts again, optional). Repeat to re-arm after sending.
- **Reactive Counter**: Count (an event), Send At N, once or Every Time.
- Reset All Reactions (and For Actor / For Event) resets them. `Reactions.Show 1` draws their state above them.
- Data Validation and Map Check flag missing events and loops (sending the event they listen for).
- C++: `UReactionSubsystem::OnReset` tells anything with its own state that a reset happened.
- 4 new tests (27 in all).

## Pass 13: logic actors removed, one-off run cleanup

- **Removed: Reactive Event Relay, Gate and Counter** (and `ReactiveLogicActors.h/.cpp`). Shared or game-wide state
  ("the third time", "all three notes read") belongs in your own system, such as a fact database, driven from
  Blueprint actions and requirements. A reaction already covers the Relay: Event Received, Delay, Broadcast Event.
  Delete any you placed before updating, or the level will warn that their class is missing.
  `UReactionSubsystem::OnReset` stays, so your own system can reset itself with Reset All Reactions.
- **Reactive Volume** stays, alone in Place Actors > Reactive.
- **Run Reaction Preset**: a finished run is let go straight away. Before, the per-target slot held the last
  run (and its action copies) until the same preset reaction ran on that target again.
- **stat Reactions** has a new **Run Cleanup** line: the subsystem's per-tick check for one-off runs whose
  target has gone.
- 4 tests removed with the actors.
- **Stop Reaction If Not Met** (new, on every action next to its requirements): if a requirement fails, the
  whole reaction stops there and ends as **Failed**, instead of skipping that action. Off by default, so existing
  reactions behave as before. Set it in an action Blueprint's Class Defaults to make it that class's default;
  each placed action can change it. The row shows "(stops if not met)". Hidden on actions without Extra
  Requirements (Branch and friends). Data Validation warns if it's on with no requirements to check.
- 1 new test (24 in all).

## Pass 15: a leaner core, and Reactive Actors Extras

- **New sibling plugin: Reactive Actors Extras** (`Plugins/ReactiveActorsExtras`). Holds the built-in actions that need
  more than the engine core: **Spawn VFX** (Niagara) and **Set Data Layer State** (World Partition). Enable it next to
  Reactive Actors to keep using them. Existing reactions keep working: `DefaultReactiveActors.ini` redirects the old
  class names to the new module. With Extras disabled, those slots load empty and Data Validation flags them.
- **Reactive Actors no longer needs Niagara** (removed from the `.uplugin` and `ReactiveActors.Build.cs`).
- **Set Actor Hidden** now has **Change: Hide / Show / Toggle** instead of the Hidden tick box. Saved actions convert
  on load (Hidden unticked becomes Show). **Toggle Actor Hidden** is deprecated and hidden from the dropdown; existing
  ones still work. A hidden Run Action node with a Hidden pin needs re-wiring to Change.
- **End Play** trigger: hidden from the Triggers dropdown (it fires while actors are on their way out, which is easy
  to misuse). Existing ones still work.
- **Reset Reactions** action: deprecated and hidden from the Actions dropdown. Reset from game code with the
  Reset All Reactions (and For Actor / Identity / Event) nodes. Existing ones still work.
- Data Validation stays in the core plugin.

## Pass 16: save and load

The plugin doesn't write files (apart from two optional helper nodes). It gives you its state as plain structs your
save system carries: no object pointers, matched back up by position and checked against the event tag or class.

- **Reaction Component**: `Capture Reaction State` -> `Reactive Component State`, and `Restore Reaction State`.
  A restore before Begin Play is kept and applied at Begin Play, before any trigger fires.
- **Whole world**: `Capture World Reaction State` -> `Reactive World State`, and `Restore World Reaction State`
  (also on `UReactionSubsystem`). Covers actors placed in levels, keyed by level, actor and component. Components
  in levels not loaded yet are restored when they stream in.
- **Streaming**: a component whose level streams out keeps its state in the subsystem and gets it back when it
  streams in again, with no save involved.
- **No save system?** `Save Reactions To Slot` / `Load Reactions From Slot` (uses `UReactiveSaveGame`).
- **Save systems using the SaveGame archive on components** (Easy Multi Save, hand-rolled): the component fills
  and reads its own `SavedReactionState` property in `Serialize`, so it saves with no extra steps.
- **What's saved**: each reaction's Enabled; every rule's and trigger's SaveGame variables. Built in: Max
  Activations' count, Cooldown's time left. Your own rules and triggers: tick SaveGame on a variable.
  New events on every block: **On Before Save** / **On After Load**, to turn values that can't be saved as they
  are (a world time) into ones that can (time left), and back.
- **Not saved**: runs in progress (they start fresh), actions, Run Reaction Preset calls from Blueprint.
- 3 new tests (27 in all).

## Pass 17: loading without double-ups

- **Nothing restarts on load**, and now nothing fires before the restore either, as long as the restore happens in
  the frame the level loads (a Game Mode or Level Blueprint Begin Play, or a save system's load event).
- **Begin Play trigger**: fires on the next tick for every scope (Self used to fire inside Begin Play). Actors
  beginning play together are listening by then, and a same-frame restore is already in place.
- **Hold Reactions Until Restored** / **Release Held Reactions** (subsystem and library): for save systems that
  restore a few frames late. Triggers and Reactive Volumes queue their fires; Restore World Reaction State releases
  them, after the saved counts are back.
- **Wait For Event**: new **Timeout** (0 = off, the default) and **Fail On Timeout**. Its tooltip now says plainly
  it's for short, local moments, not quest steps: a waiting reaction isn't saved.
- 2 new tests (29 in all).

## Pass 18: hold-then-restore

- **Recommended load setup**: as the level starts (Game Mode Begin Play), if you're loading a save, call
  **Hold Reactions Until Restored**; when the save is ready, call **Restore World Reaction State**, which releases
  them. Works however late the restore comes. Restoring straight away in Begin Play without holding also works.
- **Hold Safety Release** (Project Settings > Plugins > Reactive Actors > Save and Load, default 10 s, 0 = off):
  a hold with no restore lets go by itself, with a warning.
- **Keep State When Streamed Out** (same place, default on): components keep their state across streaming out and
  in, no save needed. Off: they start fresh each time they stream in.
- No switch for saving itself: nothing is saved unless something calls the save nodes, so projects that don't save
  simply don't use them.

## Pass 19: the Reactive State Cache

Save and load reshaped around one struct per level, **Reactive World State**, and an in-memory cache.

- **New: `UReactiveStateCache`** (Game Instance subsystem). Components where something happened (a reaction ran, a
  trigger fired, Enabled changed, a restore) write their state to it when they stream out or the level ends, and take
  it back when they begin play. Untouched actors take no space.
- **Nodes**:
  - **Get World Reaction State**: the current level's state, loaded and streamed-out actors, to put in your save.
  - **Set World Reaction State**: before Open Level (the level gets it as it starts), or after the level loads
    (applied straight away). Set then Open Level of the same map (a reload) also works.
  - **Clear World Reaction State**: forget all cached and waiting state (New Game).
  - **On World Reaction State Expiring** (event on the Reactive State Cache): a level is ending; store the struct if
    you want to come back to it. The cache then forgets it, so only the current level is held.
  - **Hold Reactions Until Restored** / **Release Held Reactions**: unchanged; Set World Reaction State releases.
    Hold does nothing if the level already got its state as it started.
- **Reaction and trigger IDs**: assigned in the editor when you edit a component or preset (copies get new ones), so
  saves still match after reordering. Content not edited since keeps matching by position and event tag.
- **Removed**: Capture / Restore World Reaction State, Save / Load Reactions To Slot (`UReactiveSaveGame`), the
  component's automatic SaveGame-archive hook for Easy Multi Save, and the subsystem's stash. Easy Multi Save, SPUD and
  Unreal's SaveGame all just store the Reactive World State struct.
- **Renamed**: `FReactiveComponentState` is now `FReactiveActorState` (with its key inside).
- Tests: 30 in all (cache round trip, cached state applied at Begin Play).

## Pass 20: engine-upgrade-safe saves, renamed classes, spawned actors

- **Saved rule and trigger data records the engine's file versions** it was written with (and any custom versions its
  properties used), and is read back with those versions, as Unreal's own Save Game files do. Saves survive engine
  upgrades that change how properties are stored. Still bytes, so they can't be edited by hand.
- **Renamed classes are followed**: saved state stores each rule's and trigger's full class path, and on load follows
  Core Redirects (C++ classes) and redirectors (Blueprint classes moved or renamed). Before, a renamed rule silently
  started fresh.
- **Spawned actors are no longer cached or included in Get World Reaction State**, only actors placed in levels
  (including streamed sublevels and World Partition cells). A spawned actor comes back with a different name, so its
  entry could never be used again. Save a spawned actor's reactions with **Capture Reaction State** into a SaveGame
  variable on the actor, and **Restore Reaction State** when your save system re-spawns it. (We may revisit this with
  a Save ID later.)
- 1 new test (31 in all).

## Pass 21: actions saved, SaveGame checks

- **Actions' SaveGame variables are saved** with their reaction, including actions nested in Branch, Random Action
  and Run On Targets (matched by position and class). Built in: **Random Action**'s last pick, so Avoid Repeat holds
  after a load. Runs in progress still aren't saved.
- **On Before Save / On After Load are gone from Blueprint.** They're now C++-only virtuals, `PreSaveState` /
  `PostLoadState`, on every block (Cooldown uses them). Blueprint blocks just tick SaveGame on plain values.
- **Data Validation checks SaveGame variables** on every Reactive Actors Blueprint (actions, requirements, rules,
  triggers): actor or component references and Timer Handles warn, with what to save instead (an Identity Tag; let the
  timer restart, or use Cooldown). SaveGame on a requirement warns that requirements are never saved.
- Blocks with nothing ticked SaveGame add (almost) nothing to a save.
- 2 new tests (33 in all).

## Pass 22: Launch Actor

- **New action: Launch Actor** (World). Sends the target flying: **Away From Instigator** (default), **Towards
  Instigator**, **Relative to Target** or a **World Direction**. Instigator directions are worked out from above and
  tilted up by **Lift** (default 25 degrees) so things arc instead of skidding or diving into the floor. **Speed** is a
  speed (cm/s), not a force, so a vase and a chair fly the same; **Spread** adds a little randomness; **Spin** a tumble.
- Characters are launched with Launch Character (knockback). Props that aren't simulating start simulating (**Start
  Physics If Needed**); Static props only fly with **Make Movable If Static**, which warns about baked lighting.
- Core plugin: physics is part of the engine, no extra plugin needed.
- 1 new test (34 in all).

## Pass 23: Launch Actor settles

- **Make Movable If Static is gone.** **Start Physics If Needed** now does it all: a Static prop is made Movable, given
  physics collision if it has none, and starts simulating. Nothing to fix up: the old setting is simply dropped.
- **New: Stop Physics When Settled** (off by default). Once the prop has landed and stopped moving (still for half a
  second, or asleep), physics is switched off so it stays where it fell, and whatever Start Physics If Needed changed is
  put back: Static again if it started Static, and its old collision. Launching it again before it settles keeps the
  original settings to put back.
- **New: Wait Until Settled** (off by default). The reaction waits on this action until the prop has come to rest, or a
  launched character is back on the ground.
- **Max Settle Time** (default 10 s): counts as settled after this long even if it's still rolling. 0 = no limit.
- If the reaction is cancelled mid-flight, a prop with Stop Physics When Settled still settles and is put back.
- Baked lighting: a prop that started Static leaves its baked shadow where it was placed. Light it with Movable or
  Stationary lights if it's going to be launched.

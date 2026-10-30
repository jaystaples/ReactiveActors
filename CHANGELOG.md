# Changelog

All notable changes to Reactive Actors. Versions follow [semantic versioning](https://semver.org/): while below 1.0, minor versions (0.**x**.0) may include breaking changes.

Detailed upgrade notes for each change are in [ReactiveActors/Docs/Migration.md](ReactiveActors/Docs/Migration.md).

## [Unreleased]

## [0.1.0] - First public prototype

### Added
- Reaction Component, Reactions, Actions, Requirements, Rules and Triggers, all extendable in Blueprint and C++.
- Reaction Presets for sharing setups between actors.
- Reactive Volume for area-based events.
- Built-in actions: Branch, Random Action, Run On Targets, Delay, Wait For Event, Stop Reaction, Broadcast Event, Set Reaction Enabled, Set Actor Hidden, Launch Actor, Play Sound, Play Level Sequence, Log Message.
- Built-in requirements, rules (Max Activations, Cooldown) and triggers (Begin Play, Timer, Event Received).
- Save and load through any save system, with streaming and World Partition support.
- Debug overlay, wiring lines, event history and console commands.
- Data Validation and Map Check support, including Validate Settings for your own blocks.
- Reactive Actors Extras: Spawn VFX and Set Data Layer State.

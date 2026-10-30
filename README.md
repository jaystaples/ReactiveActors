# Reactive Actors

**Give any actor in your level its own behaviour, straight from the Details panel. No new Blueprint class, no giant Level Blueprint.**

<!-- Add your demo video or GIF here, e.g. ![Reactive Actors demo](Docs/demo.gif) -->

📖 **[Documentation](https://jaystaples.github.io/ReactiveActorsDocs/)** · 🐛 [Report a bug](../../issues/new/choose) · 💬 [Ask a question](../../discussions)

## Why

Most level scripting in a game is tiny one-off moments: a lamp sparks when you pick up a photo, a book flies off a shelf as you walk past, a mesh changes when you look away. Each one normally needs its own Blueprint Actor, or ends up in a huge Level Blueprint.

Reactive Actors lets a designer add that logic to *any* actor, even a plain static mesh or a light. Things in the level **send events** (Gameplay Tags), and anything that cares **reacts**. Nothing needs a reference to anything else.

## How it works

1. Add a **Reaction Component** to any actor.
2. Add a **Reaction**: *when this event arrives, check these things, then do these actions.*
3. Send the event from a trigger, a Reactive Volume, another reaction, or any Blueprint with **Broadcast Event**.

It comes with built-in pieces: actions such as Play Sound, Delay, Branch, Random Action and Launch Actor; requirements; rules such as Max Activations and Cooldown; and triggers. You can write your own **in Blueprint**, and they appear in the same dropdowns. Use a **Reaction Preset** to share one setup across many actors.

Also included:
- **A debug overlay** (`Reactions.Show 1`) showing what each actor listens for, and why something didn't fire.
- **Validation** that catches setup mistakes before you press Play.
- **Save and load** that works with the save system you already use: Unreal's Save Game, SPUD, Easy Multi Save or your own.

## Quick start

1. Copy `ReactiveActors` (and optionally `ReactiveActorsExtras`) into your project's `Plugins` folder.
2. Open the project and let Unreal build the plugin. Your project needs to be able to compile C++.
3. Follow [Getting Started](https://jaystaples.github.io/ReactiveActorsDocs/getting-started) to build your first reaction.
## What's in this repo

| Folder | What it is |
| --- | --- |
| `ReactiveActors` | The core plugin. Needs nothing beyond the engine. |
| `ReactiveActorsExtras` | Optional extra actions that need other engine plugins: Spawn VFX (Niagara) and Set Data Layer State (World Partition). |

## Engine versions

| Engine | Status |
| --- | --- |
| 5.7 | Supported (tested on 5.7.4) |
| 5.6 and earlier | Untested |

## Status

**Early prototype.** Things will change, and saves or setups may need updating between versions; [CHANGELOG.md](CHANGELOG.md) lists what changed. Single-player only for now.

## Contributing

Bug reports, ideas and pull requests are very welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for how. If you're new to open source, issues labelled **good first issue** are a good place to start.

## License

[MIT](LICENSE): free to use in any project, commercial or not.

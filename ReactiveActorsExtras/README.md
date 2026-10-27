# Reactive Actors Extras

Extra built-in actions for Reactive Actors that need more than the engine core:

| Action | Needs |
|---|---|
| **Spawn VFX** | the Niagara plugin |
| **Set Data Layer State** | a World Partition level |

They're common, and faster as C++ classes than as Blueprint actions, but they live here so that Reactive Actors
itself depends on nothing beyond the engine. Enable this plugin next to Reactive Actors to get them in every Actions
dropdown. Reactions saved before the split keep working: Reactive Actors redirects the old class names here.

If this plugin is disabled, any Spawn VFX or Set Data Layer State in your reactions loads as an empty action slot
(Data Validation flags it).

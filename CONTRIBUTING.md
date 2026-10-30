# Contributing to Reactive Actors

Thanks for helping! Every bug report, idea and pull request makes the plugin better.

## Asking questions

Use [Discussions](../../discussions) for "how do I...?" questions and ideas you'd like to talk through. Keep Issues for bugs and concrete feature requests.

## Reporting a bug

[Open a bug report](../../issues/new/choose) and fill in the form. The most useful things to include:

- **Engine version** (e.g. 5.7.4) and plugin version.
- **What you expected, and what happened.**
- **Steps to reproduce**, ideally in a fresh level.
- **The output of `Reactions.History 50`** from the console, right after it goes wrong.
- **Any warnings or errors** in the Output Log (filter by `LogReactiveActors`).

## Suggesting a feature

[Open a feature request](../../issues/new/choose) and describe the *problem* you're trying to solve, not only the solution. There may be a way to do it already, or a simpler one.

Reactive Actors deliberately keeps a small core. Anything that needs another plugin belongs in Reactive Actors Extras, and anything very game-specific is usually better as your own Blueprint action or requirement.

## Making a pull request

1. **Open an issue first** for anything bigger than a small fix, so we can agree on the approach before you spend time on it.
2. **Fork** the repo and create a branch from `main` (e.g. `fix-cooldown-reset`).
3. Make your change, keeping it focused: one fix or feature per pull request.
4. **Build and test:**
   - The project compiles with no new warnings.
   - The automated tests pass: **Tools → Session Frontend → Automation**, search `ReactiveActors`, run them all.
   - For new behaviour, add a test in `Source/ReactiveActorsTests` where you can.
5. **Update the docs** if you changed what something does or how it's set up, and add a line to `CHANGELOG.md` under *Unreleased*.
6. Open the pull request and fill in the template.

## Code style

Match the code around you. In particular:

- **Unreal's coding standard** (tabs, `PascalCase`, `b` prefix for bools).
- **Class prefixes:** `UACT_` for actions, `UREQ_` for requirements, `URULE_` for rules and `UTRG_` for triggers, so they sort together in pickers.
- **Comments and tooltips are written for designers:** short, plain sentences that say what it does and when to use it.
- **No validation warnings when everything is fine:** only warn about real mistakes.
- **The core plugin needs nothing beyond the engine.** New dependencies go in Extras.

## Code of conduct

Be kind and constructive. Everyone here is volunteering their time.

## License

By contributing, you agree that your contributions are licensed under the [MIT License](LICENSE), the same as the rest of the project.

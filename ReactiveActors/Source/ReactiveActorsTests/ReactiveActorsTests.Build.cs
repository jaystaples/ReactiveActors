// Copyright 2026 Backyard Games Pty Ltd

using UnrealBuildTool;

// Automated tests for Reactive Actors. Editor only, so it never ships: run them from
// Tools > Session Frontend > Automation (filter "ReactiveActors"), or from the command line with
//   UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests ReactiveActors;Quit" -unattended -nullrhi
public class ReactiveActorsTests : ModuleRules
{
	public ReactiveActorsTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"ReactiveActors",
			}
		);
	}
}

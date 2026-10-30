// Copyright 2026 Backyard Games Pty Ltd

using UnrealBuildTool;

// Custom Blueprint nodes (Run Action). An "UncookedOnly" module: loaded whenever Blueprints can be
// opened or compiled (the editor, the cooker, the game run from project files) but left out of a
// packaged game, which never needs node classes.
public class ReactiveActorsNodes : ModuleRules
{
	public ReactiveActorsNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"BlueprintGraph",
				"KismetCompiler",
				"UnrealEd", // the Blueprint compiler's message log (FCompilerResultsLog)
				"ReactiveActors",
			}
		);
	}
}

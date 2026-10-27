// Copyright 2026 Backyard Games Pty Ltd

using UnrealBuildTool;

// Reactive Actors Extras: built-in actions that need more than the engine core (Niagara, World Partition).
public class ReactiveActorsExtras : ModuleRules
{
	public ReactiveActorsExtras(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"ReactiveActors",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Niagara",
			}
		);
	}
}

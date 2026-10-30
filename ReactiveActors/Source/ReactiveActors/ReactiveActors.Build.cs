// Copyright 2026 Backyard Games Pty Ltd

using UnrealBuildTool;

public class ReactiveActors : ModuleRules
{
	public ReactiveActors(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public headers only need these
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"DeveloperSettings",
			}
		);

		// Used only inside .cpp files (sound, sequences, the plugin manager)
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"LevelSequence",
				"MovieScene",
			}
		);
	}
}

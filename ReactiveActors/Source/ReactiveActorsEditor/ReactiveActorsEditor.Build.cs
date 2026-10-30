// Copyright 2026 Backyard Games Pty Ltd

using UnrealBuildTool;

public class ReactiveActorsEditor : ModuleRules
{
	public ReactiveActorsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"AssetTools",
				"AssetDefinition",
				"GameplayTags",
				"PropertyEditor",
				"Projects",
				"PlacementMode",
				"DataValidation",
				"Slate",
				"SlateCore",
				"ReactiveActors",
			}
		);
	}
}

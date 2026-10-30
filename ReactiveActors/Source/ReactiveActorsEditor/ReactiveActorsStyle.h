// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Editor icons for the plugin's classes, from SVGs in Resources/Icons:
//   ClassIcon.<Class>       16 px: class pickers, Blueprint parent class, Content Browser list view
//   ClassThumbnail.<Class>  64 px: Content Browser tiles
// Unreal looks up the class tree for these, so every built-in and Blueprint subclass of Reactive Action,
// Requirement, Rule and Trigger gets its base class's icon. Registered by the editor module at startup.

#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;

class FReactiveActorsStyle
{
public:
	static void Register();
	static void Unregister();

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};

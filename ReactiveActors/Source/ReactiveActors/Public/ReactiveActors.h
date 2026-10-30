// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - runtime module

#pragma once

#include "Modules/ModuleInterface.h"

class FReactiveActorsModule : public IModuleInterface
{
public:
	/** Registers the plugin's gameplay tags and applies Project Settings to the debug console variables. */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

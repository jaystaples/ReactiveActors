// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - runtime module

#include "ReactiveActors.h"
#include "ReactiveActorsLog.h"
#include "ReactiveActorsStats.h"

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "ReactiveActorsSettings.h"

DEFINE_LOG_CATEGORY(LogReactiveActors);

DEFINE_STAT(STAT_ReactionsSendEvent);
DEFINE_STAT(STAT_ReactionsReactTo);
DEFINE_STAT(STAT_ReactionsExecuteAction);
DEFINE_STAT(STAT_ReactionsCheckRequirement);
DEFINE_STAT(STAT_ReactionsFireTrigger);
DEFINE_STAT(STAT_ReactionsDebug);
DEFINE_STAT(STAT_ReactionsCleanup);
DEFINE_STAT(STAT_ReactionsEventsSent);
DEFINE_STAT(STAT_ReactionsStarted);
DEFINE_STAT(STAT_ReactionsActionsExecuted);
DEFINE_STAT(STAT_ReactionsOneShots);

void FReactiveActorsModule::StartupModule()
{
	// The plugin's gameplay tags: Plugins/ReactiveActors/Config/Tags/*.ini
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ReactiveActors")))
	{
		UGameplayTagsManager::Get().AddTagIniSearchPath(Plugin->GetBaseDir() / TEXT("Config") / TEXT("Tags"));
	}

	// Project Settings > Plugins > Reactive Actors: overlay defaults into the Reactions.* console variables
	GetDefault<UReactiveActorsSettings>()->ApplyToConsoleVariables();
}

void FReactiveActorsModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FReactiveActorsModule, ReactiveActors)

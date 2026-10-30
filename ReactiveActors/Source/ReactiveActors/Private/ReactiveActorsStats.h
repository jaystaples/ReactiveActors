// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Performance numbers for the plugin. In the console: stat Reactions (time spent per frame, and how many
// events, reactions and actions ran). The same scopes show up in Unreal Insights (CPU track), so a
// capture shows exactly which events and actions cost what.

#pragma once

#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("Reactions"), STATGROUP_Reactions, STATCAT_Advanced);

// Time
DECLARE_CYCLE_STAT_EXTERN(TEXT("Send Event"), STAT_ReactionsSendEvent, STATGROUP_Reactions, REACTIVEACTORS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("React To Event"), STAT_ReactionsReactTo, STATGROUP_Reactions, REACTIVEACTORS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Execute Action"), STAT_ReactionsExecuteAction, STATGROUP_Reactions, REACTIVEACTORS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Check Requirement"), STAT_ReactionsCheckRequirement, STATGROUP_Reactions, REACTIVEACTORS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Fire Trigger"), STAT_ReactionsFireTrigger, STATGROUP_Reactions, REACTIVEACTORS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Debug Drawing"), STAT_ReactionsDebug, STATGROUP_Reactions, REACTIVEACTORS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Run Cleanup"), STAT_ReactionsCleanup, STATGROUP_Reactions, REACTIVEACTORS_API);

// Counts (per frame)
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Events Sent"), STAT_ReactionsEventsSent, STATGROUP_Reactions, REACTIVEACTORS_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Reactions Started"), STAT_ReactionsStarted, STATGROUP_Reactions, REACTIVEACTORS_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Actions Executed"), STAT_ReactionsActionsExecuted, STATGROUP_Reactions, REACTIVEACTORS_API);

// Current
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("One-off Runs Going"), STAT_ReactionsOneShots, STATGROUP_Reactions, REACTIVEACTORS_API);

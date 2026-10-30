// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// The plugin's log category: every runtime log line uses it, so it can be filtered or turned up in one go.
// Console: Log LogReactiveActors Verbose (more detail) or Log LogReactiveActors Warning (quieter).

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

REACTIVEACTORS_API DECLARE_LOG_CATEGORY_EXTERN(LogReactiveActors, Log, All);

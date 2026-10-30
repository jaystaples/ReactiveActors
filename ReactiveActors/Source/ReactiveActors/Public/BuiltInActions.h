// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions
//
// Every built-in action, in one include. They live in four files under Actions/:
//   FlowActions          Branch, Delay, Wait For Event, Stop Reaction, Random Action, Run On Targets
//   EventActions         Broadcast Event, Set Reaction Enabled, Reset Reactions
//   WorldActions         Set Actor Hidden, Toggle Actor Hidden, Set Data Layer State
//   PresentationActions  Play Level Sequence, Play Sound, Spawn VFX, Log Message
// Include just the one you need in new code; this header stays for existing includes.
// Each action calls Finish Action itself: straight away, or when its work is done (Delay, Play Level Sequence).

#pragma once

#include "Actions/EventActions.h"
#include "Actions/FlowActions.h"
#include "Actions/PresentationActions.h"
#include "Actions/WorldActions.h"

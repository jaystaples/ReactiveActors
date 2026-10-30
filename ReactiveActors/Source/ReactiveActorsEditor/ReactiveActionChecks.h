// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Checks on Reactive Action Blueprints, shared by the compile check (ReactiveActionCompilerExtension)
// and Data Validation (ReactiveActionValidator), so both find the same problems.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraphNode;

namespace ReactiveActionChecks
{
	/**
	 * A Reactive Action Blueprint that implements Execute Action but never calls Finish Action anywhere (nor the
	 * parent's Execute Action, which finishes for it): returns its Execute Action node. Otherwise null.
	 * Finish Action called from anywhere in the Blueprint counts: it's often called later, from a timer or event.
	 */
	UEdGraphNode* FindExecuteWithoutFinish(const UBlueprint* Blueprint);

	/** The message for that problem. */
	FText MissingFinishMessage();
}

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Blueprint compile check for Reactive Actions: every action must call Finish Action, or the
// reaction running it waits forever. When a Reactive Action Blueprint implements Execute Action
// but never calls Finish Action anywhere in the Blueprint, compiling it gives a warning in the
// Compiler Results, pointing at the Execute Action node.
//
// Not checked: Blueprints that call the parent's Execute Action (the parent finishes).
// Finish Action called from anywhere in the Blueprint counts, since it's often called later,
// from a timer, a bound event or another function.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintCompilerExtension.h"
#include "ReactiveActionCompilerExtension.generated.h"

UCLASS()
class UReactiveActionCompilerExtension : public UBlueprintCompilerExtension
{
	GENERATED_BODY()

protected:
	virtual void ProcessBlueprintCompiled(const FKismetCompilerContext& CompilationContext, const FBlueprintCompiledData& Data) override;
};

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// The checks behind Map Check and Data Validation, in one place so both report the same problems.
// Each problem goes to a Report callback with a label saying where it is ("Reaction 2 (Event.Door.Open):
// action Play Sound: Sound isn't set") and whether it's an error (fails validation) or a warning.
// Editor only.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class AActor;
class FDataValidationContext;
class UReactiveObjectBase;
class UReactiveRequirement;
class UReactiveTrigger;
struct FReaction;
enum class EDataValidationResult : uint8;

namespace ReactiveValidation
{
	/** Where a problem goes. bError: fails validation; otherwise a warning. */
	using FReport = TFunctionRef<void(const FText& /*Message*/, bool /*bError*/)>;

	/** A block's own Validate Settings, each line prefixed with Label. */
	void CheckBlock(const UReactiveObjectBase* Block, const FText& Label, FReport Report);

	/** A list of requirements: empty slots, then each one's settings. */
	void CheckRequirements(const TArray<TObjectPtr<UReactiveRequirement>>& Requirements, const FText& Label, FReport Report);

	/**
	 * Reactions: event tags, empty slots, rules, and every action (nested ones too) with its requirements.
	 * Prefix goes before "Reaction N" ("Preset Lamp "). OwnerForAheadChecks (Map Check): also test each action's
	 * requirements against that actor where they can be checked ahead of time. Null skips that.
	 */
	void CheckReactions(const TArray<FReaction>& Reactions, const FText& Prefix, const AActor* OwnerForAheadChecks, FReport Report);

	/** Triggers: empty slots, each trigger's own checks and settings, its rules and conditions. */
	void CheckTriggers(const TArray<TObjectPtr<UReactiveTrigger>>& Triggers, const FText& Prefix, FReport Report);

	/**
	 * Runs Check against a Data Validation context: errors and warnings go to it, each prefixed with Where.
	 * Returns Invalid if any error was reported, otherwise Valid.
	 */
	EDataValidationResult Validate(FDataValidationContext& Context, const FText& Where, TFunctionRef<void(FReport)> Check);
}

#endif

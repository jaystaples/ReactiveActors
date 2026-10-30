// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveValidation.h"

#if WITH_EDITOR

#include "GameFramework/Actor.h"
#include "Misc/DataValidation.h"
#include "Reaction.h"
#include "ReactiveAction.h"
#include "ReactiveRequirement.h"
#include "ReactiveRule.h"
#include "ReactiveTrigger.h"

namespace ReactiveValidation
{
	static FText BlockName(const UObject* Block)
	{
		return Block->GetClass()->GetDisplayNameText();
	}

	void CheckBlock(const UReactiveObjectBase* Block, const FText& Label, FReport Report)
	{
		if (!Block)
		{
			return;
		}

		TArray<FText> Errors;
		TArray<FText> Warnings;
		Block->CollectSettingsProblems(Errors, Warnings);
		for (const FText& Error : Errors)
		{
			Report(FText::Format(INVTEXT("{0}: {1}"), Label, Error), /*bError*/ true);
		}
		for (const FText& Warning : Warnings)
		{
			Report(FText::Format(INVTEXT("{0}: {1}"), Label, Warning), /*bError*/ false);
		}
	}

	void CheckRequirements(const TArray<TObjectPtr<UReactiveRequirement>>& Requirements, const FText& Label, FReport Report)
	{
		for (int32 Index = 0; Index < Requirements.Num(); ++Index)
		{
			const UReactiveRequirement* Requirement = Requirements[Index];
			if (!Requirement)
			{
				Report(FText::Format(INVTEXT("{0}: requirement slot {1} is empty (no requirement selected)."), Label, Index), false);
				continue;
			}
			CheckBlock(Requirement, FText::Format(INVTEXT("{0}: requirement {1}"), Label, BlockName(Requirement)), Report);
		}
	}

	/** One action, then the actions nested inside it (Branch, Random Action, Run On Targets). */
	static void CheckAction(const UReactiveAction* Action, const FText& Label, const AActor* AheadOwner, FReport Report, int32 Depth = 0)
	{
		if (Depth > 16)
		{
			return; // a loop of nested actions shouldn't happen, but never recurse forever
		}

		const FText ActionLabel = FText::Format(INVTEXT("{0}: action {1}"), Label, BlockName(Action));
		CheckBlock(Action, ActionLabel, Report);
		CheckRequirements(Action->GetRequirements(), ActionLabel, Report);
		CheckRequirements(Action->GetExtraRequirements(), ActionLabel, Report);

		if (Action->StopsReactionIfNotMet() && Action->GetRequirements().IsEmpty() && Action->GetExtraRequirements().IsEmpty())
		{
			Report(FText::Format(INVTEXT("{0}: Stop Reaction If Not Met is on, but it has no requirements, so it never stops anything."), ActionLabel), false);
		}

		// Map Check only: requirements that can be tested now, against the actor this action will act on
		if (AheadOwner)
		{
			FReactionContext OwnerContext;
			OwnerContext.Target = const_cast<AActor*>(AheadOwner);
			for (const UReactiveRequirement* Requirement : Action->GetRequirements())
			{
				FText Reason;
				// Instigator checks can't be tested ahead of time (there's no instigator yet), nor can dice rolls
				if (Requirement && Requirement->CanCheckAhead() && !Requirement->Evaluate(OwnerContext, Reason))
				{
					Report(FText::Format(INVTEXT("{0}: {1}"), ActionLabel, Reason), false);
				}
			}
		}

		TArray<UReactiveAction*> Nested;
		Action->GetNestedActions(Nested);
		const AActor* NestedOwner = Action->NestedActionsKeepTarget() ? AheadOwner : nullptr;
		for (const UReactiveAction* Child : Nested)
		{
			if (!Child)
			{
				Report(FText::Format(INVTEXT("{0}: empty action slot."), ActionLabel), false);
				continue;
			}
			CheckAction(Child, ActionLabel, NestedOwner, Report, Depth + 1);
		}
	}

	void CheckReactions(const TArray<FReaction>& Reactions, const FText& Prefix, const AActor* OwnerForAheadChecks, FReport Report)
	{
		for (int32 Index = 0; Index < Reactions.Num(); ++Index)
		{
			const FReaction& Reaction = Reactions[Index];
			const FText Label = FText::Format(INVTEXT("{0}Reaction {1} ({2})"), Prefix, Index,
				FText::FromString(Reaction.EventTag.IsValid() ? Reaction.EventTag.ToString() : FString(TEXT("no event"))));

			if (!Reaction.bEnabled)
			{
				continue; // switched off on purpose: nothing it holds will run
			}
			if (!Reaction.EventTag.IsValid())
			{
				Report(FText::Format(INVTEXT("{0}: no Event Tag set, it will never fire (fine only if it's run with Run Reaction Preset)."), Label), false);
			}
			if (Reaction.Actions.IsEmpty())
			{
				Report(FText::Format(INVTEXT("{0}: has no actions, so it does nothing."), Label), false);
			}

			for (int32 RuleIndex = 0; RuleIndex < Reaction.Rules.Num(); ++RuleIndex)
			{
				const UReactiveRule* Rule = Reaction.Rules[RuleIndex];
				if (!Rule)
				{
					Report(FText::Format(INVTEXT("{0}: rule slot {1} is empty (no rule selected)."), Label, RuleIndex), false);
					continue;
				}
				CheckBlock(Rule, FText::Format(INVTEXT("{0}: rule {1}"), Label, BlockName(Rule)), Report);
			}

			for (const UReactiveAction* Action : Reaction.Actions)
			{
				if (!Action)
				{
					Report(FText::Format(INVTEXT("{0}: empty action slot."), Label), false);
					continue;
				}
				CheckAction(Action, Label, OwnerForAheadChecks, Report);
			}
		}
	}

	void CheckTriggers(const TArray<TObjectPtr<UReactiveTrigger>>& Triggers, const FText& Prefix, FReport Report)
	{
		for (int32 Index = 0; Index < Triggers.Num(); ++Index)
		{
			const UReactiveTrigger* Trigger = Triggers[Index];
			if (!Trigger)
			{
				Report(FText::Format(INVTEXT("{0}Trigger slot {1} is empty (no trigger selected)."), Prefix, Index), false);
				continue;
			}

			const FText Label = FText::Format(INVTEXT("{0}Trigger {1} ({2})"), Prefix, Index, BlockName(Trigger));

			// Its built-in checks (no event, Identity scope with no identity, empty slots...)
			Trigger->CheckForErrors([&Report, &Label](const FText& Message)
				{
					Report(FText::Format(INVTEXT("{0}: {1}"), Label, Message), false);
				});

			CheckBlock(Trigger, Label, Report);
			for (const UReactiveRule* Rule : Trigger->GetRules())
			{
				CheckBlock(Rule, FText::Format(INVTEXT("{0}: rule {1}"), Label, Rule ? BlockName(Rule) : FText::GetEmpty()), Report);
			}
			for (const UReactiveRequirement* Condition : Trigger->GetConditions())
			{
				CheckBlock(Condition, FText::Format(INVTEXT("{0}: condition {1}"), Label, Condition ? BlockName(Condition) : FText::GetEmpty()), Report);
			}
		}
	}

	EDataValidationResult Validate(FDataValidationContext& Context, const FText& Where, TFunctionRef<void(FReport)> Check)
	{
		bool bAnyError = false;
		Check([&Context, &Where, &bAnyError](const FText& Message, bool bError)
			{
				const FText Line = FText::Format(INVTEXT("{0}: {1}"), Where, Message);
				if (bError)
				{
					Context.AddError(Line);
					bAnyError = true;
				}
				else
				{
					Context.AddWarning(Line);
				}
			});
		return bAnyError ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
	}
}

#endif // WITH_EDITOR

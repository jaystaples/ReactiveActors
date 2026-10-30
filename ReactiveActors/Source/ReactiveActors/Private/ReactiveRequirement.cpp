// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveRequirement.h"
#include "ReactiveActorsStats.h"

bool UReactiveRequirement::Evaluate(const FReactionContext& Context, FText& OutFailReason) const
{
	SCOPE_CYCLE_COUNTER(STAT_ReactionsCheckRequirement);

	FText Reason;
	const bool bMet = CheckRequirement(Context, Reason);
	if (bMet != bInvert)
	{
		return true;
	}

	if (bInvert)
	{
		// The check passed, which Not turns into a failure: say what passed
		OutFailReason = FText::Format(INVTEXT("{0} passed (Not is on)."), FText::FromString(DescribeRequirement()));
	}
	else
	{
		OutFailReason = Reason.IsEmpty()
			? FText::Format(INVTEXT("{0} failed."), GetClass()->GetDisplayNameText())
			: Reason;
	}
	return false;
}

FString UReactiveRequirement::BuildSummary() const
{
	const FString Description = ScriptSummaryOr(DescribeRequirement());
	return bInvert ? TEXT("Not ") + Description : Description;
}

FString UReactiveRequirement::DescribeRequirement() const
{
	return GetClass()->GetDisplayNameText().ToString();
}

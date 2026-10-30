// Copyright 2026 Backyard Games Pty Ltd

#include "ReactionPreset.h"

#include "ReactiveTrigger.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "ReactiveValidation.h"
#include "ReactiveSaveData.h"

EDataValidationResult UReactionPreset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	const EDataValidationResult Ours = ReactiveValidation::Validate(Context, FText::FromString(GetName()),
		[this](ReactiveValidation::FReport Report)
		{
			ReactiveValidation::CheckReactions(Reactions, FText::GetEmpty(), /*OwnerForAheadChecks*/ nullptr, Report);
			ReactiveValidation::CheckTriggers(Triggers, FText::GetEmpty(), Report);
		});
	return CombineDataValidationResults(Result, Ours);
}

void UReactionPreset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Reactions and triggers added (or copied) get IDs, so saved games still match after they're reordered
	if (ReactiveSave::EnsureIds(Reactions, Triggers))
	{
		MarkPackageDirty();
	}
}
#endif

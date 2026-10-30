// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveActorsSettings.h"

#include "HAL/IConsoleManager.h"

UReactiveActorsSettings::UReactiveActorsSettings()
{
	SectionName = TEXT("Reactive Actors");
}

void UReactiveActorsSettings::ApplyToConsoleVariables() const
{
	// The variables only exist outside shipping builds; FindConsoleVariable returns null there
	auto SetFloat = [](const TCHAR* Name, float Value)
		{
			if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
			{
				Var->Set(Value, ECVF_SetByProjectSetting);
			}
		};

	SetFloat(TEXT("Reactions.Show.Distance"), ShowDistance);
	SetFloat(TEXT("Reactions.Show.Offset"), ShowOffset);
	SetFloat(TEXT("Reactions.Show.Linger"), LingerTime);
	SetFloat(TEXT("Reactions.Show.StuckTime"), StuckTime);
	SetFloat(TEXT("Reactions.Show.WiringTime"), WiringTime);

	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Reactions.Show.Rules")))
	{
		Var->Set(bShowRules ? 1 : 0, ECVF_SetByProjectSetting);
	}
}

#if WITH_EDITOR
void UReactiveActorsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyToConsoleVariables();
}
#endif

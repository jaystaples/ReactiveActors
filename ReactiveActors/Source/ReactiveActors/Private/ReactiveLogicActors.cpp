// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveLogicActors.h"
#include "ReactiveActorsLog.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ReactionSubsystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "Misc/DataValidation.h"
#include "Misc/UObjectToken.h"
#include "ReactiveValidation.h"
#endif

namespace ReactiveLogic
{
	static FString TagOrNone(const FGameplayTag& Tag)
	{
		return Tag.IsValid() ? Tag.ToString() : FString(TEXT("<none>"));
	}

	/** True if Sent is Listen or a child of it. */
	static bool Hears(const FGameplayTag& Sent, const FGameplayTag& Listen)
	{
		return UReactionSubsystem::EventMatches(Sent, Listen, /*bExactMatch*/ false);
	}
}

// =================================================================================================
// Base
// =================================================================================================

AReactiveLogicActor::AReactiveLogicActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

#if WITH_EDITORONLY_DATA
	// The engine's trigger icon for now, like the Reactive Volume
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite && !IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> TriggerTexture(TEXT("/Engine/EditorResources/S_Trigger"));
		Sprite->Sprite = TriggerTexture.Get();
		Sprite->SetupAttachment(Root);
		Sprite->bIsScreenSizeScaled = true;
		Sprite->SpriteInfo.Category = TEXT("Triggers");
		Sprite->SpriteInfo.DisplayName = INVTEXT("Triggers");
	}

	// What it does, above the icon, so invisible logic still reads in the level
	Label = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	if (Label)
	{
		Label->SetupAttachment(Root);
		Label->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextBottom);
		Label->SetWorldSize(14.f);
		Label->SetTextRenderColor(FColor(255, 150, 60));
		Label->SetHiddenInGame(true);
	}
#endif
}

void AReactiveLogicActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	if (Label)
	{
		Label->SetText(FText::FromString(DescribeLogic()));
	}
#endif
}

FString AReactiveLogicActor::DescribeLogic() const
{
	return GetClass()->GetDisplayNameText().ToString();
}

void AReactiveLogicActor::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			EventHandle = Subsystem->OnEventSent.AddUObject(this, &AReactiveLogicActor::HandleEventSent);
			ResetHandle = Subsystem->OnReset.AddUObject(this, &AReactiveLogicActor::HandleReset);
		}
	}
}

void AReactiveLogicActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UReactionSubsystem* Subsystem = World->GetSubsystem<UReactionSubsystem>())
		{
			Subsystem->OnEventSent.Remove(EventHandle);
			Subsystem->OnReset.Remove(ResetHandle);
		}
	}
	EventHandle.Reset();
	ResetHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void AReactiveLogicActor::HandleEventSent(const FReactiveEventInfo& Info)
{
	if (bSending)
	{
		return; // its own output (or something it set off) coming straight back: ignored, or it would loop
	}
	HandleEvent(Info);
}

void AReactiveLogicActor::HandleReset(const AActor* OnlyActor, const FGameplayTag& OnlyEvent)
{
	if (OnlyActor && OnlyActor != this)
	{
		return;
	}
	if (OnlyEvent.IsValid() && !(OutputEvent.IsValid() && OutputEvent.MatchesTag(OnlyEvent)) && !ListensFor(OnlyEvent))
	{
		return;
	}
	ResetLogic();
}

void AReactiveLogicActor::ResetLogic()
{
	OnResetLogic();
}

void AReactiveLogicActor::SendOutput(AActor* EventInstigator)
{
	if (!OutputEvent.IsValid() || bSending)
	{
		return;
	}

	UWorld* World = GetWorld();
	UReactionSubsystem* Subsystem = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	TGuardValue<bool> SendingGuard(bSending, true);

	// This actor is the source, so the debug wiring lines start from it
	if (Scope == EReactiveScopePolicy::Identity)
	{
		if (TargetIdentity.IsValid())
		{
			Subsystem->BroadcastEventToIdentity(TargetIdentity, OutputEvent, EventInstigator, this);
		}
		else
		{
			UE_LOG(LogReactiveActors, Warning, TEXT("%s: Identity scope but no Target Identity set."), *GetActorNameOrLabel());
		}
	}
	else
	{
		Subsystem->BroadcastEventFrom(OutputEvent, EventInstigator, this);
	}
}

#if WITH_EDITOR
void AReactiveLogicActor::GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const
{
	if (!OutputEvent.IsValid())
	{
		Report(INVTEXT("Send Event isn't set, so it sends nothing."), true);
	}
	if (Scope == EReactiveScopePolicy::Identity && !TargetIdentity.IsValid())
	{
		Report(INVTEXT("uses Identity scope but has no Target Identity, so its event goes nowhere."), true);
	}
}

void AReactiveLogicActor::CheckForErrors()
{
	Super::CheckForErrors();

	FMessageLog MapCheck("MapCheck");
	GatherProblems([this, &MapCheck](const FText& Message, bool bError)
		{
			TSharedRef<FTokenizedMessage> Line = bError ? MapCheck.Error() : MapCheck.Warning();
			Line->AddToken(FUObjectToken::Create(this));
			Line->AddToken(FTextToken::Create(Message));
		});
}

EDataValidationResult AReactiveLogicActor::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	const EDataValidationResult Ours = ReactiveValidation::Validate(Context, FText::FromString(GetActorNameOrLabel()),
		[this](ReactiveValidation::FReport Report) { GatherProblems(Report); });
	return CombineDataValidationResults(Result, Ours);
}
#endif

// =================================================================================================
// Event Relay
// =================================================================================================

void AReactiveEventRelay::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnResetLogic(); // clears pending sends
	Super::EndPlay(EndPlayReason);
}

bool AReactiveEventRelay::ListensFor(const FGameplayTag& Event) const
{
	return ListenFor.IsValid() && ListenFor.MatchesTag(Event);
}

void AReactiveEventRelay::HandleEvent(const FReactiveEventInfo& Info)
{
	if (!ReactiveLogic::Hears(Info.Event, ListenFor))
	{
		return;
	}
	if (MaxTimes > 0 && RelayCount >= MaxTimes)
	{
		return;
	}
	if (Chance < 100.f && FMath::FRand() * 100.f >= Chance)
	{
		return;
	}

	++RelayCount;
	TWeakObjectPtr<AActor> WeakInstigator = Info.Instigator.Get();

	if (Delay <= 0.f)
	{
		SendNow(WeakInstigator);
		return;
	}

	FTimerHandle& Handle = Pending.AddDefaulted_GetRef();
	GetWorldTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateUObject(this, &AReactiveEventRelay::SendNow, WeakInstigator), Delay, /*bLoop*/ false);
}

void AReactiveEventRelay::SendNow(TWeakObjectPtr<AActor> WeakInstigator)
{
	// Forget timers that have fired
	FTimerManager& Timers = GetWorldTimerManager();
	Pending.RemoveAll([&Timers](const FTimerHandle& Handle) { return !Timers.IsTimerActive(Handle); });

	SendOutput(WeakInstigator.Get());
}

void AReactiveEventRelay::OnResetLogic()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : Pending)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	Pending.Reset();
	RelayCount = 0;
}

FString AReactiveEventRelay::GetDebugStatus() const
{
	FString Out = MaxTimes > 0 ? FString::Printf(TEXT("relayed %d/%d"), RelayCount, MaxTimes) : FString::Printf(TEXT("relayed %d"), RelayCount);
	if (!Pending.IsEmpty())
	{
		Out += FString::Printf(TEXT(", %d waiting"), Pending.Num());
	}
	return Out;
}

FString AReactiveEventRelay::DescribeLogic() const
{
	FString Out = FString::Printf(TEXT("Relay: %s -> %s"), *ReactiveLogic::TagOrNone(ListenFor), *ReactiveLogic::TagOrNone(OutputEvent));
	TArray<FString> Extras;
	if (Delay > 0.f)
	{
		Extras.Add(FString::Printf(TEXT("after %.1fs"), Delay));
	}
	if (Chance < 100.f)
	{
		Extras.Add(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Chance)));
	}
	if (MaxTimes > 0)
	{
		Extras.Add(FString::Printf(TEXT("max %d"), MaxTimes));
	}
	if (!Extras.IsEmpty())
	{
		Out += FString::Printf(TEXT(" (%s)"), *FString::Join(Extras, TEXT(", ")));
	}
	return Out;
}

#if WITH_EDITOR
void AReactiveEventRelay::GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const
{
	Super::GatherProblems(Report);

	if (!ListenFor.IsValid())
	{
		Report(INVTEXT("When isn't set, so it never relays."), true);
	}
	else if (OutputEvent.IsValid() && OutputEvent.MatchesTag(ListenFor))
	{
		Report(INVTEXT("sends the event it listens for, so it would hear itself (ignored as a loop)."), true);
	}
	if (Chance <= 0.f)
	{
		Report(INVTEXT("Chance is 0, so it never relays."), false);
	}
}
#endif

// =================================================================================================
// Gate
// =================================================================================================

bool AReactiveEventGate::ListensFor(const FGameplayTag& Event) const
{
	return Needs.ContainsByPredicate([&Event](const FGameplayTag& Need) { return Need.IsValid() && Need.MatchesTag(Event); });
}

int32 AReactiveEventGate::CountHeard() const
{
	if (Order == EReactiveGateOrder::InOrder)
	{
		return NextInOrder;
	}
	int32 Count = 0;
	for (const bool bHeard : Heard)
	{
		Count += bHeard ? 1 : 0;
	}
	return Count;
}

void AReactiveEventGate::HandleEvent(const FReactiveEventInfo& Info)
{
	if (bDone || Needs.IsEmpty())
	{
		return;
	}

	bool bChanged = false;
	if (Order == EReactiveGateOrder::InOrder)
	{
		if (Needs.IsValidIndex(NextInOrder) && ReactiveLogic::Hears(Info.Event, Needs[NextInOrder]))
		{
			++NextInOrder;
			bChanged = true;
		}
		else if (bWrongOrderStartsAgain
			&& Needs.ContainsByPredicate([&Info](const FGameplayTag& Need) { return ReactiveLogic::Hears(Info.Event, Need); }))
		{
			// One of Needs, out of turn: start again (it may itself be the first one)
			NextInOrder = ReactiveLogic::Hears(Info.Event, Needs[0]) ? 1 : 0;
		}
	}
	else
	{
		Heard.SetNum(Needs.Num());
		for (int32 Index = 0; Index < Needs.Num(); ++Index)
		{
			if (!Heard[Index] && ReactiveLogic::Hears(Info.Event, Needs[Index]))
			{
				Heard[Index] = true;
				bChanged = true;
			}
		}
	}

	if (!bChanged || CountHeard() < Needs.Num())
	{
		return;
	}

	// Everything heard: open
	if (bRepeat)
	{
		Heard.Reset();
		NextInOrder = 0;
	}
	else
	{
		bDone = true;
	}
	SendOutput(Info.Instigator.Get());
}

void AReactiveEventGate::OnResetLogic()
{
	Heard.Reset();
	NextInOrder = 0;
	bDone = false;
}

FString AReactiveEventGate::GetDebugStatus() const
{
	if (bDone)
	{
		return TEXT("open (sent)");
	}
	return FString::Printf(TEXT("%d/%d heard"), CountHeard(), Needs.Num());
}

FString AReactiveEventGate::DescribeLogic() const
{
	TArray<FString> Names;
	for (const FGameplayTag& Need : Needs)
	{
		Names.Add(ReactiveLogic::TagOrNone(Need));
	}
	const TCHAR* Joiner = Order == EReactiveGateOrder::InOrder ? TEXT(" then ") : TEXT(" + ");
	return FString::Printf(TEXT("Gate: %s -> %s%s"), Names.IsEmpty() ? TEXT("<nothing>") : *FString::Join(Names, Joiner),
		*ReactiveLogic::TagOrNone(OutputEvent), bRepeat ? TEXT(" (repeats)") : TEXT(""));
}

#if WITH_EDITOR
void AReactiveEventGate::GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const
{
	Super::GatherProblems(Report);

	if (Needs.IsEmpty())
	{
		Report(INVTEXT("Needs is empty, so it never opens."), true);
	}
	for (int32 Index = 0; Index < Needs.Num(); ++Index)
	{
		if (!Needs[Index].IsValid())
		{
			Report(FText::Format(INVTEXT("Needs slot {0} is empty, so it can never be heard."), Index), true);
		}
		else if (OutputEvent.IsValid() && OutputEvent.MatchesTag(Needs[Index]))
		{
			Report(INVTEXT("sends one of the events it needs, so it would hear itself (ignored as a loop)."), false);
		}
	}
}
#endif

// =================================================================================================
// Counter
// =================================================================================================

bool AReactiveEventCounter::ListensFor(const FGameplayTag& Event) const
{
	return CountEvent.IsValid() && CountEvent.MatchesTag(Event);
}

void AReactiveEventCounter::HandleEvent(const FReactiveEventInfo& Info)
{
	if (bDone || !ReactiveLogic::Hears(Info.Event, CountEvent))
	{
		return;
	}

	++Count;
	const int32 SendAt = FMath::Max(Target, 1);
	const bool bReached = bRepeat ? (Count % SendAt == 0) : (Count >= SendAt);
	if (!bReached)
	{
		return;
	}

	if (!bRepeat)
	{
		bDone = true;
	}
	SendOutput(Info.Instigator.Get());
}

void AReactiveEventCounter::OnResetLogic()
{
	Count = 0;
	bDone = false;
}

FString AReactiveEventCounter::GetDebugStatus() const
{
	if (bDone)
	{
		return FString::Printf(TEXT("count %d (sent)"), Count);
	}
	const int32 SendAt = FMath::Max(Target, 1);
	return FString::Printf(TEXT("count %d/%d"), bRepeat ? Count % SendAt : Count, SendAt);
}

FString AReactiveEventCounter::DescribeLogic() const
{
	return FString::Printf(TEXT("Counter: %s x%d -> %s%s"), *ReactiveLogic::TagOrNone(CountEvent), FMath::Max(Target, 1),
		*ReactiveLogic::TagOrNone(OutputEvent), bRepeat ? TEXT(" (every time)") : TEXT(""));
}

#if WITH_EDITOR
void AReactiveEventCounter::GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const
{
	Super::GatherProblems(Report);

	if (!CountEvent.IsValid())
	{
		Report(INVTEXT("Count isn't set, so it never counts."), true);
	}
	else if (OutputEvent.IsValid() && OutputEvent.MatchesTag(CountEvent))
	{
		Report(INVTEXT("sends the event it counts, so it would hear itself (ignored as a loop)."), true);
	}
}
#endif

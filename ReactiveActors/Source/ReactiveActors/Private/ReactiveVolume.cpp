// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors

#include "ReactiveVolume.h"
#include "ReactiveActorsLog.h"

#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ReactionComponent.h"
#include "ReactionSubsystem.h"
#include "ReactiveRequirement.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "Misc/DataValidation.h"
#include "Misc/UObjectToken.h"
#include "ReactiveValidation.h"
#endif

AReactiveVolume::AReactiveVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Overlaps only, like a Trigger Box: nothing collides with it. Shapes are hidden in game by default.
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->InitBoxExtent(FVector(200.f, 200.f, 100.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
	Box->ShapeColor = FColor(255, 150, 60, 255);
	Box->SetHiddenInGame(true);
	RootComponent = Box;

#if WITH_EDITORONLY_DATA
	// The engine's trigger icon for now
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite && !IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> TriggerTexture(TEXT("/Engine/EditorResources/S_Trigger"));
		Sprite->Sprite = TriggerTexture.Get();
		Sprite->SetupAttachment(Box);
		Sprite->bIsScreenSizeScaled = true;
		Sprite->SpriteInfo.Category = TEXT("Triggers");
		Sprite->SpriteInfo.DisplayName = INVTEXT("Triggers");
	}
#endif

	SetCanBeDamaged(false);
}

void AReactiveVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (Box)
	{
		Box->SetBoxExtent(Size.ComponentMax(FVector(1.f)));
	}
}

bool AReactiveVolume::IsActorInside(const AActor* Actor) const
{
	// The set holds weak pointers to non-const actors; looking one up doesn't change it
	return Actor && Inside.Contains(TWeakObjectPtr<AActor>(const_cast<AActor*>(Actor)));
}

TArray<AActor*> AReactiveVolume::GetActorsInside() const
{
	TArray<AActor*> Actors;
	Actors.Reserve(Inside.Num());
	for (const TWeakObjectPtr<AActor>& Weak : Inside)
	{
		if (AActor* Actor = Weak.Get())
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

bool AReactiveVolume::IsActorInVolumeWithIdentity(const AActor* Actor, const FGameplayTag& Identity, bool& bOutFoundVolume)
{
	bOutFoundVolume = false;
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	if (!World || !Identity.IsValid())
	{
		return false;
	}

	for (TActorIterator<AReactiveVolume> It(World); It; ++It)
	{
		const AReactiveVolume* Volume = *It;
		// HasTag: a volume tagged Actor.Room.Kitchen counts for Actor.Room
		if (!Volume || !Volume->VolumeIdentityTags.HasTag(Identity))
		{
			continue;
		}
		bOutFoundVolume = true;
		if (Volume->IsActorInside(Actor))
		{
			return true;
		}
	}
	return false;
}

void AReactiveVolume::BeginPlay()
{
	Super::BeginPlay();

	// Next frame: actors beginning play now are listening by then, and initial overlaps are known
	CatchUpHandle = GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &AReactiveVolume::CatchUpExistingOverlaps));
}

void AReactiveVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CatchUpHandle);
	Entered.Reset();
	Inside.Reset();

	Super::EndPlay(EndPlayReason);
}

void AReactiveVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	HandleEnter(OtherActor);
}

void AReactiveVolume::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	HandleLeave(OtherActor);
}

void AReactiveVolume::CatchUpExistingOverlaps()
{
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		HandleEnter(Actor);
	}
}

void AReactiveVolume::HandleEnter(AActor* Actor)
{
	if (!HasActorBegunPlay() || !IsValid(Actor) || Actor == this || !HasTargetActorIdentity(Actor))
	{
		return; // not one of ours: no events, and never counted as inside
	}

	bool bAlreadyInside = false;
	Inside.Add(Actor, &bAlreadyInside);
	if (bAlreadyInside || !PassesConditions(Actor))
	{
		return;
	}

	Entered.Add(Actor);
	SendVolumeEvent(EnterEvent, Actor);
}

void AReactiveVolume::HandleLeave(AActor* Actor)
{
	if (!Actor || Inside.Remove(Actor) == 0)
	{
		return;
	}

	if (Entered.Remove(Actor) > 0)
	{
		SendVolumeEvent(LeaveEvent, Actor);
	}
}

bool AReactiveVolume::HasTargetActorIdentity(const AActor* Actor) const
{
	if (!TargetActorIdentity.IsValid())
	{
		return true;
	}

	TInlineComponentArray<UReactionComponent*> Components(Actor);
	for (const UReactionComponent* Component : Components)
	{
		// HasTag: an actor with Actor.Player.Main counts for Actor.Player
		if (Component && Component->GetIdentityTags().HasTag(TargetActorIdentity))
		{
			return true;
		}
	}
	return false;
}

bool AReactiveVolume::PassesConditions(AActor* Actor) const
{
	// The actor coming in is both Target and Instigator, so a check works whichever one it's set to look at
	FReactionContext Context;
	Context.Target = Actor;
	Context.Instigator = Actor;
	Context.Event = EnterEvent;

	for (const UReactiveRequirement* Condition : Conditions)
	{
		FText Reason;
		if (Condition && !Condition->Evaluate(Context, Reason))
		{
			UE_LOG(LogReactiveActors, Verbose, TEXT("%s: %s doesn't count: %s"),
				*GetActorNameOrLabel(), *Actor->GetActorNameOrLabel(), *Reason.ToString());
			return false;
		}
	}
	return true;
}

void AReactiveVolume::SendVolumeEvent(const FGameplayTag& Event, AActor* EventInstigator)
{
	if (!Event.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	UReactionSubsystem* Subsystem = World ? World->GetSubsystem<UReactionSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	// Hold Reactions Until Restored: send once the restore is in
	{
		TWeakObjectPtr<AReactiveVolume> WeakThis = this;
		TWeakObjectPtr<AActor> WeakInstigator = EventInstigator;
		const FGameplayTag HeldEvent = Event;
		if (Subsystem->DeferIfHolding([WeakThis, WeakInstigator, HeldEvent]()
			{
				if (AReactiveVolume* Self = WeakThis.Get())
				{
					Self->SendVolumeEvent(HeldEvent, WeakInstigator.Get());
				}
			}))
		{
			return;
		}
	}

	// This volume is the source, so the debug wiring lines start from it. Suspended events are dropped there.
	if (Scope == EReactiveScopePolicy::Identity)
	{
		if (TargetIdentity.IsValid())
		{
			Subsystem->BroadcastEventToIdentity(TargetIdentity, Event, EventInstigator, this);
		}
		else
		{
			UE_LOG(LogReactiveActors, Warning, TEXT("%s: Identity scope but no Target Identity set."), *GetActorNameOrLabel());
		}
	}
	else
	{
		Subsystem->BroadcastEventFrom(Event, EventInstigator, this);
	}
}

#if WITH_EDITOR
void AReactiveVolume::GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const
{
	if (VolumeIdentityTags.IsEmpty() && !EnterEvent.IsValid() && !LeaveEvent.IsValid())
	{
		Report(INVTEXT("has no events and no Volume Identity Tags, so it does nothing."), false);
	}
	if (Scope == EReactiveScopePolicy::Identity && !TargetIdentity.IsValid())
	{
		Report(INVTEXT("uses Identity scope but has no Target Identity, so its events go nowhere."), true);
	}
	ReactiveValidation::CheckRequirements(Conditions, INVTEXT("Conditions"), Report);
}

void AReactiveVolume::CheckForErrors()
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

EDataValidationResult AReactiveVolume::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	const EDataValidationResult Ours = ReactiveValidation::Validate(Context, FText::FromString(GetActorNameOrLabel()),
		[this](ReactiveValidation::FReport Report) { GatherProblems(Report); });
	return CombineDataValidationResults(Result, Ours);
}
#endif

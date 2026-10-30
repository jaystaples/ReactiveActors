// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors
//
// Placeable logic actors (Place Actors > Reactive). Invisible in game: an icon and a label in the editor.
// Each hears events wherever they're sent and sends its own Event to the Event Scope (World or an Identity),
// passing on the instigator of the event that set it off.
//
//   Reactive Event Relay   "when A, then B": hears one event and sends another, after an optional delay,
//                          with an optional chance, up to a number of times.
//   Reactive Gate          sends its event once it has heard a set of events, in any order or in order.
//                          "Read all three notes, then the door unlocks."
//   Reactive Counter       counts an event and sends its own when the count is reached (once, or every N).
//                          "The third time the fridge is opened."
//
// Reset All Reactions (and For Actor / For Event) resets the Gate and Counter (and the Relay's count).
// Reactions.Show 1 draws their state above them while playing.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ReactionContext.h"
#include "ReactiveLogicActors.generated.h"

class UBillboardComponent;
class UTextRenderComponent;
struct FReactiveEventInfo;

/** Shared by the logic actors: listening, sending, the editor icon and label, validation and resets. */
UCLASS(Abstract, NotBlueprintable, meta = (PrioritizeCategories = "Reactive"),
	HideCategories = (Rendering, Replication, Collision, HLOD, Physics, Networking, Input, LOD, Cooking))
class REACTIVEACTORS_API AReactiveLogicActor : public AActor
{
	GENERATED_BODY()

public:
	AReactiveLogicActor(const FObjectInitializer& ObjectInitializer);

	/** Puts it back as it was at the start: counts, what it has heard, pending sends. */
	UFUNCTION(BlueprintCallable, Category = "Reactive")
	void ResetLogic();

	/** One line about its current state, for the debug overlay ("2/3 heard"). */
	virtual FString GetDebugStatus() const { return FString(); }

	/** The editor label: what it does, in one line ("Relay: Event.A -> Event.B"). */
	virtual FString DescribeLogic() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	/** Problems with its settings, for Map Check and Data Validation. Subclasses add their own. */
	virtual void GatherProblems(TFunctionRef<void(const FText& /*Message*/, bool /*bError*/)> Report) const;
#endif

	/** An event was sent somewhere (not while events are suspended). */
	virtual void HandleEvent(const FReactiveEventInfo& Info) {}

	/** Clears this actor's own state (called by ResetLogic). */
	virtual void OnResetLogic() {}

	/** True if this actor listens for Event (for Reset ... For Event). */
	virtual bool ListensFor(const FGameplayTag& Event) const { return false; }

	/** Sends Event to Event Scope, from this actor. Refused while it's already sending (a loop). */
	void SendOutput(AActor* EventInstigator);

	/** The event it sends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (DisplayName = "Send Event", ReactiveTags = "Event", DisplayPriority = 10))
	FGameplayTag OutputEvent;

	/** World: everyone listening hears it. Identity: only actors with Target Identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (DisplayName = "Event Scope", InvalidEnumValues = "Self", DisplayPriority = 11))
	EReactiveScopePolicy Scope = EReactiveScopePolicy::World;

	/** Identity scope: send to actors with this identity, or a child of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive",
		meta = (ReactiveTags = "Identity", EditCondition = "Scope == EReactiveScopePolicy::Identity", EditConditionHides, DisplayPriority = 12))
	FGameplayTag TargetIdentity;

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	/** Editor icon, so it can be found and clicked. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;

	/** Editor label: what it does. */
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Label;
#endif

private:
	void HandleEventSent(const FReactiveEventInfo& Info);
	void HandleReset(const AActor* OnlyActor, const FGameplayTag& OnlyEvent);

	FDelegateHandle EventHandle;
	FDelegateHandle ResetHandle;

	/** True while sending: hearing its own output again would loop forever. */
	bool bSending = false;
};

// ---- Event Relay --------------------------------------------------------------------------------

/** "When A, then B": hears one event and sends another, after an optional delay and chance, up to a number of times. */
UCLASS(meta = (DisplayName = "Reactive Event Relay"))
class REACTIVEACTORS_API AReactiveEventRelay : public AReactiveLogicActor
{
	GENERATED_BODY()

public:
	virtual FString GetDebugStatus() const override;
	virtual FString DescribeLogic() const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleEvent(const FReactiveEventInfo& Info) override;
	virtual void OnResetLogic() override;
	virtual bool ListensFor(const FGameplayTag& Event) const override;

#if WITH_EDITOR
	virtual void GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const override;
#endif

	/** The event to hear (or a child of it), wherever it's sent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (DisplayName = "When", ReactiveTags = "Event", DisplayPriority = 0))
	FGameplayTag ListenFor;

	/** Seconds to wait before sending. 0: straight away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (ClampMin = 0, UIMin = 0, Units = "s", DisplayPriority = 1))
	float Delay = 0.f;

	/** How likely it is to send each time: 100 always. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (ClampMin = 0, ClampMax = 100, UIMin = 0, UIMax = 100, Units = "Percent", DisplayPriority = 2))
	float Chance = 100.f;

	/** Stops relaying after this many sends (Reset Reactions starts the count again). 0: no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (ClampMin = 0, UIMin = 0, DisplayName = "Max Times", DisplayPriority = 3))
	int32 MaxTimes = 0;

private:
	void SendNow(TWeakObjectPtr<AActor> WeakInstigator);

	/** Sends so far (including ones waiting on Delay). */
	int32 RelayCount = 0;

	/** Sends waiting on Delay. */
	TArray<FTimerHandle> Pending;
};

// ---- Gate ---------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EReactiveGateOrder : uint8
{
	/** Every event in Needs, in any order. */
	AnyOrder UMETA(DisplayName = "Any Order"),
	/** Every event in Needs, in the order listed. */
	InOrder UMETA(DisplayName = "In Order"),
};

/** Sends its event once it has heard every event in Needs: in any order, or in order. */
UCLASS(meta = (DisplayName = "Reactive Gate"))
class REACTIVEACTORS_API AReactiveEventGate : public AReactiveLogicActor
{
	GENERATED_BODY()

public:
	virtual FString GetDebugStatus() const override;
	virtual FString DescribeLogic() const override;

protected:
	virtual void HandleEvent(const FReactiveEventInfo& Info) override;
	virtual void OnResetLogic() override;
	virtual bool ListensFor(const FGameplayTag& Event) const override;

#if WITH_EDITOR
	virtual void GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const override;
#endif

	/** The events to hear (a child of one counts too). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (ReactiveTags = "Event", DisplayPriority = 0))
	TArray<FGameplayTag> Needs;

	/** Any Order, or In Order (the order listed). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (DisplayPriority = 1))
	EReactiveGateOrder Order = EReactiveGateOrder::AnyOrder;

	/** In Order: hearing one of Needs out of turn starts again from the first. Off: out-of-turn ones are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive",
		meta = (EditCondition = "Order == EReactiveGateOrder::InOrder", EditConditionHides, DisplayPriority = 2))
	bool bWrongOrderStartsAgain = true;

	/** After sending, start listening again from scratch. Off: it sends once (until Reset Reactions). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (DisplayPriority = 3))
	bool bRepeat = false;

private:
	/** Any Order: which of Needs have been heard. */
	TArray<bool> Heard;

	/** In Order: how many of Needs have been heard in turn. */
	int32 NextInOrder = 0;

	/** Sent, and not repeating: ignores everything until reset. */
	bool bDone = false;

	int32 CountHeard() const;
};

// ---- Counter ------------------------------------------------------------------------------------

/** Counts an event and sends its own when the count is reached: once, or every time it's reached again. */
UCLASS(meta = (DisplayName = "Reactive Counter"))
class REACTIVEACTORS_API AReactiveEventCounter : public AReactiveLogicActor
{
	GENERATED_BODY()

public:
	virtual FString GetDebugStatus() const override;
	virtual FString DescribeLogic() const override;

protected:
	virtual void HandleEvent(const FReactiveEventInfo& Info) override;
	virtual void OnResetLogic() override;
	virtual bool ListensFor(const FGameplayTag& Event) const override;

#if WITH_EDITOR
	virtual void GatherProblems(TFunctionRef<void(const FText&, bool)> Report) const override;
#endif

	/** The event to count (a child of it counts too). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (DisplayName = "Count", ReactiveTags = "Event", DisplayPriority = 0))
	FGameplayTag CountEvent;

	/** Sends when the count reaches this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (DisplayName = "Send At", ClampMin = 1, UIMin = 1, DisplayPriority = 1))
	int32 Target = 3;

	/** Send every time the count reaches another multiple (every 3rd). Off: once (until Reset Reactions). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactive", meta = (DisplayName = "Every Time", DisplayPriority = 2))
	bool bRepeat = false;

private:
	int32 Count = 0;
	bool bDone = false;
};

// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions
//
// Presentation: sound, effects, sequences and messages. Play Level Sequence, Play Sound, Log Message.
// Spawn VFX lives in the Reactive Actors Extras plugin (it needs Niagara).
// NotBlueprintable keeps them out of the "pick parent class" list; they still appear in every Actions dropdown.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "ReactiveAction.h"
#include "PresentationActions.generated.h"

class ALevelSequenceActor;
class UAudioComponent;
class ULevelSequence;
class ULevelSequencePlayer;
class USoundAttenuation;
class USoundBase;

// ---- Play Level Sequence ------------------------------------------------------------------------

/**
 * Plays a Level Sequence. By default the reaction waits for it to finish before the next action;
 * untick Wait For Sequence To Finish to let the next actions run while it plays.
 * Don't wait on a looping sequence: it never finishes.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Play Level Sequence"))
class REACTIVEACTORS_API UACT_PlayLevelSequence : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;
	virtual void OnCleanup_Implementation() override;
	virtual void OnCancel_Implementation() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	TObjectPtr<ULevelSequence> Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0.01, UIMin = 0.01))
	float PlayRate = 1.f;

	/** On: the reaction waits until the sequence ends. Off: the next action runs straight away while it plays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bWaitForSequenceToFinish = true;

private:
	UFUNCTION()
	void HandleSequenceFinished();

	void ReleasePlayer(bool bDestroyActor);

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> Player;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;
};

// ---- Play Sound ---------------------------------------------------------------------------------

/** Where Play Sound plays. */
UENUM(BlueprintType)
enum class EReactiveSoundMode : uint8
{
	/** Not in the world: UI, music, a voice in the player's head. */
	TwoD UMETA(DisplayName = "2D"),
	/** At the target's location, left there. */
	AtTarget UMETA(DisplayName = "At Target"),
	/** Attached to the target (or one of its sockets), so it follows it. Stops if the target is destroyed. */
	AttachedToTarget UMETA(DisplayName = "Attached to Target"),
};

/**
 * Plays a sound: 2D, at the target, or attached to it. Carries on straight away, or with Wait Until
 * Finished, when the sound ends (cancelling the reaction then stops the sound).
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Play Sound"))
class REACTIVEACTORS_API UACT_PlaySound : public UReactiveAction
{
	GENERATED_BODY()

public:
	virtual void ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const override;

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;
	virtual void OnCancel_Implementation() override;
	virtual void OnCleanup_Implementation() override;

	/** The sound (a Sound Wave, Sound Cue or MetaSound). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	EReactiveSoundMode Mode = EReactiveSoundMode::AtTarget;

	/** Attached to Target: the socket or bone to attach to. None = the target's root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, EditCondition = "Mode == EReactiveSoundMode::AttachedToTarget", EditConditionHides))
	FName Socket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0, UIMin = 0))
	float Volume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, ClampMin = 0.01, UIMin = 0.01))
	float Pitch = 1.f;

	/** How it fades with distance. Empty = the sound's own settings. Not used for 2D. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, EditCondition = "Mode != EReactiveSoundMode::TwoD", EditConditionHides))
	TObjectPtr<USoundAttenuation> Attenuation;

	/**
	 * On: the reaction waits here until the sound ends, and cancelling the reaction stops it.
	 * Off: carries on straight away. Looping sounds never end, so they always carry on straight away.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bWaitUntilFinished = false;

private:
	void HandleSoundFinished(UAudioComponent* Component);
	/** Unbinds; with bStopSound, also stops the sound it was waiting on. */
	void StopWaiting(bool bStopSound);

	TWeakObjectPtr<UAudioComponent> Playing;
};

// ---- Log Message --------------------------------------------------------------------------------

/** Prints a message to the screen and/or log. Stripped from shipping builds. For debugging reactions. */
UCLASS(NotBlueprintable, meta = (DisplayName = "Log Message"))
class REACTIVEACTORS_API UACT_LogMessage : public UReactiveAction
{
	GENERATED_BODY()

protected:
	virtual void OnExecuteAction_Implementation(const FReactionContext& Context) override;
	virtual FString DescribeAction() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, MultiLine = true))
	FString Message = TEXT("Action fired");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bPrintToScreen = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn))
	bool bPrintToLog = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, EditCondition = "bPrintToScreen"))
	FLinearColor Color = FLinearColor(0.f, 0.66f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ExposeOnSpawn, EditCondition = "bPrintToScreen", ClampMin = 0, Units = "s"))
	float Duration = 3.f;
};

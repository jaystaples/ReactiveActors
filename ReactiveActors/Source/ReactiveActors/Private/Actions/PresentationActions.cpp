// Copyright 2026 Backyard Games Pty Ltd
// Reactive Actors - built-in actions: presentation

#include "Actions/PresentationActions.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "ReactiveActorsLog.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

// ---- Play Level Sequence ------------------------------------------------------------------------

void UACT_PlayLevelSequence::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	AActor* Target = Context.Target; // never null here
	UWorld* World = GetWorld();
	if (!Sequence || !World)
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: no Sequence set."), *GetClass()->GetName(), *GetNameSafe(Target));
		FinishAction();
		return;
	}

	// Replay: clear the previous run's player first
	ReleasePlayer(/*bDestroyActor*/ true);

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.PlayRate = PlayRate;

	ALevelSequenceActor* OutActor = nullptr;
	Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, Settings, OutActor);
	SequenceActor = OutActor;

	if (!Player)
	{
		FinishAction();
		return;
	}

	Player->OnFinished.AddDynamic(this, &UACT_PlayLevelSequence::HandleSequenceFinished);
	Player->Play();

	// Not waiting: finish now so the reaction carries on while the sequence plays.
	// (HandleSequenceFinished's later FinishAction call is then a harmless no-op.)
	if (!bWaitForSequenceToFinish)
	{
		FinishAction();
	}
}

void UACT_PlayLevelSequence::HandleSequenceFinished()
{
	// Keep the sequence actor so the sequence's final state stays applied
	ReleasePlayer(/*bDestroyActor*/ false);
	FinishAction();
}

void UACT_PlayLevelSequence::OnCleanup_Implementation()
{
	ReleasePlayer(/*bDestroyActor*/ true);
}

void UACT_PlayLevelSequence::OnCancel_Implementation()
{
	// Stop the sequence that's playing (only reached while waiting for it to finish)
	ReleasePlayer(/*bDestroyActor*/ true);
}

void UACT_PlayLevelSequence::ReleasePlayer(bool bDestroyActor)
{
	if (Player)
	{
		Player->OnFinished.RemoveDynamic(this, &UACT_PlayLevelSequence::HandleSequenceFinished);
		if (bDestroyActor)
		{
			Player->Stop();
		}
		Player = nullptr;
	}

	if (bDestroyActor && SequenceActor)
	{
		SequenceActor->Destroy();
		SequenceActor = nullptr;
	}
}

// ---- Play Sound ---------------------------------------------------------------------------------

void UACT_PlaySound::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	AActor* Target = Context.Target;
	if (!Sound || (Mode != EReactiveSoundMode::TwoD && !Target))
	{
		UE_LOG(LogReactiveActors, Warning, TEXT("%s on %s: %s, nothing played."), *GetClass()->GetName(), *GetNameSafe(Target),
			!Sound ? TEXT("no Sound set") : TEXT("no target"));
		FinishAction();
		return;
	}

	UAudioComponent* Component = nullptr;
	switch (Mode)
	{
	case EReactiveSoundMode::TwoD:
		Component = UGameplayStatics::SpawnSound2D(this, Sound, Volume, Pitch);
		break;

	case EReactiveSoundMode::AtTarget:
		Component = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Target->GetActorLocation(), Target->GetActorRotation(),
			Volume, Pitch, /*StartTime*/ 0.f, Attenuation);
		break;

	case EReactiveSoundMode::AttachedToTarget:
		Component = UGameplayStatics::SpawnSoundAttached(Sound, Target->GetRootComponent(), Socket, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed*/ true, Volume, Pitch, /*StartTime*/ 0.f, Attenuation);
		break;
	}

	// No component (audio off, a dedicated server) or a looping sound: nothing to wait for
	if (!bWaitUntilFinished || !Component || Sound->IsLooping())
	{
		FinishAction();
		return;
	}

	Playing = Component;
	Component->OnAudioFinishedNative.AddUObject(this, &UACT_PlaySound::HandleSoundFinished);
}

void UACT_PlaySound::HandleSoundFinished(UAudioComponent* Component)
{
	if (Component != Playing.Get() || !IsRunning())
	{
		return;
	}
	StopWaiting(/*bStopSound*/ false);
	FinishAction();
}

void UACT_PlaySound::StopWaiting(bool bStopSound)
{
	if (UAudioComponent* Component = Playing.Get())
	{
		Component->OnAudioFinishedNative.RemoveAll(this);
		if (bStopSound)
		{
			Component->Stop();
		}
	}
	Playing.Reset();
}

void UACT_PlaySound::OnCancel_Implementation()
{
	StopWaiting(/*bStopSound*/ true);
}

void UACT_PlaySound::OnCleanup_Implementation()
{
	StopWaiting(/*bStopSound*/ true);
}

FString UACT_PlaySound::DescribeAction() const
{
	const TCHAR* Where = Mode == EReactiveSoundMode::TwoD ? TEXT("2D")
		: Mode == EReactiveSoundMode::AttachedToTarget ? TEXT("attached") : TEXT("at target");
	return FString::Printf(TEXT("Play Sound %s (%s)%s"), Sound ? *Sound->GetName() : TEXT("<no sound>"), Where,
		bWaitUntilFinished ? TEXT(", wait") : TEXT(""));
}

// ---- Log Message --------------------------------------------------------------------------------

void UACT_LogMessage::OnExecuteAction_Implementation(const FReactionContext& Context)
{
	UKismetSystemLibrary::PrintString(this, Message, bPrintToScreen, bPrintToLog, Color, Duration);
	FinishAction();
}

// ---- Summaries (the row titles in the editor) ---------------------------------------------------

FString UACT_PlayLevelSequence::DescribeAction() const
{
	return FString::Printf(TEXT("Play %s%s"), Sequence ? *Sequence->GetName() : TEXT("<no sequence>"),
		bWaitForSequenceToFinish ? TEXT("") : TEXT(" (don't wait)"));
}

FString UACT_LogMessage::DescribeAction() const
{
	return FString::Printf(TEXT("Log \"%s\""), *Message.Left(40).Replace(TEXT("\n"), TEXT(" ")));
}

// ---- Validate Settings (Data Validation and Map Check) ----------------------------------------

void UACT_PlaySound::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Sound)
	{
		Errors.Add(INVTEXT("Sound isn't set, so it plays nothing."));
	}
}

void UACT_PlayLevelSequence::ValidateSettings_Implementation(TArray<FText>& Errors, TArray<FText>& Warnings) const
{
	if (!Sequence)
	{
		Errors.Add(INVTEXT("Sequence isn't set, so it plays nothing."));
	}
}

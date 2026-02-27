// ============================================================================
//
//  Yarn Spinner for Unreal Engine
//
//  Copyright (c) Yarn Spinner Pty. Ltd. All Rights Reserved.
//
//  Yarn Spinner is a trademark of Secret Lab Pty. Ltd., used under license.
//
//  This code is subject to the terms and conditions of the license found in
//  the LICENSE.md file in the root of this repository.
//
//  For help, support, and more information, visit:
//    https://yarnspinner.dev
//    https://docs.yarnspinner.dev
//
// ============================================================================

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// our own header - must be included first for unreal header tool
#include "YarnVoiceOverPresenter.h"

// we need the dialogue runner to access the line provider and cancellation tokens.
// presenters communicate with the runner to signal when they're done.
#include "YarnDialogueRunner.h"

// logging macros (UE_LOG with LogYarnSpinner category)
#include "YarnSpinnerModule.h"

// FTimerManager for SetTimer/ClearTimer - we use timers for pre/post delays
// and to defer operations to avoid recursion.
#include "TimerManager.h"

// UWorld - needed to access GetTimerManager() since timer manager is per-world.
// also used to check if the world is valid before setting timers.
#include "Engine/World.h"

// AActor - GetOwner() returns an AActor, we need this for the audio component
// attachment and to find existing audio components on the owning actor.
#include "GameFramework/Actor.h"

// ----------------------------------------------------------------------------
// constructor
// ----------------------------------------------------------------------------

UYarnVoiceOverPresenter::UYarnVoiceOverPresenter()
{
	// we need ticking for fade-out and playback completion detection, but we
	// don't want it running all the time - only when we're actually playing
	// audio or fading out. so we enable tick capability but start disabled.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ----------------------------------------------------------------------------
// lifecycle
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::BeginPlay()
{
	Super::BeginPlay();

	// make sure we have an audio component ready to go before dialogue starts.
	// this avoids any hitches when the first line comes through.
	EnsureAudioComponent();
}

void UYarnVoiceOverPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// this is critical for preventing crashes. if we have timers pending when
	// the component is destroyed (level transition, actor destruction, etc),
	// those timers would fire and try to call methods on a dead object.
	// we clear them here to be safe.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreStartTimerHandle);
		World->GetTimerManager().ClearTimer(PostCompleteTimerHandle);
	}

	// stop any audio that's currently playing. we don't fade out here because
	// the component is being destroyed anyway - just cut it immediately.
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
	}

	// reset state flags so if this component somehow gets reused (pooling),
	// it starts fresh.
	bIsPlaying = false;
	bIsFadingOut = false;

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------------
// tick - used for fade-out animation and detecting playback completion
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// handle the fade-out animation when audio is interrupted. we gradually
	// reduce the volume over FadeOutTimeOnInterrupt seconds to avoid an
	// abrupt audio cut which sounds jarring.
	if (bIsFadingOut && AudioComponent)
	{
		if (FadeOutTimeOnInterrupt > 0.0f)
		{
			// advance the fade progress based on elapsed time
			FadeOutProgress += DeltaTime / FadeOutTimeOnInterrupt;

			if (FadeOutProgress >= 1.0f)
			{
				// fade complete - clamp to 1, restore original volume setting
				// (so it's correct for the next clip), stop playback, and
				// disable ticking since we're done.
				FadeOutProgress = 1.0f;
				bIsFadingOut = false;

				AudioComponent->SetVolumeMultiplier(OriginalVolume);
				AudioComponent->Stop();

				SetComponentTickEnabled(false);
				CompleteLine();
			}
			else
			{
				// still fading - interpolate volume from original down to zero.
				// this gives us a smooth fade-out curve.
				float NewVolume = FMath::Lerp(OriginalVolume, 0.0f, FadeOutProgress);
				AudioComponent->SetVolumeMultiplier(NewVolume);
			}
		}
		else
		{
			// fade time is zero, so just cut immediately. this is useful when
			// you want instant interruption without any fade.
			bIsFadingOut = false;
			AudioComponent->Stop();
			SetComponentTickEnabled(false);
			CompleteLine();
		}
	}
	// check if audio playback finished naturally (not interrupted). unreal's
	// audio component doesn't give us a reliable callback for this, so we
	// poll it each tick while we're playing.
	else if (bIsPlaying && AudioComponent && !AudioComponent->IsPlaying())
	{
		OnAudioFinished();
	}
}

// ----------------------------------------------------------------------------
// dialogue presenter interface implementation
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::OnDialogueStarted_Implementation()
{
	// nothing to do here - we set up the audio component in BeginPlay and
	// actual playback starts when RunLine is called. subclasses can override
	// this if they need to do setup when dialogue begins.
}

void UYarnVoiceOverPresenter::OnDialogueComplete_Implementation()
{
	// dialogue has ended, so clean up any audio that might still be playing.
	// this handles the case where dialogue is stopped mid-line.
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
	}

	// reset all state
	bIsPlaying = false;
	bIsFadingOut = false;
	SetComponentTickEnabled(false);

	// clear any pending timers. this is important because if we had a
	// pre-start delay timer pending, we don't want it to fire after
	// dialogue has ended.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PreStartTimerHandle);
		World->GetTimerManager().ClearTimer(PostCompleteTimerHandle);
	}
}

void UYarnVoiceOverPresenter::RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry)
{
	// store the current line so we can reference it later (for logging, etc)
	CurrentLine = Line;

	// make sure we have a valid audio component before trying to play anything
	EnsureAudioComponent();

	// if there's already audio playing from a previous line, stop it. this
	// shouldn't normally happen if the dialogue flow is working correctly,
	// but it's a safety measure.
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->Stop();
	}

	// get the audio clip for this line. this calls the virtual function which
	// can be overridden in blueprints or c++ subclasses to provide custom
	// audio lookup logic.
	USoundBase* VoiceOverClip = GetVoiceOverClip(Line);

	if (!VoiceOverClip)
	{
		// no audio for this line - that's fine, not all lines need voice over.
		// log a warning so developers know, then optionally advance to the
		// next line if configured to do so.
		UE_LOG(LogYarnSpinner, Warning, TEXT("VoiceOverPresenter: No audio clip for line '%s'"), *Line.RawLine.LineID);

		if (bEndLineWhenVoiceOverComplete)
		{
			OnLinePresentationComplete();
		}
		return;
	}

	// start playback, optionally with a delay. the delay is useful for
	// synchronising with text presenters or other effects.
	if (WaitTimeBeforeStart > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			// set up a timer to start playback after the delay. we use
			// CreateUObject to bind the audio clip parameter.
			World->GetTimerManager().SetTimer(
				PreStartTimerHandle,
				FTimerDelegate::CreateUObject(this, &UYarnVoiceOverPresenter::StartPlayback, VoiceOverClip),
				WaitTimeBeforeStart,
				false  // don't loop
			);
		}
	}
	else
	{
		// no delay - start immediately
		StartPlayback(VoiceOverClip);
	}
}

// ----------------------------------------------------------------------------
// audio clip lookup
// ----------------------------------------------------------------------------

USoundBase* UYarnVoiceOverPresenter::GetVoiceOverClip_Implementation(const FYarnLocalizedLine& Line)
{
	// this is the default implementation which looks for an "audio:" metadata
	// tag on the line. override this in your subclass or blueprint to provide
	// your own audio lookup logic - for example, looking up clips in a data
	// table based on line id, or constructing asset paths from a naming
	// convention.

	// check if the line has a metadata tag specifying the audio asset path.
	// metadata tags come from yarn script comments like: #audio:/Game/Audio/Line01
	for (const FString& Tag : Line.Metadata)
	{
		if (Tag.StartsWith(TEXT("audio:")))
		{
			// extract the path after "audio:" prefix
			FString AssetPath = Tag.Mid(6);  // 6 = length of "audio:"

			// use StaticLoadObject to synchronously load the asset. this is
			// fine for audio clips which are typically small. if you have
			// performance concerns, consider async loading in your override.
			return Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *AssetPath));
		}
	}

	// no audio tag found - return nullptr to indicate no audio for this line
	return nullptr;
}

// ----------------------------------------------------------------------------
// audio component management
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::EnsureAudioComponent()
{
	// if we already have an audio component, nothing to do
	if (!AudioComponent)
	{
		// first, check if our owning actor already has an audio component we
		// can use. this allows designers to set up the audio component in the
		// editor with specific settings (attenuation, etc).
		if (AActor* Owner = GetOwner())
		{
			AudioComponent = Owner->FindComponentByClass<UAudioComponent>();
		}

		// if we still don't have one, create our own. this is marked as
		// transient so it doesn't get saved with the actor.
		if (!AudioComponent && GetOwner())
		{
			AudioComponent = NewObject<UAudioComponent>(GetOwner(), NAME_None, RF_Transient);

			// configure for dialogue playback - we don't want it to auto-play
			// or auto-destroy, and we default to 2d (non-spatialised) audio
			// since dialogue is usually played at full volume regardless of
			// listener position.
			AudioComponent->bAutoActivate = false;
			AudioComponent->bAutoDestroy = false;
			AudioComponent->bAllowSpatialization = false;

			// register and attach to the actor so it moves with them
			AudioComponent->RegisterComponent();
			AudioComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}

// ----------------------------------------------------------------------------
// playback control
// ----------------------------------------------------------------------------

void UYarnVoiceOverPresenter::StartPlayback(USoundBase* AudioClip)
{
	// safety check - if we don't have what we need, just complete the line
	if (!AudioComponent || !AudioClip)
	{
		CompleteLine();
		return;
	}

	// set the sound and start playing
	AudioComponent->SetSound(AudioClip);
	AudioComponent->Play();

	// update state - we're now playing, not fading
	bIsPlaying = true;
	bIsFadingOut = false;

	// enable ticking so we can detect when playback finishes
	SetComponentTickEnabled(true);

	// notify listeners that playback has started
	OnVoiceOverStarted.Broadcast();

	UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverPresenter: Playing audio for line '%s'"), *CurrentLine.RawLine.LineID);
}

void UYarnVoiceOverPresenter::OnAudioFinished()
{
	// audio finished playing naturally (not interrupted)
	bIsPlaying = false;
	SetComponentTickEnabled(false);

	UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverPresenter: Audio finished for line '%s'"), *CurrentLine.RawLine.LineID);

	// apply a post-complete delay if configured. this gives a natural pause
	// after the voice over finishes before moving to the next line.
	if (WaitTimeAfterComplete > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				PostCompleteTimerHandle,
				this,
				&UYarnVoiceOverPresenter::CompleteLine,
				WaitTimeAfterComplete,
				false  // don't loop
			);
		}
	}
	else
	{
		// no delay - complete immediately
		CompleteLine();
	}
}

void UYarnVoiceOverPresenter::CompleteLine()
{
	// notify listeners that voice over is complete (whether it finished
	// naturally or was interrupted and faded out)
	OnVoiceOverComplete.Broadcast();

	// if configured to control line advancement, tell the dialogue runner
	// we're done. otherwise, let another presenter or manual input control
	// when to advance.
	if (bEndLineWhenVoiceOverComplete)
	{
		OnLinePresentationComplete();
	}
}

void UYarnVoiceOverPresenter::BeginFadeOut()
{
	// called when the current line is interrupted (e.g., player skipped ahead).
	// instead of abruptly cutting the audio, we fade it out smoothly.

	if (!AudioComponent)
	{
		// no audio component means nothing to fade - just complete
		CompleteLine();
		return;
	}

	// store the current volume so we can restore it after the fade (for the
	// next clip) and as the starting point for the fade interpolation.
	OriginalVolume = AudioComponent->VolumeMultiplier;

	// reset progress and start the fade
	FadeOutProgress = 0.0f;
	bIsFadingOut = true;

	// enable ticking so the fade animation runs
	SetComponentTickEnabled(true);

	UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverPresenter: Beginning fade-out for interrupted line"));
}

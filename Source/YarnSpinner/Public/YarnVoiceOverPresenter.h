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

#pragma once

#include "CoreMinimal.h"
#include "YarnDialoguePresenter.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "YarnVoiceOverPresenter.generated.h"

// ============================================================================
// Voice-over presenter - plays audio clips for dialogue lines
// ============================================================================
//
// This is an ActorComponent that handles playing voice-over audio clips
// synchronised with dialogue lines.
//
// BLUEPRINT VS C++ USAGE:
// - All timing properties are editable in blueprints and the editor.
// - GetVoiceOverClip is a BlueprintNativeEvent - override it in blueprints
//   to provide custom audio lookup logic without any C++.
// - Events (OnVoiceOverStarted, OnVoiceOverComplete) are BlueprintAssignable
//   so you can bind to them in blueprints via the event graph.
// - The component is BlueprintSpawnable so you can add it dynamically.
// - For C++ subclasses, override GetVoiceOverClip_Implementation.
//
// AUDIO COMPONENT:
// - If you assign an AudioComponent in the editor, that one is used.
// - If not, one is created automatically in BeginPlay.
// - The auto-created component is 2D (non-spatialised) by default since
//   dialogue audio usually shouldn't be affected by 3D position.
// - If you want spatialised dialogue, set up your own AudioComponent with
//   the desired attenuation settings and assign it.
//
// TIMING:
// - WaitTimeBeforeStart: delay before audio starts (good for syncing with
//   text fade-in or character animation).
// - WaitTimeAfterComplete: delay after audio ends before signalling done
//   (gives a natural pause between lines).
// - FadeOutTimeOnInterrupt: smooth fade when interrupted (avoids audio pop).
//
// USAGE:
// - Use this presenter alongside a text presenter (like UYarnWidgetPresenter).
// - Add both to the dialogue runner's presenter list.
// - They'll both receive lines and handle them in parallel.
// - Typically you want the text presenter to control line advancement, so set
//   bEndLineWhenVoiceOverComplete = false on this presenter.

/** Simple delegate for voice-over presenter events (no parameters). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FYarnVoiceOverEvent);

/**
 * A dialogue presenter that plays voice-over audio clips for dialogue lines.
 *
 * Handles audio playback with support for pre/post delays, smooth fade-out on
 * interruption, and optional automatic line advancement when audio completes.
 *
 * Override GetVoiceOverClip in blueprints or C++ to provide custom audio
 * lookup logic. The default implementation looks for an "audio:" metadata
 * tag on the line.
 *
 * @see UYarnDialoguePresenter the base presenter class
 * @see AYarnVoiceOverDemo a complete demo actor using this presenter
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable)
class YARNSPINNER_API UYarnVoiceOverPresenter : public UYarnDialoguePresenter
{
	GENERATED_BODY()

public:
	UYarnVoiceOverPresenter();

	// ========================================================================
	// UActorComponent interface
	// ========================================================================

	virtual void BeginPlay() override;

	/** Clears timers and stops audio to prevent callbacks on destroyed object. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Handles fade-out animation and playback completion detection. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================================================
	// UYarnDialoguePresenter interface
	// ========================================================================

	virtual void OnDialogueStarted_Implementation() override;
	virtual void OnDialogueComplete_Implementation() override;
	virtual void RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry) override;

	// ========================================================================
	// configuration - all editable in editor and blueprints
	// ========================================================================

	/**
	 * If true, this presenter will request the next line when voice-over
	 * playback completes. Set to false if you want another presenter (like
	 * a text presenter) or player input to control line advancement.
	 *
	 * Typically you'd have this false and let the text presenter handle it,
	 * since players might want to read at their own pace regardless of audio.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over")
	bool bEndLineWhenVoiceOverComplete = true;

	/**
	 * Time in seconds to fade out audio when interrupted (player skips ahead).
	 * Prevents a jarring audio cut. Set to 0 for instant stop.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over|Timing", meta = (ClampMin = "0.0"))
	float FadeOutTimeOnInterrupt = 0.05f;

	/**
	 * Time in seconds to wait before starting audio playback after receiving
	 * a line. Useful for syncing with text fade-in animations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over|Timing", meta = (ClampMin = "0.0"))
	float WaitTimeBeforeStart = 0.0f;

	/**
	 * Time in seconds to wait after audio completes before signalling done.
	 * Gives a natural pause between lines. Not applied when interrupted.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over|Timing", meta = (ClampMin = "0.0"))
	float WaitTimeAfterComplete = 0.0f;

	/**
	 * The audio component to use for playback. If not assigned, one will be
	 * created automatically in BeginPlay.
	 *
	 * If you want spatialised audio (3D positioning, attenuation), set up
	 * your own AudioComponent with the desired settings and assign it here.
	 * The auto-created component is 2D (non-spatialised) by default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over")
	UAudioComponent* AudioComponent;

	// ========================================================================
	// audio lookup - override this for custom audio resolution
	// ========================================================================

	/**
	 * Get the voice-over audio clip for a line.
	 *
	 * BLUEPRINT USAGE:
	 * Override this in your blueprint to provide custom audio lookup. For
	 * example, you might look up clips from a data table based on line ID,
	 * or load them from a specific folder based on character name.
	 *
	 * C++ USAGE:
	 * Override GetVoiceOverClip_Implementation in your subclass.
	 *
	 * DEFAULT BEHAVIOUR:
	 * The default implementation looks for an "audio:" metadata tag on the
	 * line and loads the asset from that path. If no tag is found, returns
	 * nullptr (no audio for this line).
	 *
	 * @param Line the localised line to get audio for
	 * @return the audio clip to play, or nullptr if none available
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Voice Over")
	USoundBase* GetVoiceOverClip(const FYarnLocalizedLine& Line);
	virtual USoundBase* GetVoiceOverClip_Implementation(const FYarnLocalizedLine& Line);

	// ========================================================================
	// events - bind to these in blueprints or c++
	// ========================================================================

	/**
	 * Called when voice-over playback starts for a line.
	 * Useful for triggering character lip-sync or animations.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Voice Over|Events")
	FYarnVoiceOverEvent OnVoiceOverStarted;

	/**
	 * Called when voice-over playback completes for a line.
	 * Fires both for natural completion and after fade-out from interruption.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Voice Over|Events")
	FYarnVoiceOverEvent OnVoiceOverComplete;

protected:
	// ========================================================================
	// internal state - not exposed to blueprints
	// ========================================================================

	/** The current line being presented (stored for logging/debugging). */
	FYarnLocalizedLine CurrentLine;

	/** Whether we're currently playing audio (not fading). */
	bool bIsPlaying = false;

	/** Whether we're in the process of fading out after interruption. */
	bool bIsFadingOut = false;

	/** Current fade-out progress (0 = just started, 1 = complete). */
	float FadeOutProgress = 0.0f;

	/** Original volume before fade-out started (restored after fade). */
	float OriginalVolume = 1.0f;

	/** Timer handle for pre-start delay - cleared in EndPlay to prevent crashes. */
	FTimerHandle PreStartTimerHandle;

	/** Timer handle for post-complete delay - cleared in EndPlay to prevent crashes. */
	FTimerHandle PostCompleteTimerHandle;

	// ========================================================================
	// internal methods
	// ========================================================================

	/** Makes sure we have a valid audio component, creating one if needed. */
	void EnsureAudioComponent();

	/** Starts playing the audio clip. */
	void StartPlayback(USoundBase* AudioClip);

	/** Called when audio finishes playing naturally. */
	void OnAudioFinished();

	/** Signals that line presentation is complete (broadcasts event, optionally advances). */
	void CompleteLine();

	/** Starts the fade-out animation for interrupted playback. */
	void BeginFadeOut();
};

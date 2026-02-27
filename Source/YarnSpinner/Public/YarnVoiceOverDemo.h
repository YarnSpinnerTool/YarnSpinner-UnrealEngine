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
#include "GameFramework/Actor.h"
#include "YarnDialogueRunner.h"
#include "YarnWidgetPresenter.h"
#include "YarnVoiceOverPresenter.h"
#include "YarnVoiceOverDemo.generated.h"

class UYarnProject;

// ============================================================================
// Voice-over demo actor - a complete, ready-to-use dialogue setup
// ============================================================================
//
// A complete, self-contained actor that sets up dialogue with text display,
// voice-over audio, and localisation support - all wired together and ready
// to use.
//
// Drop it in a level, assign a yarn project, point it at your audio folder,
// and call StartDialogue().
//
// BLUEPRINT VS C++ USAGE:
// - The actor is Blueprintable, so you can create blueprint subclasses.
// - All configuration properties are editable in blueprints and the editor.
// - Events (OnDialogueStarted, OnDialogueCompleted) are BlueprintAssignable.
// - Control methods (StartDialogue, StopDialogue, SetLocale) are BlueprintCallable.
// - For most use cases, you can configure everything in the editor without code.
//
// AUDIO FILE NAMING:
// Audio files are looked up by line ID. If your yarn script generates a line
// with ID "line:greeting-01", name your audio file "greeting-01" (the "line:"
// prefix is stripped automatically).
//
// LOCALISATION:
// - Base language audio goes in: {AudioBasePath}/{BaseAudioFolder}/
// - Localised audio goes in: {AudioBasePath}/Audio-{locale}/
// - For example: /Game/VoiceOverDemo/Audio/ for English (base)
//                /Game/VoiceOverDemo/Audio-de/ for German
//                /Game/VoiceOverDemo/Audio-zh-Hans/ for Simplified Chinese
//
// AUDIO CACHING:
// Loaded audio clips are cached to avoid repeated disk access. The cache is
// cleared when the locale changes, since different locales have different
// audio files.
//
// COMPONENTS:
// The actor creates three components in its constructor:
// - UYarnDialogueRunner: runs the yarn virtual machine
// - UYarnWidgetPresenter: displays text and options on screen
// - UYarnPathBasedVoiceOverPresenter: plays voice-over audio
// All three are visible in the editor and can be customised there.

/**
 * A ready-to-use voice-over demo actor that auto-loads audio by line ID.
 *
 * Audio files are automatically loaded based on the line ID - put your audio
 * files in a folder with names matching the line IDs.
 *
 * For example, if your line has ID "tutorial-tom-01", place an audio file
 * named "tutorial-tom-01" (any supported format) in the AudioBasePath folder.
 *
 * QUICK START:
 * 1. Place this actor in your level
 * 2. Assign the YarnProject
 * 3. Set AudioBasePath to your audio folder (e.g., "/Game/VoiceOverDemo/")
 * 4. Set bAutoStart = true, or call StartDialogue("Start")
 *
 * @see UYarnDialogueRunner the dialogue runner component
 * @see UYarnVoiceOverPresenter the voice-over presenter component
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API AYarnVoiceOverDemo : public AActor
{
	GENERATED_BODY()

public:
	AYarnVoiceOverDemo();

	virtual void BeginPlay() override;

	// ========================================================================
	// dialogue control - all callable from blueprints
	// ========================================================================

	/**
	 * Start dialogue at the specified node.
	 *
	 * @param NodeName the name of the yarn node to start from (e.g., "Start")
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void StartDialogue(const FString& NodeName);

	/**
	 * Stop the current dialogue immediately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void StopDialogue();

	/**
	 * Check if dialogue is currently running.
	 *
	 * @return true if dialogue is active, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	bool IsDialogueRunning() const;

	// ========================================================================
	// configuration - set these in the editor or before BeginPlay
	// ========================================================================

	/**
	 * The yarn project asset to use. Contains your compiled dialogue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	UYarnProject* YarnProject;

	/**
	 * The node to start dialogue at when using bAutoStart or StartDialogueFromStart.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FString StartNode = TEXT("Start");

	/**
	 * Whether to automatically start dialogue when the actor begins play.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	bool bAutoStart = false;

	/**
	 * If true, selected options will be run as lines (with voice over) before
	 * continuing to the next content.
	 *
	 * Useful when your options have recorded voice-over that should play when
	 * the player selects them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	bool bRunSelectedOptionAsLine = true;

	// ========================================================================
	// localisation - supports multiple languages for text and audio
	// ========================================================================

	/**
	 * The current locale code (e.g., "en", "de", "zh-Hans").
	 * Leave empty to use the system locale or base language.
	 *
	 * Affects both text lookup (via the line provider) and audio lookup
	 * (via the audio folder naming convention).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localisation")
	FString CurrentLocale;

	/**
	 * Set the locale for text and audio at runtime.
	 *
	 * Clears the audio cache (since different locales have different audio)
	 * and updates the line provider to fetch text in the new language.
	 *
	 * @param LocaleCode the locale code (e.g., "de", "pt-BR", "zh-Hans")
	 */
	UFUNCTION(BlueprintCallable, Category = "Localisation")
	void SetLocale(const FString& LocaleCode);

	/**
	 * Get the current locale code.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localisation")
	FString GetCurrentLocale() const { return CurrentLocale; }

	/**
	 * Get available locales from the yarn project.
	 *
	 * @return array of locale codes (e.g., ["en", "de", "zh-Hans"])
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localisation")
	TArray<FString> GetAvailableLocales() const;

	/**
	 * Base path for audio assets. The locale-specific folder will be appended.
	 *
	 * Example: "/Game/VoiceOverDemo/" becomes:
	 * - "/Game/VoiceOverDemo/Audio/" for base language
	 * - "/Game/VoiceOverDemo/Audio-de/" for German
	 * - "/Game/VoiceOverDemo/Audio-zh-Hans/" for Simplified Chinese
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over")
	FString AudioBasePath = TEXT("/Game/VoiceOverDemo/");

	/**
	 * Audio folder name for the base language (usually "Audio").
	 * Only used when CurrentLocale is empty or matches the project's BaseLanguage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over")
	FString BaseAudioFolder = TEXT("Audio");

	// ========================================================================
	// appearance - passed through to the widget presenter
	// ========================================================================

	/**
	 * Typewriter speed (characters per second). 0 = instant display.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	float TypewriterSpeed = 50.0f;

	/**
	 * Background colour for the dialogue box.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.9f);

	/**
	 * Text colour for dialogue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor TextColor = FLinearColor::White;

	/**
	 * Text colour for character names.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor CharacterNameColor = FLinearColor(0.8f, 0.6f, 0.2f, 1.0f);

	// ========================================================================
	// voice over timing - passed through to the voice over presenter
	// ========================================================================

	/**
	 * Fade-out time when audio is interrupted (seconds).
	 * Prevents jarring audio cuts when the player skips ahead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over", meta = (ClampMin = "0.0"))
	float FadeOutTime = 0.05f;

	/**
	 * Delay before starting audio playback (seconds).
	 * Useful for syncing with text fade-in animations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over", meta = (ClampMin = "0.0"))
	float PreDelay = 0.0f;

	/**
	 * Delay after audio completes (seconds).
	 * Gives a natural pause between lines.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over", meta = (ClampMin = "0.0"))
	float PostDelay = 0.0f;

	// ========================================================================
	// events - bind to these in blueprints or c++
	// ========================================================================

	/**
	 * Fired when dialogue starts running.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnYarnDialogueStartBP OnDialogueStarted;

	/**
	 * Fired when dialogue completes (reaches the end or is stopped).
	 */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnYarnDialogueCompleteBP OnDialogueCompleted;

	// ========================================================================
	// accessors - for getting at the underlying components
	// ========================================================================

	/**
	 * Get the dialogue runner component.
	 * Useful for advanced configuration or adding custom command handlers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	UYarnDialogueRunner* GetDialogueRunner() const { return DialogueRunner; }

	/**
	 * Load audio for a line ID from the configured path.
	 * Called by UYarnPathBasedVoiceOverPresenter to get audio clips.
	 *
	 * @param LineID the yarn line ID (e.g., "line:greeting-01")
	 * @return the loaded audio clip, or nullptr if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice Over")
	USoundBase* LoadAudioForLineID(const FString& LineID) const;

	/**
	 * Get the audio asset path for the current locale.
	 *
	 * @return the full path to the audio folder (e.g., "/Game/VoiceOverDemo/Audio-de/")
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Voice Over")
	FString GetAudioPathForLocale() const;

protected:
	// ========================================================================
	// components - created in constructor, visible in editor
	// ========================================================================

	/**
	 * The dialogue runner that executes yarn scripts.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UYarnDialogueRunner* DialogueRunner;

	/**
	 * The widget presenter that displays text and options on screen.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UYarnWidgetPresenter* WidgetPresenter;

	/**
	 * The voice-over presenter that plays audio clips.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UYarnVoiceOverPresenter* VoiceOverPresenter;

	/**
	 * Cache of loaded audio assets.
	 * Keyed by "locale:lineID" to handle multiple locales.
	 * Cleared when locale changes via SetLocale().
	 */
	UPROPERTY()
	mutable TMap<FString, USoundBase*> AudioCache;

	/** Internal handler for dialogue start event - rebroadcasts to our event. */
	UFUNCTION()
	void HandleDialogueStarted();

	/** Internal handler for dialogue complete event - rebroadcasts to our event. */
	UFUNCTION()
	void HandleDialogueComplete();
};

// ============================================================================
// path-based voice-over presenter - delegates audio lookup to the demo actor
// ============================================================================
//
// A presenter subclass that delegates audio lookup to its owning
// AYarnVoiceOverDemo actor. This keeps all the audio path logic in one place
// (the demo actor) rather than duplicating it in the presenter.
//
// You don't need to use this class directly - it's created automatically
// by AYarnVoiceOverDemo.

/**
 * Voice-over presenter that loads audio by line ID from a folder path.
 * Used internally by AYarnVoiceOverDemo.
 */
UCLASS()
class YARNSPINNER_API UYarnPathBasedVoiceOverPresenter : public UYarnVoiceOverPresenter
{
	GENERATED_BODY()

public:
	/**
	 * The owning demo actor that handles audio loading.
	 * Set automatically by AYarnVoiceOverDemo in BeginPlay.
	 */
	UPROPERTY()
	AYarnVoiceOverDemo* OwnerActor;

	/**
	 * Delegates to OwnerActor->LoadAudioForLineID.
	 * Falls back to base implementation if no owner actor.
	 */
	virtual USoundBase* GetVoiceOverClip_Implementation(const FYarnLocalizedLine& Line) override;
};

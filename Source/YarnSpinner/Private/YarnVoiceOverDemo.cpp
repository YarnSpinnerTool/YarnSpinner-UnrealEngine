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

// Our own header - must be included first for the Unreal header tool.
#include "YarnVoiceOverDemo.h"

// UYarnProject - the compiled dialogue asset, providing access to base text,
// localisations, and available locales.
#include "YarnProgram.h"

// UYarnBuiltinLineProvider - created at runtime to handle text lookup
// with localisation support.
#include "YarnLocalization.h"

// Logging macros (UE_LOG with LogYarnSpinner category).
#include "YarnSpinnerModule.h"

// USoundBase - the base class for all audio assets, used by
// LoadAudioForLineID to return loaded audio clips.
#include "Sound/SoundBase.h"

// ============================================================================
// AYarnVoiceOverDemo
// ============================================================================
//
// A ready-to-use demo actor that sets up a complete voice-over enabled
// dialogue system, demonstrating how to integrate Yarn Spinner with localised
// audio.
//
// To use it:
// 1. Place this actor in your level
// 2. Assign a yarn project
// 3. Set the audio base path to where your audio files live
// 4. Optionally set a starting locale or let it auto-detect
// 5. Call StartDialogue() or set bAutoStart = true
//
// The actor handles all the wiring between the dialogue runner, text presenter,
// voice-over presenter, and localisation system.

// ----------------------------------------------------------------------------
// constructor - sets up default subobjects
// ----------------------------------------------------------------------------

AYarnVoiceOverDemo::AYarnVoiceOverDemo()
{
	// We don't need per-frame ticking for dialogue.
	PrimaryActorTick.bCanEverTick = false;

	// Create the core components as default subobjects so they're always
	// available and show up in the editor. Using CreateDefaultSubobject
	// means they'll be created automatically with the actor.
	DialogueRunner = CreateDefaultSubobject<UYarnDialogueRunner>(TEXT("DialogueRunner"));
	WidgetPresenter = CreateDefaultSubobject<UYarnWidgetPresenter>(TEXT("WidgetPresenter"));
	VoiceOverPresenter = CreateDefaultSubobject<UYarnPathBasedVoiceOverPresenter>(TEXT("VoiceOverPresenter"));
}

// ----------------------------------------------------------------------------
// lifecycle
// ----------------------------------------------------------------------------

void AYarnVoiceOverDemo::BeginPlay()
{
	// --------------------------------------------------------------------
	// set up the dialogue runner
	// --------------------------------------------------------------------
	if (DialogueRunner)
	{
		if (YarnProject)
		{
			// Assign the yarn project to the runner.
			DialogueRunner->YarnProject = YarnProject;

			// Create and configure the builtin line provider for localisation.
			// This handles looking up text in the correct language based on
			// the current locale setting.
			UYarnBuiltinLineProvider* LineProvider = NewObject<UYarnBuiltinLineProvider>(this);
			LineProvider->SetYarnProject(YarnProject);

			// If no locale is explicitly set, enable auto-detection which
			// will try to match the system language to available translations.
			LineProvider->bAutoDetectCulture = CurrentLocale.IsEmpty();
			if (!CurrentLocale.IsEmpty())
			{
				LineProvider->TextLocaleCode = CurrentLocale;
			}

			DialogueRunner->LineProvider = LineProvider;
		}

		// We'll start dialogue manually or via bAutoStart, not immediately.
		DialogueRunner->bAutoStart = false;

		// Pass through the run-selected-option-as-line setting. When enabled,
		// selecting an option will play its voice-over before continuing.
		DialogueRunner->bRunSelectedOptionAsLine = bRunSelectedOptionAsLine;

		// Register both presenters with the dialogue runner. They'll both
		// receive lines and can handle them in parallel (text display and
		// audio playback).
		DialogueRunner->DialoguePresenters.Add(WidgetPresenter);
		DialogueRunner->DialoguePresenters.Add(VoiceOverPresenter);

		// Hook up event delegates so we can re-broadcast them.
		DialogueRunner->OnDialogueStart.AddDynamic(this, &AYarnVoiceOverDemo::HandleDialogueStarted);
		DialogueRunner->OnDialogueComplete.AddDynamic(this, &AYarnVoiceOverDemo::HandleDialogueComplete);
	}

	// --------------------------------------------------------------------
	// configure the widget presenter (text display)
	// --------------------------------------------------------------------
	if (WidgetPresenter)
	{
		// Apply the visual settings from our exposed properties.
		WidgetPresenter->TypewriterSpeed = TypewriterSpeed;
		WidgetPresenter->BackgroundColor = BackgroundColor;
		WidgetPresenter->TextColor = TextColor;
		WidgetPresenter->CharacterNameColor = CharacterNameColor;
	}

	// --------------------------------------------------------------------
	// configure the voice-over presenter (audio playback)
	// --------------------------------------------------------------------
	if (VoiceOverPresenter)
	{
		// Apply timing settings.
		VoiceOverPresenter->FadeOutTimeOnInterrupt = FadeOutTime;
		VoiceOverPresenter->WaitTimeBeforeStart = PreDelay;
		VoiceOverPresenter->WaitTimeAfterComplete = PostDelay;

		// Don't let the voice-over presenter control line advancement - we
		// want the text presenter to do that, or manual player input.
		VoiceOverPresenter->bEndLineWhenVoiceOverComplete = false;

		// Give the path-based presenter a reference back to us so it can
		// call our LoadAudioForLineID function.
		if (UYarnPathBasedVoiceOverPresenter* PathPresenter = Cast<UYarnPathBasedVoiceOverPresenter>(VoiceOverPresenter))
		{
			PathPresenter->OwnerActor = this;
		}
	}

	Super::BeginPlay();

	// If auto-start is enabled and we have a starting node, start dialogue.
	if (bAutoStart && !StartNode.IsEmpty())
	{
		StartDialogue(StartNode);
	}
}

// ----------------------------------------------------------------------------
// dialogue control
// ----------------------------------------------------------------------------

void AYarnVoiceOverDemo::StartDialogue(const FString& NodeName)
{
	// Delegate to the runner.
	if (DialogueRunner)
	{
		DialogueRunner->StartDialogue(NodeName);
	}
}

void AYarnVoiceOverDemo::StopDialogue()
{
	// Delegate to the runner.
	if (DialogueRunner)
	{
		DialogueRunner->StopDialogue();
	}
}

bool AYarnVoiceOverDemo::IsDialogueRunning() const
{
	return DialogueRunner && DialogueRunner->IsDialogueRunning();
}

// ----------------------------------------------------------------------------
// localisation
// ----------------------------------------------------------------------------

void AYarnVoiceOverDemo::SetLocale(const FString& LocaleCode)
{
	// Store the new locale.
	CurrentLocale = LocaleCode;

	// Clear the audio cache - the cached audio files are locale-specific,
	// so we need to reload them when the locale changes.
	AudioCache.Empty();

	// Update the line provider so text is fetched in the new language.
	if (DialogueRunner && DialogueRunner->LineProvider)
	{
		if (UYarnBuiltinLineProvider* BuiltinProvider = Cast<UYarnBuiltinLineProvider>(DialogueRunner->LineProvider))
		{
			BuiltinProvider->SetLocaleCode(LocaleCode);
		}
	}

	UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverDemo: Locale set to '%s'"), *LocaleCode);
}

TArray<FString> AYarnVoiceOverDemo::GetAvailableLocales() const
{
	TArray<FString> Locales;

	if (YarnProject)
	{
		// Include the base language first.
		if (!YarnProject->BaseLanguage.IsEmpty())
		{
			Locales.Add(YarnProject->BaseLanguage);
		}

		// Add all additional localisation languages.
		for (const auto& Pair : YarnProject->Localizations)
		{
			// Use AddUnique to avoid duplicates if base language is also
			// in the localizations map.
			Locales.AddUnique(Pair.Key);
		}
	}

	return Locales;
}

// ----------------------------------------------------------------------------
// audio path construction
// ----------------------------------------------------------------------------

FString AYarnVoiceOverDemo::GetAudioPathForLocale() const
{
	// Start with the configured base path.
	FString BasePath = AudioBasePath;

	// Ensure it ends with a slash for proper path concatenation.
	if (!BasePath.EndsWith(TEXT("/")))
	{
		BasePath += TEXT("/");
	}

	// If no locale is set, or we're using the base language, use the base
	// audio folder (typically just "Audio").
	if (CurrentLocale.IsEmpty() || (YarnProject && CurrentLocale == YarnProject->BaseLanguage))
	{
		return BasePath + BaseAudioFolder + TEXT("/");
	}

	// For other locales, use the naming convention: Audio-{locale}
	// Examples: Audio-de, Audio-pt-BR, Audio-zh-Hans
	return BasePath + TEXT("Audio-") + CurrentLocale + TEXT("/");
}

// ----------------------------------------------------------------------------
// audio loading
// ----------------------------------------------------------------------------

USoundBase* AYarnVoiceOverDemo::LoadAudioForLineID(const FString& LineID) const
{
	// Create a cache key that includes the locale, so different locales
	// don't share cached audio references.
	FString CacheKey = CurrentLocale + TEXT(":") + LineID;

	// Check if we've already loaded this audio.
	if (USoundBase** CachedAudio = AudioCache.Find(CacheKey))
	{
		return *CachedAudio;
	}

	// Yarn line IDs typically have a "line:" prefix, but audio files
	// are named without it. Strip the prefix if present.
	FString AssetName = LineID;
	if (AssetName.StartsWith(TEXT("line:")))
	{
		AssetName = AssetName.RightChop(5);  // 5 = length of "line:"
	}

	// Construct the full asset path.
	FString AudioPath = GetAudioPathForLocale();
	FString AssetPath = AudioPath + AssetName;

	UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverDemo: Looking for audio at '%s' (locale: %s)"),
		*AssetPath, CurrentLocale.IsEmpty() ? TEXT("base") : *CurrentLocale);

	// Try to load the audio asset. We use StaticLoadObject for synchronous
	// loading, which is fine for audio clips that are typically small.
	USoundBase* Audio = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *AssetPath));

	if (Audio)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverDemo: Found audio for line '%s'"), *LineID);
		AudioCache.Add(CacheKey, Audio);
	}
	else
	{
		// Try an alternate path format where the asset name is repeated
		// (some import workflows create paths like /Game/Audio/Line01.Line01).
		FString WavePath = AssetPath + TEXT(".") + AssetName;
		Audio = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *WavePath));

		if (Audio)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("VoiceOverDemo: Found audio for line '%s' (alternate path)"), *LineID);
			AudioCache.Add(CacheKey, Audio);
		}
		else
		{
			// No audio found - this might be intentional (not all lines have
			// voice over) so we log a warning.
			UE_LOG(LogYarnSpinner, Warning, TEXT("VoiceOverDemo: No audio found for line '%s' at path '%s'"), *LineID, *AssetPath);
		}
	}

	return Audio;
}

// ----------------------------------------------------------------------------
// event handlers
// ----------------------------------------------------------------------------

void AYarnVoiceOverDemo::HandleDialogueStarted()
{
	// Re-broadcast the event so blueprint or C++ listeners on this actor
	// can respond to dialogue starting.
	OnDialogueStarted.Broadcast();
}

void AYarnVoiceOverDemo::HandleDialogueComplete()
{
	// Re-broadcast the event so blueprint or C++ listeners on this actor
	// can respond to dialogue ending.
	OnDialogueCompleted.Broadcast();
}

// ============================================================================
// UYarnPathBasedVoiceOverPresenter
// ============================================================================
//
// A specialised voice-over presenter that delegates audio lookup to its owning
// AYarnVoiceOverDemo actor. This allows the demo actor to control audio loading
// and caching in a centralised place.

USoundBase* UYarnPathBasedVoiceOverPresenter::GetVoiceOverClip_Implementation(const FYarnLocalizedLine& Line)
{
	// If we have an owner actor, let it handle audio loading.
	if (OwnerActor)
	{
		return OwnerActor->LoadAudioForLineID(Line.RawLine.LineID);
	}

	// Fall back to the base implementation which looks for audio: metadata.
	return Super::GetVoiceOverClip_Implementation(Line);
}

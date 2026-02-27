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

// Ready-to-use dialogue UI widgets and demo actors. Use these as a starting
// point for your own dialogue UI, or drop in the demo actors to get something
// working quickly.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "YarnSpinnerCore.h"
#include "YarnDialoguePresenter.h"
#include "YarnDialogueUITemplates.generated.h"

// Forward declarations
class UYarnDialogueRunner;
class UTextBlock;
class UButton;
class UVerticalBox;
class UHorizontalBox;
class UImage;
class UBorder;
class USizeBox;

// ============================================================================
// Simple Dialogue UI
// ============================================================================
// A minimal dialogue UI with just a text box and option buttons.
// Good for prototyping or simple games.

/**
 * Simple dialogue UI widget - minimal text box with options.
 *
 * This is a ready-to-use dialogue UI that provides:
 * - A text area that displays dialogue with typewriter effect
 * - A speaker name display
 * - Option buttons that appear when choices are available
 * - Click/key to advance functionality
 *
 * Extend this class in Blueprint to customize the appearance.
 * Override the bound widget names to use your own UMG layout.
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API UYarnSimpleDialogueWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ========================================================================
    // Widget Bindings - Override these in your subclass UMG
    // ========================================================================

    /** The text block for the speaker's name */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* SpeakerNameText;

    /** The text block for the dialogue content */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* DialogueText;

    /** Container for option buttons */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UVerticalBox* OptionsContainer;

    /** The continue indicator (shown when waiting for player input) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UWidget* ContinueIndicator;

    /** Border/background for the dialogue box */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UBorder* DialogueBoxBorder;

    // ========================================================================
    // Configuration
    // ========================================================================

    /** Speed of the typewriter effect (characters per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    float TypewriterSpeed = 30.0f;

    /** Whether to use typewriter effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    bool bUseTypewriter = true;

    /** Class to use for option buttons (must have a UTextBlock named "OptionText") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    TSubclassOf<UUserWidget> OptionButtonClass;

    // ========================================================================
    // Public Interface
    // ========================================================================

    /** Display a line of dialogue */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void ShowLine(const FString& SpeakerName, const FString& Text);

    /** Display dialogue options */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void ShowOptions(const TArray<FYarnOption>& Options);

    /** Clear the options display */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void ClearOptions();

    /** Show/hide the continue indicator */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void SetContinueIndicatorVisible(bool bVisible);

    /** Skip the typewriter effect and show full text */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void SkipTypewriter();

    /** Check if typewriter is still animating */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    bool IsTypewriterAnimating() const { return bIsTypewriting; }

    // ========================================================================
    // Events
    // ========================================================================

    /** Called when an option is selected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOptionSelected, int32, OptionIndex);
    UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|UI")
    FOnOptionSelected OnOptionSelected;

    /** Called when typewriter finishes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTypewriterComplete);
    UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|UI")
    FOnTypewriterComplete OnTypewriterComplete;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Typewriter state
    FString CurrentFullText;
    int32 TypewriterIndex = 0;
    bool bIsTypewriting = false;
    float TypewriterTime = 0.0f;

    UFUNCTION()
    void OnOptionButtonClicked(int32 Index);
};

// ============================================================================
// RPG Dialogue UI
// ============================================================================
// A more feature-rich UI with portrait, name plate, and styled options.
// Good for RPGs, visual novels, and story-driven games.

/**
 * RPG-style dialogue UI widget with portrait support.
 *
 * Features:
 * - Character portrait display
 * - Styled name plate
 * - Dialogue text with typewriter effect
 * - Numbered option buttons
 * - Support for character emotions
 *
 * Extend this class in Blueprint to customize appearance.
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API UYarnRPGDialogueWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ========================================================================
    // Widget Bindings
    // ========================================================================

    /** Portrait image */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UImage* PortraitImage;

    /** Speaker name text */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* SpeakerNameText;

    /** Name plate background */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UBorder* NamePlateBorder;

    /** Dialogue text */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* DialogueText;

    /** Options container */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UVerticalBox* OptionsContainer;

    /** Continue indicator */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UWidget* ContinueIndicator;

    /** Main dialogue box border */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UBorder* DialogueBoxBorder;

    // ========================================================================
    // Configuration
    // ========================================================================

    /** Typewriter speed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    float TypewriterSpeed = 40.0f;

    /** Use typewriter effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    bool bUseTypewriter = true;

    /** Option button widget class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    TSubclassOf<UUserWidget> OptionButtonClass;

    /** Map of character names to portrait textures */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    TMap<FString, UTexture2D*> CharacterPortraits;

    /** Map of character names to name plate colors */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    TMap<FString, FLinearColor> CharacterColors;

    /** Default portrait for unknown characters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    UTexture2D* DefaultPortrait;

    /** Default name plate color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    FLinearColor DefaultNameColor = FLinearColor(0.2f, 0.2f, 0.8f, 1.0f);

    // ========================================================================
    // Public Interface
    // ========================================================================

    /** Display a line with portrait */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void ShowLine(const FString& SpeakerName, const FString& Text, const FString& Emotion = TEXT(""));

    /** Set portrait by character name */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void SetPortrait(const FString& CharacterName, const FString& Emotion = TEXT(""));

    /** Set portrait directly from texture */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void SetPortraitTexture(UTexture2D* Texture);

    /** Display options */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void ShowOptions(const TArray<FYarnOption>& Options);

    /** Clear options */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void ClearOptions();

    /** Show/hide continue indicator */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void SetContinueIndicatorVisible(bool bVisible);

    /** Skip typewriter */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void SkipTypewriter();

    /** Check if typewriter is animating */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    bool IsTypewriterAnimating() const { return bIsTypewriting; }

    // ========================================================================
    // Events
    // ========================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOptionSelected, int32, OptionIndex);
    UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|UI")
    FOnOptionSelected OnOptionSelected;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTypewriterComplete);
    UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|UI")
    FOnTypewriterComplete OnTypewriterComplete;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Typewriter state
    FString CurrentFullText;
    int32 TypewriterIndex = 0;
    bool bIsTypewriting = false;
    float TypewriterTime = 0.0f;

    UFUNCTION()
    void OnOptionButtonClicked(int32 Index);
};

// ============================================================================
// Subtitle UI
// ============================================================================
// A minimal subtitle display for games with voice acting.
// Shows text at the bottom of the screen without interrupting gameplay.

/**
 * Subtitle-style dialogue widget.
 *
 * Features:
 * - Minimal UI that doesn't block gameplay
 * - Optional speaker name prefix
 * - Auto-hide after duration
 * - Semi-transparent background
 *
 * Perfect for:
 * - Voice-over heavy games
 * - Environmental storytelling
 * - Non-blocking dialogue
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API UYarnSubtitleWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ========================================================================
    // Widget Bindings
    // ========================================================================

    /** The subtitle text */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* SubtitleText;

    /** Background border */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UBorder* BackgroundBorder;

    // ========================================================================
    // Configuration
    // ========================================================================

    /** Include speaker name in subtitle (e.g., "Guard: Stop right there!") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    bool bShowSpeakerName = true;

    /** Format string for speaker name (use {0} for name, {1} for text) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    FString SpeakerFormat = TEXT("{0}: {1}");

    /** Auto-hide subtitle after this many seconds (0 = don't auto-hide) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    float AutoHideDuration = 0.0f;

    /** Fade in/out duration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    float FadeDuration = 0.2f;

    /** Background opacity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|UI")
    float BackgroundOpacity = 0.7f;

    // ========================================================================
    // Public Interface
    // ========================================================================

    /** Show a subtitle */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void ShowSubtitle(const FString& SpeakerName, const FString& Text);

    /** Hide the subtitle */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    void HideSubtitle();

    /** Check if subtitle is visible */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|UI")
    bool IsSubtitleVisible() const { return bIsVisible; }

    // ========================================================================
    // Events
    // ========================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSubtitleHidden);
    UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|UI")
    FOnSubtitleHidden OnSubtitleHidden;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Visibility and fade state
    bool bIsVisible = false;
    float CurrentOpacity = 0.0f;
    float TimeRemaining = 0.0f;
    bool bIsFadingIn = false;
    bool bIsFadingOut = false;
};

// ============================================================================
// Demo Actors
// ============================================================================
// Ready-to-use dialogue setups that you can drop into a level. They handle
// all the wiring between dialogue runner and UI widgets.

/**
 * Drop-in demo actor using the Simple Dialogue UI.
 *
 * Drag this into your level for an instant working dialogue system.
 * Set the YarnProject and StartNode, then hit Play.
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API AYarnSimpleDialogueDemo : public AActor
{
    GENERATED_BODY()

public:
    AYarnSimpleDialogueDemo();

    virtual void BeginPlay() override;

    /** The Yarn project to run */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    UYarnProject* YarnProject;

    /** The node to start from */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    FString StartNode = TEXT("Start");

    /** Whether to start dialogue automatically */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    bool bAutoStart = true;

    /** Key to advance dialogue */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    FKey AdvanceKey = EKeys::SpaceBar;

    /** Custom widget class (leave empty for default) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    TSubclassOf<UYarnSimpleDialogueWidget> CustomWidgetClass;

    /** Start dialogue manually */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
    void StartDialogue();

    /** Get the dialogue runner */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
    UYarnDialogueRunner* GetDialogueRunner() const { return DialogueRunner; }

protected:
    UPROPERTY()
    UYarnDialogueRunner* DialogueRunner;

    UPROPERTY()
    UYarnSimpleDialogueWidget* DialogueWidget;

    void SetupDialogueRunner();
    void SetupUI();

    // Dialogue event handlers
    UFUNCTION()
    void OnLineReceived(const FYarnLocalizedLine& Line);

    UFUNCTION()
    void OnOptionsReceived(const FYarnOptionSet& Options);

    UFUNCTION()
    void OnDialogueComplete();

    UFUNCTION()
    void OnOptionSelected(int32 OptionIndex);
};

/**
 * Drop-in demo actor using the RPG Dialogue UI.
 *
 * Features portrait support and styled name plates.
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API AYarnRPGDialogueDemo : public AActor
{
    GENERATED_BODY()

public:
    AYarnRPGDialogueDemo();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    UYarnProject* YarnProject;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    FString StartNode = TEXT("Start");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    bool bAutoStart = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    FKey AdvanceKey = EKeys::SpaceBar;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    TSubclassOf<UYarnRPGDialogueWidget> CustomWidgetClass;

    /** Character portraits (character name -> texture) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Characters")
    TMap<FString, UTexture2D*> CharacterPortraits;

    /** Character name plate colors */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Characters")
    TMap<FString, FLinearColor> CharacterColors;

    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
    void StartDialogue();

    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
    UYarnDialogueRunner* GetDialogueRunner() const { return DialogueRunner; }

protected:
    UPROPERTY()
    UYarnDialogueRunner* DialogueRunner;

    UPROPERTY()
    UYarnRPGDialogueWidget* DialogueWidget;

    void SetupDialogueRunner();
    void SetupUI();

    // Dialogue event handlers
    UFUNCTION()
    void OnLineReceived(const FYarnLocalizedLine& Line);

    UFUNCTION()
    void OnOptionsReceived(const FYarnOptionSet& Options);

    UFUNCTION()
    void OnDialogueComplete();

    UFUNCTION()
    void OnOptionSelected(int32 OptionIndex);
};

/**
 * Drop-in demo actor using the Subtitle UI.
 *
 * Perfect for voice-over heavy games or environmental storytelling.
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API AYarnSubtitleDemo : public AActor
{
    GENERATED_BODY()

public:
    AYarnSubtitleDemo();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    UYarnProject* YarnProject;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    FString StartNode = TEXT("Start");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    bool bAutoStart = true;

    /** Duration to show each subtitle (0 = wait for advance key) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    float SubtitleDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    FKey AdvanceKey = EKeys::SpaceBar;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
    TSubclassOf<UYarnSubtitleWidget> CustomWidgetClass;

    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
    void StartDialogue();

    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
    UYarnDialogueRunner* GetDialogueRunner() const { return DialogueRunner; }

protected:
    UPROPERTY()
    UYarnDialogueRunner* DialogueRunner;

    UPROPERTY()
    UYarnSubtitleWidget* SubtitleWidget;

    void SetupDialogueRunner();
    void SetupUI();

    // Dialogue event handlers
    UFUNCTION()
    void OnLineReceived(const FYarnLocalizedLine& Line);

    UFUNCTION()
    void OnOptionsReceived(const FYarnOptionSet& Options);

    UFUNCTION()
    void OnDialogueComplete();

    // Timer for auto-advancing dialogue after subtitle duration
    FTimerHandle AutoAdvanceTimer;
};

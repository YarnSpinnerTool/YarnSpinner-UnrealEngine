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
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "YarnLinePresenter.generated.h"

class UCanvasPanel;

/** Delegate for typewriter completion event */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnTypewriterComplete);

/**
 * The typewriter display mode.
 */
UENUM(BlueprintType)
enum class EYarnTypewriterMode : uint8
{
	/** Show all text instantly */
	Instant UMETA(DisplayName = "Instant"),

	/** Reveal text letter by letter */
	ByLetter UMETA(DisplayName = "By Letter"),

	/** Reveal text word by word */
	ByWord UMETA(DisplayName = "By Word")
};

/**
 * A dialogue presenter that displays lines using UMG widgets.
 *
 * This presenter handles text display with typewriter effects, character name
 * display, fade effects, and auto-advance functionality. Configure it with
 * references to your UMG widgets.
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnLinePresenter : public UYarnDialoguePresenter
{
	GENERATED_BODY()

public:
	UYarnLinePresenter();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// UYarnDialoguePresenter interface
	virtual void OnDialogueStarted_Implementation() override;
	virtual void OnDialogueComplete_Implementation() override;
	virtual void RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry) override;
	virtual void RunOptions_Implementation(const FYarnOptionSet& Options) override;
	virtual void OnHurryUpRequested_Implementation() override;
	virtual void OnNextLineRequested_Implementation() override;

	/**
	 * The text widget that displays the dialogue text.
	 * Set this to a UTextBlock in your UI.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Widgets")
	UTextBlock* LineTextWidget;

	/**
	 * The text widget that displays the character name.
	 * If not set, character names will be included in the line text.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Widgets")
	UTextBlock* CharacterNameWidget;

	/**
	 * The container widget for the character name.
	 * This widget will be shown/hidden based on whether a character name is present.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Widgets")
	UWidget* CharacterNameContainer;

	/**
	 * The container widget for the entire line display.
	 * This widget will be shown/hidden during dialogue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Widgets")
	UWidget* LineContainer;

	/**
	 * Whether to show character names in the line text when no separate
	 * character name widget is configured.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Character")
	bool bShowCharacterNameInLine = true;

	/**
	 * The typewriter mode for revealing text.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Typewriter")
	EYarnTypewriterMode TypewriterMode = EYarnTypewriterMode::ByLetter;

	/**
	 * Characters per second for letter-by-letter typewriter effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Typewriter", meta = (EditCondition = "TypewriterMode == EYarnTypewriterMode::ByLetter", ClampMin = "1"))
	int32 LettersPerSecond = 60;

	/**
	 * Words per second for word-by-word typewriter effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Typewriter", meta = (EditCondition = "TypewriterMode == EYarnTypewriterMode::ByWord", ClampMin = "1"))
	int32 WordsPerSecond = 10;

	/**
	 * Whether to fade the UI in and out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Fade")
	bool bUseFadeEffect = false;

	/**
	 * Duration of the fade-in effect in seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Fade", meta = (EditCondition = "bUseFadeEffect", ClampMin = "0.0"))
	float FadeInDuration = 0.25f;

	/**
	 * Duration of the fade-out effect in seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Fade", meta = (EditCondition = "bUseFadeEffect", ClampMin = "0.0"))
	float FadeOutDuration = 0.1f;

	/**
	 * Whether to automatically advance to the next line after display.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Auto Advance")
	bool bAutoAdvance = false;

	/**
	 * Delay in seconds before auto-advancing after the line finishes displaying.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Auto Advance", meta = (EditCondition = "bAutoAdvance", ClampMin = "0.0"))
	float AutoAdvanceDelay = 1.0f;

	/**
	 * Called when the typewriter effect completes.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnTypewriterComplete OnTypewriterComplete;

protected:
	/** The full text to display */
	FString FullText;

	/** Current character index for typewriter effect */
	int32 CurrentCharIndex;

	/** Time accumulator for typewriter effect */
	float TypewriterTimer;

	/** Whether we're currently running a typewriter effect */
	bool bIsTypewriting;

	/** Whether the typewriter was hurried */
	bool bTypewriterHurried;

	/** Time remaining before auto-advance */
	float AutoAdvanceTimer;

	/** Whether we're waiting for auto-advance */
	bool bWaitingForAutoAdvance;

	/** Show the full text immediately */
	void ShowFullText();

	/** Update the displayed text during typewriter effect */
	void UpdateTypewriterText();

	/** Set visibility of the line container */
	void SetLineContainerVisible(bool bVisible);

	/** Set visibility of the character name container */
	void SetCharacterNameVisible(bool bVisible);
};

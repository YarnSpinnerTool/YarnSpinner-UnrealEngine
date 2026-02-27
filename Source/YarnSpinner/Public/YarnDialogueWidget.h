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
#include "Blueprint/UserWidget.h"
#include "YarnSpinnerCore.h"
#include "YarnDialogueWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UButton;
class UBorder;
class UCanvasPanel;
class SVerticalBox;
class STextBlock;
class SBorder;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnDialogueContinue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYarnOptionChosen, int32, OptionIndex);

/**
 * A ready-to-use dialogue widget for displaying Yarn Spinner dialogue.
 *
 * This widget provides a simple but functional dialogue UI with:
 * - Character name display
 * - Dialogue text with typewriter effect
 * - Option buttons for choices
 * - Continue button/click to advance
 *
 * You can use this directly or subclass it for customization.
 * For Blueprint customization, create a Widget Blueprint that derives from this class
 * and use BindWidget meta to connect your own UI elements.
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UYarnDialogueWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

	/**
	 * Show a line of dialogue.
	 * @param CharacterName The name of the speaking character (empty if none).
	 * @param DialogueText The dialogue text to display.
	 * @param bUseTypewriter Whether to use typewriter effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Dialogue Widget")
	void ShowLine(const FString& CharacterName, const FString& DialogueText, bool bUseTypewriter = true);

	/**
	 * Show dialogue options.
	 * @param Options The options to display.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Dialogue Widget")
	void ShowOptions(const TArray<FYarnOption>& Options);

	/**
	 * Hide the options panel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Dialogue Widget")
	void HideOptions();

	/**
	 * Show the dialogue widget.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Dialogue Widget")
	void ShowDialogue();

	/**
	 * Hide the dialogue widget.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Dialogue Widget")
	void HideDialogue();

	/**
	 * Skip the typewriter effect and show full text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Dialogue Widget")
	void SkipTypewriter();

	/**
	 * Check if typewriter is currently running.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Dialogue Widget")
	bool IsTypewriting() const { return bIsTypewriting; }

	/** Called when the player wants to continue (click or button). */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Dialogue Widget")
	FOnYarnDialogueContinue OnContinue;

	/** Called when the player selects an option. */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Dialogue Widget")
	FOnYarnOptionChosen OnOptionChosen;

	/** Characters per second for typewriter effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Dialogue Widget")
	float TypewriterSpeed = 50.0f;

	/** Background color for the dialogue box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Dialogue Widget|Appearance")
	FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.15f, 0.95f);

	/** Text color for dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Dialogue Widget|Appearance")
	FLinearColor TextColor = FLinearColor::White;

	/** Text color for character name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Dialogue Widget|Appearance")
	FLinearColor CharacterNameColor = FLinearColor(1.0f, 0.8f, 0.3f, 1.0f);

	/** Font size for dialogue text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Dialogue Widget|Appearance")
	int32 DialogueFontSize = 18;

	/** Font size for character name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Dialogue Widget|Appearance")
	int32 CharacterNameFontSize = 22;

protected:
	// Blueprint-bindable widgets (optional - if not bound, Slate UI is used)

	/** The main container panel. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Yarn Spinner|Dialogue Widget")
	UCanvasPanel* RootCanvas;

	/** The dialogue box background. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Yarn Spinner|Dialogue Widget")
	UBorder* DialogueBox;

	/** Text block for character name. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Yarn Spinner|Dialogue Widget")
	UTextBlock* CharacterNameText;

	/** Text block for dialogue. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Yarn Spinner|Dialogue Widget")
	UTextBlock* DialogueText;

	/** Container for option buttons. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Yarn Spinner|Dialogue Widget")
	UVerticalBox* OptionsContainer;

	/** Continue indicator text. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Yarn Spinner|Dialogue Widget")
	UTextBlock* ContinueIndicator;

private:
	// Slate widgets for programmatic UI
	TSharedPtr<SBorder> SlateDialogueBox;
	TSharedPtr<SVerticalBox> SlateContentBox;
	TSharedPtr<STextBlock> SlateCharacterName;
	TSharedPtr<STextBlock> SlateDialogueText;
	TSharedPtr<SVerticalBox> SlateOptionsBox;
	TSharedPtr<STextBlock> SlateContinueIndicator;

	FString FullDialogueText;
	int32 CurrentCharIndex;
	float TypewriterTimer;
	bool bIsTypewriting;
	bool bUsingSlateUI;
	TArray<FYarnOption> CurrentOptions;

	/** Update the displayed text during typewriter effect */
	void UpdateTypewriterText();

	/** Handle option button click */
	FReply OnOptionClicked(int32 Index);

	/** Select an option by keyboard number (0-8 for keys 1-9) */
	void SelectOptionByNumber(int32 Index);
};

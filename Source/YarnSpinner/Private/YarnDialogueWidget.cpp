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

#include "YarnDialogueWidget.h"
#include "YarnSpinnerModule.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SOverlay.h"
#include "GameFramework/PlayerController.h"

UYarnDialogueWidget::UYarnDialogueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurrentCharIndex(0)
	, TypewriterTimer(0.0f)
	, bIsTypewriting(false)
	, bUsingSlateUI(false)
{
}

TSharedRef<SWidget> UYarnDialogueWidget::RebuildWidget()
{
	// Check if Blueprint has bound widgets
	if (DialogueBox && DialogueText)
	{
		bUsingSlateUI = false;
		return Super::RebuildWidget();
	}

	// No Blueprint widgets - create Slate UI directly
	bUsingSlateUI = true;

	// Create the dialogue text widget
	SAssignNew(SlateDialogueText, STextBlock)
		.Text(FText::GetEmpty())
		.ColorAndOpacity(FSlateColor(TextColor))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", DialogueFontSize))
		.AutoWrapText(true);

	// Create character name widget
	SAssignNew(SlateCharacterName, STextBlock)
		.Text(FText::GetEmpty())
		.ColorAndOpacity(FSlateColor(CharacterNameColor))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", CharacterNameFontSize))
		.Visibility(EVisibility::Collapsed);

	// Create continue indicator
	SAssignNew(SlateContinueIndicator, STextBlock)
		.Text(FText::FromString(TEXT("Click to continue...")))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 0.8f)))
		.Font(FCoreStyle::GetDefaultFontStyle("Italic", 14))
		.Visibility(EVisibility::Collapsed);

	// Create options container
	SAssignNew(SlateOptionsBox, SVerticalBox)
		.Visibility(EVisibility::Collapsed);

	// Create content box
	SAssignNew(SlateContentBox, SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SlateCharacterName.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SlateDialogueText.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 12, 0, 0)
		[
			SlateOptionsBox.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(0, 8, 0, 0)
		[
			SlateContinueIndicator.ToSharedRef()
		];

	// Create the dialogue box with border
	SAssignNew(SlateDialogueBox, SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(FSlateColor(BackgroundColor))
		.Padding(FMargin(24.0f, 20.0f))
		[
			SlateContentBox.ToSharedRef()
		];

	// Create the full widget with positioning
	return SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(40.0f, 0.0f, 40.0f, 40.0f))
		[
			SNew(SBox)
			.MinDesiredHeight(150.0f)
			.MaxDesiredHeight(300.0f)
			[
				SlateDialogueBox.ToSharedRef()
			]
		];
}

void UYarnDialogueWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	SlateDialogueBox.Reset();
	SlateContentBox.Reset();
	SlateCharacterName.Reset();
	SlateDialogueText.Reset();
	SlateOptionsBox.Reset();
	SlateContinueIndicator.Reset();
}

void UYarnDialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueWidget: NativeConstruct, UsingSlateUI=%d"), bUsingSlateUI);
}

void UYarnDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsTypewriting)
	{
		TypewriterTimer += InDeltaTime;

		float CharTime = 1.0f / TypewriterSpeed;
		while (TypewriterTimer >= CharTime && CurrentCharIndex < FullDialogueText.Len())
		{
			TypewriterTimer -= CharTime;
			CurrentCharIndex++;
			UpdateTypewriterText();
		}

		if (CurrentCharIndex >= FullDialogueText.Len())
		{
			bIsTypewriting = false;

			// Show continue indicator
			if (bUsingSlateUI && SlateContinueIndicator.IsValid())
			{
				SlateContinueIndicator->SetVisibility(EVisibility::Visible);
			}
			else if (ContinueIndicator)
			{
				ContinueIndicator->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
}

void UYarnDialogueWidget::UpdateTypewriterText()
{
	FString DisplayText = FullDialogueText.Left(CurrentCharIndex);

	if (bUsingSlateUI && SlateDialogueText.IsValid())
	{
		SlateDialogueText->SetText(FText::FromString(DisplayText));
	}
	else if (DialogueText)
	{
		DialogueText->SetText(FText::FromString(DisplayText));
	}
}

FReply UYarnDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (bIsTypewriting)
		{
			SkipTypewriter();
			return FReply::Handled();
		}

		// Check if options are visible
		bool bOptionsVisible = false;
		if (bUsingSlateUI && SlateOptionsBox.IsValid())
		{
			bOptionsVisible = SlateOptionsBox->GetVisibility() == EVisibility::Visible;
		}
		else if (OptionsContainer)
		{
			bOptionsVisible = OptionsContainer->GetVisibility() == ESlateVisibility::Visible;
		}

		if (!bOptionsVisible)
		{
			OnContinue.Broadcast();
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UYarnDialogueWidget::ShowLine(const FString& CharacterName, const FString& InDialogueText, bool bUseTypewriter)
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueWidget::ShowLine - Character='%s' Text='%s'"), *CharacterName, *InDialogueText);

	HideOptions();

	// Show/hide character name
	if (bUsingSlateUI)
	{
		if (SlateCharacterName.IsValid())
		{
			if (!CharacterName.IsEmpty())
			{
				SlateCharacterName->SetText(FText::FromString(CharacterName));
				SlateCharacterName->SetVisibility(EVisibility::Visible);
			}
			else
			{
				SlateCharacterName->SetVisibility(EVisibility::Collapsed);
			}
		}

		if (SlateContinueIndicator.IsValid())
		{
			SlateContinueIndicator->SetVisibility(EVisibility::Collapsed);
		}
	}
	else
	{
		if (CharacterNameText)
		{
			if (!CharacterName.IsEmpty())
			{
				CharacterNameText->SetText(FText::FromString(CharacterName));
				CharacterNameText->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				CharacterNameText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (ContinueIndicator)
		{
			ContinueIndicator->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Set up dialogue text
	FullDialogueText = InDialogueText;

	if (bUseTypewriter && TypewriterSpeed > 0)
	{
		CurrentCharIndex = 0;
		TypewriterTimer = 0.0f;
		bIsTypewriting = true;

		// Clear text
		if (bUsingSlateUI && SlateDialogueText.IsValid())
		{
			SlateDialogueText->SetText(FText::GetEmpty());
		}
		else if (DialogueText)
		{
			DialogueText->SetText(FText::GetEmpty());
		}
	}
	else
	{
		bIsTypewriting = false;
		CurrentCharIndex = FullDialogueText.Len();

		// Show full text
		if (bUsingSlateUI && SlateDialogueText.IsValid())
		{
			SlateDialogueText->SetText(FText::FromString(FullDialogueText));
			if (SlateContinueIndicator.IsValid())
			{
				SlateContinueIndicator->SetVisibility(EVisibility::Visible);
			}
		}
		else if (DialogueText)
		{
			DialogueText->SetText(FText::FromString(FullDialogueText));
			if (ContinueIndicator)
			{
				ContinueIndicator->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
}

FReply UYarnDialogueWidget::OnOptionClicked(int32 Index)
{
	HideOptions();
	OnOptionChosen.Broadcast(Index);
	return FReply::Handled();
}

void UYarnDialogueWidget::ShowOptions(const TArray<FYarnOption>& Options)
{
	CurrentOptions = Options;

	// Hide continue indicator
	if (bUsingSlateUI && SlateContinueIndicator.IsValid())
	{
		SlateContinueIndicator->SetVisibility(EVisibility::Collapsed);
	}
	else if (ContinueIndicator)
	{
		ContinueIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (bUsingSlateUI && SlateOptionsBox.IsValid())
	{
		// Clear existing options
		SlateOptionsBox->ClearChildren();

		// Create button for each option
		for (int32 i = 0; i < Options.Num(); i++)
		{
			const FYarnOption& Option = Options[i];
			FString ButtonText = FString::Printf(TEXT("%d. %s"), i + 1, *Option.Line.Text.ToString());

			// Button colors
			FLinearColor ButtonBgColor = Option.bIsAvailable
				? FLinearColor(0.2f, 0.4f, 0.7f, 1.0f)  // Blue for available
				: FLinearColor(0.3f, 0.3f, 0.3f, 1.0f); // Gray for disabled
			FLinearColor ButtonTextColor = Option.bIsAvailable
				? FLinearColor::White
				: FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);

			SlateOptionsBox->AddSlot()
				.AutoHeight()
				.Padding(0, 6)
				[
					SNew(SButton)
					.IsEnabled(Option.bIsAvailable)
					.OnClicked_Lambda([this, i]() { return OnOptionClicked(i); })
					.ContentPadding(FMargin(20, 12))
					.ButtonColorAndOpacity(FSlateColor(ButtonBgColor))
					.HAlign(HAlign_Fill)
					[
						SNew(STextBlock)
						.Text(FText::FromString(ButtonText))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", DialogueFontSize))
						.ColorAndOpacity(FSlateColor(ButtonTextColor))
						.Justification(ETextJustify::Left)
					]
				];
		}

		SlateOptionsBox->SetVisibility(EVisibility::Visible);
	}
	else if (OptionsContainer)
	{
		// Use UMG widgets - Blueprint will handle this
		OptionsContainer->SetVisibility(ESlateVisibility::Visible);
	}
}

void UYarnDialogueWidget::HideOptions()
{
	if (bUsingSlateUI && SlateOptionsBox.IsValid())
	{
		SlateOptionsBox->SetVisibility(EVisibility::Collapsed);
		SlateOptionsBox->ClearChildren();
	}
	else if (OptionsContainer)
	{
		OptionsContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UYarnDialogueWidget::ShowDialogue()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueWidget::ShowDialogue"));
	SetVisibility(ESlateVisibility::Visible);

	// Set focus to this widget for keyboard input
	SetKeyboardFocus();

	// Show mouse cursor and set UI input mode
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
}

void UYarnDialogueWidget::HideDialogue()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueWidget::HideDialogue"));
	SetVisibility(ESlateVisibility::Collapsed);

	// Restore game input mode
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
	}
}

void UYarnDialogueWidget::SkipTypewriter()
{
	if (bIsTypewriting)
	{
		bIsTypewriting = false;
		CurrentCharIndex = FullDialogueText.Len();

		if (bUsingSlateUI && SlateDialogueText.IsValid())
		{
			SlateDialogueText->SetText(FText::FromString(FullDialogueText));
			if (SlateContinueIndicator.IsValid())
			{
				SlateContinueIndicator->SetVisibility(EVisibility::Visible);
			}
		}
		else if (DialogueText)
		{
			DialogueText->SetText(FText::FromString(FullDialogueText));
			if (ContinueIndicator)
			{
				ContinueIndicator->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
}

FReply UYarnDialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	// Space or Enter to continue/hurry
	if (Key == EKeys::SpaceBar || Key == EKeys::Enter)
	{
		if (bIsTypewriting)
		{
			SkipTypewriter();
			return FReply::Handled();
		}

		// Check if options are visible
		bool bOptionsVisible = false;
		if (bUsingSlateUI && SlateOptionsBox.IsValid())
		{
			bOptionsVisible = SlateOptionsBox->GetVisibility() == EVisibility::Visible;
		}
		else if (OptionsContainer)
		{
			bOptionsVisible = OptionsContainer->GetVisibility() == ESlateVisibility::Visible;
		}

		if (!bOptionsVisible)
		{
			OnContinue.Broadcast();
			return FReply::Handled();
		}
	}

	// Number keys 1-9 for option selection
	if (Key == EKeys::One || Key == EKeys::NumPadOne) { SelectOptionByNumber(0); return FReply::Handled(); }
	if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { SelectOptionByNumber(1); return FReply::Handled(); }
	if (Key == EKeys::Three || Key == EKeys::NumPadThree) { SelectOptionByNumber(2); return FReply::Handled(); }
	if (Key == EKeys::Four || Key == EKeys::NumPadFour) { SelectOptionByNumber(3); return FReply::Handled(); }
	if (Key == EKeys::Five || Key == EKeys::NumPadFive) { SelectOptionByNumber(4); return FReply::Handled(); }
	if (Key == EKeys::Six || Key == EKeys::NumPadSix) { SelectOptionByNumber(5); return FReply::Handled(); }
	if (Key == EKeys::Seven || Key == EKeys::NumPadSeven) { SelectOptionByNumber(6); return FReply::Handled(); }
	if (Key == EKeys::Eight || Key == EKeys::NumPadEight) { SelectOptionByNumber(7); return FReply::Handled(); }
	if (Key == EKeys::Nine || Key == EKeys::NumPadNine) { SelectOptionByNumber(8); return FReply::Handled(); }

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UYarnDialogueWidget::SelectOptionByNumber(int32 Index)
{
	// Check if options are visible
	bool bOptionsVisible = false;
	if (bUsingSlateUI && SlateOptionsBox.IsValid())
	{
		bOptionsVisible = SlateOptionsBox->GetVisibility() == EVisibility::Visible;
	}
	else if (OptionsContainer)
	{
		bOptionsVisible = OptionsContainer->GetVisibility() == ESlateVisibility::Visible;
	}

	if (!bOptionsVisible)
	{
		return;
	}

	// Check if index is valid and option is available
	if (Index >= 0 && Index < CurrentOptions.Num())
	{
		if (CurrentOptions[Index].bIsAvailable)
		{
			OnOptionClicked(Index);
		}
	}
}

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

#include "YarnLinePresenter.h"
#include "YarnDialogueRunner.h"
#include "YarnSpinnerModule.h"

UYarnLinePresenter::UYarnLinePresenter()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UYarnLinePresenter::BeginPlay()
{
	Super::BeginPlay();

	// hide the container at start
	SetLineContainerVisible(false);
}

void UYarnLinePresenter::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsTypewriting)
	{
		UpdateTypewriterText();
	}
	else if (bWaitingForAutoAdvance)
	{
		AutoAdvanceTimer -= DeltaTime;
		if (AutoAdvanceTimer <= 0.0f)
		{
			bWaitingForAutoAdvance = false;
			SetComponentTickEnabled(false);
			OnLinePresentationComplete();
		}
	}
}

void UYarnLinePresenter::OnDialogueStarted_Implementation()
{
	SetLineContainerVisible(false);
}

void UYarnLinePresenter::OnDialogueComplete_Implementation()
{
	SetLineContainerVisible(false);
	bIsTypewriting = false;
	bWaitingForAutoAdvance = false;
	SetComponentTickEnabled(false);
}

void UYarnLinePresenter::RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry)
{
	bIsPresentingLine = true;
	CurrentLine = Line;

	// determine the text to display
	FString TextToDisplay;
	FString CharacterName = Line.CharacterName;

	if (CharacterNameWidget)
	{
		// separate character name widget is configured
		TextToDisplay = Line.Text.ToString();

		if (!CharacterName.IsEmpty())
		{
			CharacterNameWidget->SetText(FText::FromString(CharacterName));
			SetCharacterNameVisible(true);
		}
		else
		{
			SetCharacterNameVisible(false);
		}
	}
	else
	{
		// no separate character name widget
		if (bShowCharacterNameInLine && !CharacterName.IsEmpty())
		{
			TextToDisplay = FString::Printf(TEXT("%s: %s"), *CharacterName, *Line.Text.ToString());
		}
		else
		{
			TextToDisplay = Line.Text.ToString();
		}
		SetCharacterNameVisible(false);
	}

	FullText = TextToDisplay;
	CurrentCharIndex = 0;
	TypewriterTimer = 0.0f;
	bIsTypewriting = false;
	bTypewriterHurried = false;
	bWaitingForAutoAdvance = false;

	// show the container
	SetLineContainerVisible(true);

	// start the typewriter effect if configured
	switch (TypewriterMode)
	{
	case EYarnTypewriterMode::Instant:
		ShowFullText();
		break;

	case EYarnTypewriterMode::ByLetter:
	case EYarnTypewriterMode::ByWord:
		if (LineTextWidget)
		{
			LineTextWidget->SetText(FText::GetEmpty());
		}
		bIsTypewriting = true;
		SetComponentTickEnabled(true);
		break;
	}
}

void UYarnLinePresenter::RunOptions_Implementation(const FYarnOptionSet& Options)
{
	// line presenter doesn't handle options, pass through
}

void UYarnLinePresenter::OnHurryUpRequested_Implementation()
{
	if (bIsTypewriting)
	{
		bTypewriterHurried = true;
		ShowFullText();
	}
}

void UYarnLinePresenter::OnNextLineRequested_Implementation()
{
	if (bIsTypewriting)
	{
		ShowFullText();
	}

	if (bWaitingForAutoAdvance)
	{
		bWaitingForAutoAdvance = false;
		SetComponentTickEnabled(false);
		OnLinePresentationComplete();
	}
	else if (!bIsTypewriting && bIsPresentingLine)
	{
		OnLinePresentationComplete();
	}
}

void UYarnLinePresenter::ShowFullText()
{
	bIsTypewriting = false;

	if (LineTextWidget)
	{
		LineTextWidget->SetText(FText::FromString(FullText));
	}

	OnTypewriterComplete.Broadcast();

	if (bAutoAdvance)
	{
		AutoAdvanceTimer = AutoAdvanceDelay;
		bWaitingForAutoAdvance = true;
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}

void UYarnLinePresenter::UpdateTypewriterText()
{
	if (!LineTextWidget || FullText.IsEmpty())
	{
		ShowFullText();
		return;
	}

	float CharactersPerSecond = 0.0f;
	bool bByWord = false;

	switch (TypewriterMode)
	{
	case EYarnTypewriterMode::ByLetter:
		CharactersPerSecond = static_cast<float>(LettersPerSecond);
		break;

	case EYarnTypewriterMode::ByWord:
		CharactersPerSecond = static_cast<float>(WordsPerSecond);
		bByWord = true;
		break;

	default:
		ShowFullText();
		return;
	}

	if (CharactersPerSecond <= 0.0f)
	{
		ShowFullText();
		return;
	}

	float DeltaTime = GetWorld()->GetDeltaSeconds();
	TypewriterTimer += DeltaTime * CharactersPerSecond;

	while (TypewriterTimer >= 1.0f && CurrentCharIndex < FullText.Len())
	{
		TypewriterTimer -= 1.0f;

		if (bByWord)
		{
			// advance to end of current word
			while (CurrentCharIndex < FullText.Len() && !FChar::IsWhitespace(FullText[CurrentCharIndex]))
			{
				CurrentCharIndex++;
			}
			// skip whitespace after word
			while (CurrentCharIndex < FullText.Len() && FChar::IsWhitespace(FullText[CurrentCharIndex]))
			{
				CurrentCharIndex++;
			}
		}
		else
		{
			CurrentCharIndex++;
		}
	}

	// update displayed text
	FString DisplayedText = FullText.Left(CurrentCharIndex);
	LineTextWidget->SetText(FText::FromString(DisplayedText));

	// check if complete
	if (CurrentCharIndex >= FullText.Len())
	{
		ShowFullText();
	}
}

void UYarnLinePresenter::SetLineContainerVisible(bool bVisible)
{
	if (LineContainer)
	{
		LineContainer->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UYarnLinePresenter::SetCharacterNameVisible(bool bVisible)
{
	if (CharacterNameContainer)
	{
		CharacterNameContainer->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	else if (CharacterNameWidget)
	{
		CharacterNameWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

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

	// set up the action markup registry and auto-register any handler
	// components on the owning actor ([pause/], markup events, sfx)
	ActionMarkupRegistry = NewObject<UYarnActionMarkupHandlerRegistry>(this);
	if (AActor* Owner = GetOwner())
	{
		TArray<UActorComponent*> HandlerComponents = Owner->GetComponentsByInterface(UYarnActionMarkupHandler::StaticClass());
		for (UActorComponent* Component : HandlerComponents)
		{
			TScriptInterface<IYarnActionMarkupHandler> Handler;
			Handler.SetObject(Component);
			Handler.SetInterface(Cast<IYarnActionMarkupHandler>(Component));
			ActionMarkupRegistry->RegisterHandler(Handler);
		}
	}
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
			NotifyLineWillDismiss();
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

	// Always use TextWithoutCharacterName for the body so that custom line
	// providers that rewrite CharacterName (e.g. casting) aren't bypassed by
	// the speaker prefix that Line.Text still carries.
	const FString BodyText = Line.TextWithoutCharacterName.ToString();

	if (CharacterNameWidget)
	{
		// separate character name widget is configured
		TextToDisplay = BodyText;

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
			TextToDisplay = FString::Printf(TEXT("%s: %s"), *CharacterName, *BodyText);
		}
		else
		{
			TextToDisplay = BodyText;
		}
		SetCharacterNameVisible(false);
	}

	FullText = TextToDisplay;
	CurrentCharIndex = 0;
	TypewriterTimer = 0.0f;
	bIsTypewriting = false;
	bTypewriterHurried = false;
	bWaitingForAutoAdvance = false;
	PendingPauseTime = 0.0f;

	// markup attribute positions are relative to the full parsed text, which
	// includes the character name prefix. when we display only the body text,
	// offset displayed-text indices by the prefix length so action markup
	// handlers see positions in markup space.
	MarkupIndexOffset = Line.TextMarkup.Text.Len() - FullText.Len();
	if (MarkupIndexOffset < 0)
	{
		MarkupIndexOffset = 0;
	}

	// let action markup handlers prepare for the line ([pause/] positions etc)
	if (ActionMarkupRegistry)
	{
		ActionMarkupRegistry->DispatchPrepareForLine(Line.TextMarkup);
	}

	// show the container
	SetLineContainerVisible(true);

	// start the typewriter effect if configured
	switch (TypewriterMode)
	{
	case EYarnTypewriterMode::Instant:
		if (ActionMarkupRegistry)
		{
			ActionMarkupRegistry->DispatchLineDisplayBegin(Line.TextMarkup);
		}
		ShowFullText();
		break;

	case EYarnTypewriterMode::ByLetter:
	case EYarnTypewriterMode::ByWord:
		if (LineTextWidget)
		{
			LineTextWidget->SetText(FText::GetEmpty());
		}
		if (ActionMarkupRegistry)
		{
			ActionMarkupRegistry->DispatchLineDisplayBegin(Line.TextMarkup);
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
		NotifyLineWillDismiss();
		OnLinePresentationComplete();
	}
	else if (!bIsTypewriting && bIsPresentingLine)
	{
		NotifyLineWillDismiss();
		OnLinePresentationComplete();
	}
}

void UYarnLinePresenter::NotifyLineWillDismiss()
{
	if (ActionMarkupRegistry)
	{
		ActionMarkupRegistry->DispatchLineWillDismiss();
	}
}

void UYarnLinePresenter::ShowFullText()
{
	const bool bWasTypewriting = bIsTypewriting;
	bIsTypewriting = false;
	PendingPauseTime = 0.0f;

	if (LineTextWidget)
	{
		LineTextWidget->SetText(FText::FromString(FullText));
	}

	// notify action markup handlers that the line has fully displayed
	// (only if we actually presented this line, not on shutdown paths)
	if (ActionMarkupRegistry && (bWasTypewriting || bIsPresentingLine))
	{
		ActionMarkupRegistry->DispatchLineDisplayComplete();
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

	// honour a pause requested by an action markup handler ([pause/]) before
	// revealing any more text
	if (PendingPauseTime > 0.0f)
	{
		PendingPauseTime -= DeltaTime;
		if (PendingPauseTime > 0.0f)
		{
			return;
		}
		PendingPauseTime = 0.0f;
	}

	TypewriterTimer += DeltaTime * CharactersPerSecond;

	// gives action markup handlers a chance to react to each character as it
	// appears. returns true if a handler requested a pause, which stops the
	// reveal until the pause elapses.
	auto DispatchCharacter = [this](int32 DisplayIndex) -> bool
	{
		if (!ActionMarkupRegistry)
		{
			return false;
		}
		FYarnLineCancellationToken Token = DialogueRunner ? DialogueRunner->GetCurrentCancellationToken() : FYarnLineCancellationToken();
		float PauseDuration = ActionMarkupRegistry->DispatchCharacterWillAppear(
			MarkupIndexOffset + DisplayIndex, CurrentLine.TextMarkup, Token);
		if (PauseDuration > 0.0f)
		{
			PendingPauseTime = PauseDuration;
			return true;
		}
		return false;
	};

	bool bPaused = false;
	while (!bPaused && TypewriterTimer >= 1.0f && CurrentCharIndex < FullText.Len())
	{
		TypewriterTimer -= 1.0f;

		if (bByWord)
		{
			// advance to end of current word, dispatching each character
			while (CurrentCharIndex < FullText.Len() && !FChar::IsWhitespace(FullText[CurrentCharIndex]))
			{
				if (DispatchCharacter(CurrentCharIndex))
				{
					bPaused = true;
					break;
				}
				CurrentCharIndex++;
			}
			// skip whitespace after word
			while (!bPaused && CurrentCharIndex < FullText.Len() && FChar::IsWhitespace(FullText[CurrentCharIndex]))
			{
				CurrentCharIndex++;
			}
		}
		else
		{
			if (DispatchCharacter(CurrentCharIndex))
			{
				bPaused = true;
				break;
			}
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

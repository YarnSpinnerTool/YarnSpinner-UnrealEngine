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

#include "YarnDialoguePresenter.h"
#include "YarnDialogueRunner.h"
#include "TimerManager.h"
#include "Engine/World.h"

UYarnDialoguePresenter::UYarnDialoguePresenter()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UYarnDialoguePresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear any pending auto-advance timer to prevent callbacks after destruction
	CancelAutoAdvanceTimer();

	Super::EndPlay(EndPlayReason);
}

void UYarnDialoguePresenter::OnDialogueStarted_Implementation()
{
	// Default implementation does nothing
}

void UYarnDialoguePresenter::OnDialogueComplete_Implementation()
{
	// Default implementation does nothing
}

void UYarnDialoguePresenter::OnNodeEnter_Implementation(const FString& NodeName)
{
	// Default implementation does nothing
}

void UYarnDialoguePresenter::OnNodeExit_Implementation(const FString& NodeName)
{
	// Default implementation does nothing
}

void UYarnDialoguePresenter::OnPrepareForLines_Implementation(const TArray<FString>& LineIDs)
{
	// Default implementation does nothing
	// Override in subclasses to pre-load audio, localisation, etc.
}

void UYarnDialoguePresenter::RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry)
{
	// Default implementation immediately completes
	OnLinePresentationComplete();
}

void UYarnDialoguePresenter::RunOptions_Implementation(const FYarnOptionSet& Options)
{
	// Default implementation does nothing - subclasses must override
	// to present options and call OnOptionSelected
}

void UYarnDialoguePresenter::OnHurryUpRequested_Implementation()
{
	// Default implementation does nothing
}

void UYarnDialoguePresenter::OnNextLineRequested_Implementation()
{
	// Default implementation completes line presentation
	if (bIsPresentingLine)
	{
		OnLinePresentationComplete();
	}
}

void UYarnDialoguePresenter::OnOptionsHurryUpRequested_Implementation()
{
	// Default implementation does nothing
	// Override in subclasses to speed up option presentation animations
}

void UYarnDialoguePresenter::SetDialogueRunner(UYarnDialogueRunner* Runner)
{
	DialogueRunner = Runner;
}

void UYarnDialoguePresenter::Internal_RunLine(const FYarnLocalizedLine& Line, bool bCanHurry)
{
	bIsPresentingLine = true;
	CurrentLine = Line;
	RunLine(Line, bCanHurry);
}

void UYarnDialoguePresenter::Internal_RunOptions(const FYarnOptionSet& Options)
{
	bIsPresentingOptions = true;
	CurrentOptions = Options;
	RunOptions(Options);
}

void UYarnDialoguePresenter::OnLinePresentationComplete()
{
	bIsPresentingLine = false;

	// The runner waits for ALL presenters to complete before continuing.
	// NotifyPresenterLineComplete tracks the count and only calls
	// Continue() when all presenters have finished.
	if (DialogueRunner)
	{
		DialogueRunner->NotifyPresenterLineComplete();
	}
}

void UYarnDialoguePresenter::OnOptionSelected(int32 OptionIndex)
{
	bIsPresentingOptions = false;

	if (DialogueRunner)
	{
		DialogueRunner->SelectOption(OptionIndex);
	}
}

void UYarnDialoguePresenter::RequestContinue()
{
	if (DialogueRunner)
	{
		DialogueRunner->Continue();
	}
}

bool UYarnDialoguePresenter::IsHurryUpRequested() const
{
	if (DialogueRunner)
	{
		return DialogueRunner->IsHurryUpRequested();
	}
	return false;
}

bool UYarnDialoguePresenter::IsNextContentRequested() const
{
	if (DialogueRunner)
	{
		return DialogueRunner->IsNextContentRequested();
	}
	return false;
}

bool UYarnDialoguePresenter::IsOptionHurryUpRequested() const
{
	if (DialogueRunner)
	{
		return DialogueRunner->IsOptionHurryUpRequested();
	}
	return false;
}

bool UYarnDialoguePresenter::IsOptionNextContentRequested() const
{
	if (DialogueRunner)
	{
		return DialogueRunner->IsOptionNextContentRequested();
	}
	return false;
}

// UYarnLineProvider implementation

void UYarnLineProvider::SetYarnProject(UYarnProject* Project)
{
	YarnProject = Project;
}

FYarnLocalizedLine UYarnLineProvider::GetLocalizedLine_Implementation(const FYarnLine& Line)
{
	FYarnLocalizedLine LocalizedLine;
	LocalizedLine.RawLine = Line;

	if (YarnProject)
	{
		FString BaseText = YarnProject->GetBaseText(Line.LineID);

		BaseText = FYarnVirtualMachine::ExpandSubstitutions(BaseText, Line.Substitutions);

		// Parse markup: handles [tags], escape sequences, character names,
		// select/plural/ordinal, whitespace trimming, etc.
		FYarnMarkupParseResult ParseResult = UYarnMarkupLibrary::ParseMarkupFull(
			BaseText,
			TEXT("en"),
			true // add implicit character attribute
		);

		LocalizedLine.TextMarkup = ParseResult;

		LocalizedLine.Text = FText::FromString(ParseResult.Text);
		LocalizedLine.CharacterName = ParseResult.CharacterName;
		LocalizedLine.TextWithoutCharacterName = FText::FromString(ParseResult.TextWithoutCharacterName);
	}
	else
	{
		LocalizedLine.Text = FText::FromString(Line.LineID);
	}

	return LocalizedLine;
}

// Auto-advance implementation

void UYarnDialoguePresenter::SetAutoAdvanceEnabled(bool bEnabled)
{
	bAutoAdvanceEnabled = bEnabled;

	// Cancel any pending timer if disabling
	if (!bEnabled)
	{
		CancelAutoAdvanceTimer();
	}
}

float UYarnDialoguePresenter::CalculateAutoAdvanceDelay(int32 CharacterCount) const
{
	float Delay = AutoAdvanceMinDelay + (CharacterCount * AutoAdvanceTimePerCharacter);

	// Clamp to max if set
	if (AutoAdvanceMaxDelay > 0.0f)
	{
		Delay = FMath::Min(Delay, AutoAdvanceMaxDelay);
	}

	return Delay;
}

void UYarnDialoguePresenter::StartAutoAdvanceTimer()
{
	if (!bAutoAdvanceEnabled)
	{
		return;
	}

	CancelAutoAdvanceTimer();

	int32 CharCount = CurrentLine.Text.ToString().Len();
	float Delay = CalculateAutoAdvanceDelay(CharCount);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AutoAdvanceTimerHandle,
			this,
			&UYarnDialoguePresenter::OnAutoAdvanceTimerFired,
			Delay,
			false
		);
	}
}

void UYarnDialoguePresenter::CancelAutoAdvanceTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}
}

void UYarnDialoguePresenter::OnAutoAdvanceTimerFired()
{
	// Only advance if we're still presenting a line
	if (bIsPresentingLine && DialogueRunner)
	{
		DialogueRunner->RequestNextLine();
	}
}

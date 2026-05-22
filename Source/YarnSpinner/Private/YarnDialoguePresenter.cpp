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

void UYarnDialoguePresenter::Internal_RunLine(const FYarnLocalizedLine& Line,
                                              bool bCanHurry,
                                              const FYarnLineCancellationToken& Token,
                                              const FOnYarnLineFinished& OnFinished)
{
	bIsPresentingLine = true;
	CurrentLine = Line;

	// Stash the token and callback for the duration of this line. The
	// BlueprintNativeEvent RunLine doesn't carry them in its parameter
	// list (so existing Blueprint subclasses keep compiling), so the
	// convenience helpers read them from here when the presenter says
	// it's done.
	CurrentLineCancellationToken = Token;
	CurrentLineFinishedCallback = OnFinished;

	RunLine(Line, bCanHurry);
}

void UYarnDialoguePresenter::Internal_RunOptions(const FYarnOptionSet& Options,
                                                 const FYarnLineCancellationToken& Token,
                                                 const FOnYarnOptionSelected& OnSelected)
{
	bIsPresentingOptions = true;
	CurrentOptions = Options;
	CurrentOptionsCancellationToken = Token;
	CurrentOptionSelectedCallback = OnSelected;
	RunOptions(Options);
}

void UYarnDialoguePresenter::OnLinePresentationComplete()
{
	bIsPresentingLine = false;

	// New flow: fire the callback that whoever called us handed in.
	// Take a local copy and clear the stored callback first, so a
	// re-entrant call (or a callback that calls back into us) won't
	// fire it a second time.
	if (CurrentLineFinishedCallback.IsBound())
	{
		FOnYarnLineFinished Cb = CurrentLineFinishedCallback;
		CurrentLineFinishedCallback.Unbind();
		CurrentLineCancellationToken = FYarnLineCancellationToken();
		Cb.ExecuteIfBound(EYarnLineCompletionRequest::None);
		return;
	}

	// Legacy fallback: presenter was started by some path that didn't
	// hand in a callback (typically code that bypassed Internal_RunLine).
	// Talk to the runner directly through the back-pointer.
	if (DialogueRunner)
	{
		DialogueRunner->NotifyPresenterLineComplete();
	}
}

void UYarnDialoguePresenter::OnLinePresentationCompleteAndEndLine()
{
	bIsPresentingLine = false;

	if (CurrentLineFinishedCallback.IsBound())
	{
		FOnYarnLineFinished Cb = CurrentLineFinishedCallback;
		CurrentLineFinishedCallback.Unbind();
		CurrentLineCancellationToken = FYarnLineCancellationToken();
		Cb.ExecuteIfBound(EYarnLineCompletionRequest::EndLine);
		return;
	}

	// Legacy fallback. The old API has no way to express "end this line";
	// the closest thing is asking the runner to advance, then signalling
	// our own completion. Not behaviourally identical to the new path
	// (siblings see this as a player skip rather than a line driver) but
	// it's the best we can do without a callback.
	if (DialogueRunner)
	{
		DialogueRunner->RequestNextLine();
		DialogueRunner->NotifyPresenterLineComplete();
	}
}

void UYarnDialoguePresenter::OnOptionSelected(int32 OptionIndex)
{
	bIsPresentingOptions = false;

	if (CurrentOptionSelectedCallback.IsBound())
	{
		FOnYarnOptionSelected Cb = CurrentOptionSelectedCallback;
		CurrentOptionSelectedCallback.Unbind();
		CurrentOptionsCancellationToken = FYarnLineCancellationToken();
		Cb.ExecuteIfBound(OptionIndex);
		return;
	}

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
	// Prefer the stored token. It points to whichever cancellation source
	// is actually governing this presenter's current line - the runner's
	// content source in the common case, or a wrapper's linked source
	// when this presenter is being driven by a wrapper. The token is the
	// authoritative answer because the wrapper might have cancelled its
	// own source without the runner knowing.
	if (CurrentLineCancellationToken.CanBeCancelled())
	{
		return CurrentLineCancellationToken.IsHurryUpRequested();
	}
	// Legacy fallback for code paths that didn't go through Internal_RunLine.
	if (DialogueRunner)
	{
		return DialogueRunner->IsHurryUpRequested();
	}
	return false;
}

bool UYarnDialoguePresenter::IsNextContentRequested() const
{
	if (CurrentLineCancellationToken.CanBeCancelled())
	{
		return CurrentLineCancellationToken.IsCancellationRequested();
	}
	if (DialogueRunner)
	{
		return DialogueRunner->IsNextContentRequested();
	}
	return false;
}

bool UYarnDialoguePresenter::IsOptionHurryUpRequested() const
{
	if (CurrentOptionsCancellationToken.CanBeCancelled())
	{
		return CurrentOptionsCancellationToken.IsHurryUpRequested();
	}
	if (DialogueRunner)
	{
		return DialogueRunner->IsOptionHurryUpRequested();
	}
	return false;
}

bool UYarnDialoguePresenter::IsOptionNextContentRequested() const
{
	if (CurrentOptionsCancellationToken.CanBeCancelled())
	{
		return CurrentOptionsCancellationToken.IsCancellationRequested();
	}
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

	// Use TextWithoutCharacterName so the auto-advance delay reflects only
	// the body the player is reading, not the speaker prefix.
	int32 CharCount = CurrentLine.TextWithoutCharacterName.ToString().Len();
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
	// Auto-advance is the presenter saying "I've shown this for long
	// enough; the line should move on." That's exactly the EndLine
	// semantic. Going through the new completion path means a wrapping
	// presenter sees this as its child finishing-with-end-line-request,
	// rather than bypassing the wrapper via the runner back-pointer.
	if (bIsPresentingLine)
	{
		OnLinePresentationCompleteAndEndLine();
	}
}

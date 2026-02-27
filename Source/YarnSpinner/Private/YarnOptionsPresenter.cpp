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

#include "YarnOptionsPresenter.h"
#include "YarnDialogueRunner.h"
#include "YarnSpinnerModule.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/PanelSlot.h"
#include "EngineUtils.h" // for TActorIterator

// ============================================================================
// Static constants
// ============================================================================

const FString UYarnOptionsPresenter::TruncateLastLineMarkupName = TEXT("lastline");

// ============================================================================
// UYarnOptionWidget Implementation
// ============================================================================

void UYarnOptionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (OptionButton)
	{
		OptionButton->OnClicked.AddDynamic(this, &UYarnOptionWidget::HandleButtonClicked);
	}

	OwningPresenter = FindOwningPresenter();
}

void UYarnOptionWidget::SetupOption_Implementation(const FYarnOption& InOption, int32 InIndex)
{
	OptionIndex = InIndex;
	bIsAvailable = InOption.bIsAvailable;
	bIsSelected = false;

	if (OptionText)
	{
		// Use text without character name for cleaner option display
		OptionText->SetText(InOption.Line.TextWithoutCharacterName);
	}

	if (OptionButton)
	{
		OptionButton->SetIsEnabled(bIsAvailable);
	}
}

void UYarnOptionWidget::SetOptionUnavailable_Implementation()
{
	bIsAvailable = false;

	if (OptionButton)
	{
		OptionButton->SetIsEnabled(false);
	}
}

void UYarnOptionWidget::OnOptionSelected_Implementation()
{
	bIsSelected = true;

	// Default implementation: give focus to the button
	if (OptionButton && bIsAvailable)
	{
		OptionButton->SetKeyboardFocus();
	}
}

void UYarnOptionWidget::OnOptionDeselected_Implementation()
{
	bIsSelected = false;
}

void UYarnOptionWidget::SubmitOption()
{
	if (bIsAvailable)
	{
		HandleButtonClicked();
	}
}

void UYarnOptionWidget::HandleButtonClicked()
{
	if (!OwningPresenter)
	{
		OwningPresenter = FindOwningPresenter();
	}

	if (OwningPresenter)
	{
		OwningPresenter->HandleOptionSelected(OptionIndex);
	}
	else
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnOptionWidget: Could not find owning YarnOptionsPresenter"));
	}
}

UYarnOptionsPresenter* UYarnOptionWidget::FindOwningPresenter()
{
	UObject* Current = GetOuter();
	while (Current)
	{
		if (UYarnOptionsPresenter* FoundPresenter = Cast<UYarnOptionsPresenter>(Current))
		{
			return FoundPresenter;
		}
		Current = Current->GetOuter();
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UYarnOptionsPresenter* ActorPresenter = Pawn->FindComponentByClass<UYarnOptionsPresenter>())
			{
				return ActorPresenter;
			}
		}
	}

	// Fallback: search all actors in the world
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UYarnOptionsPresenter* ActorPresenter = It->FindComponentByClass<UYarnOptionsPresenter>())
			{
				return ActorPresenter;
			}
		}
	}

	return nullptr;
}

// ============================================================================
// UYarnOptionsPresenter Implementation
// ============================================================================

UYarnOptionsPresenter::UYarnOptionsPresenter()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bHasLastLine = false;
	bOptionsVisible = false;
	bIsFading = false;
	bFadingIn = true;
	FadeProgress = 0.0f;
	SelectedOptionIndex = -1;
	bHasSubmittedSelection = false;
}

void UYarnOptionsPresenter::BeginPlay()
{
	Super::BeginPlay();

	SetOptionsContainerVisible(false);
	if (FadeWidget)
	{
		SetFadeOpacity(0.0f);
	}
}

void UYarnOptionsPresenter::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsFading)
	{
		UpdateFade(DeltaTime);
	}

	if (bOptionsVisible && !bIsFading && bEnableKeyboardNavigation && !bHasSubmittedSelection)
	{
		HandleKeyboardInput();
	}
}

void UYarnOptionsPresenter::OnDialogueStarted_Implementation()
{
	ClearOptions();
	SetOptionsContainerVisible(false);
	bHasLastLine = false;
	bOptionsVisible = false;
	bHasSubmittedSelection = false;
	SelectedOptionIndex = -1;

	if (FadeWidget)
	{
		SetFadeOpacity(0.0f);
	}
}

void UYarnOptionsPresenter::OnDialogueComplete_Implementation()
{
	ClearOptions();
	SetOptionsContainerVisible(false);
	bOptionsVisible = false;
	bIsFading = false;
	SetComponentTickEnabled(false);

	if (FadeWidget)
	{
		SetFadeOpacity(0.0f);
	}

	OnOptionsDismissed.Broadcast();
}

void UYarnOptionsPresenter::RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry)
{
	// Store the line in case we need to show it with options
	if (bShowLastLine)
	{
		LastSeenLine = Line;
		bHasLastLine = true;
	}

	// Options presenter doesn't handle lines directly - immediately complete.
	// This allows another presenter (like LinePresenter) to handle the actual display.
}

void UYarnOptionsPresenter::RunOptions_Implementation(const FYarnOptionSet& Options)
{
	bIsPresentingOptions = true;
	CurrentOptions = Options;
	bHasSubmittedSelection = false;

	bool bAnyAvailable = false;
	for (const FYarnOption& Option : Options.Options)
	{
		if (Option.bIsAvailable)
		{
			bAnyAvailable = true;
			break;
		}
	}

	if (!bAnyAvailable)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnOptionsPresenter: All options are unavailable"));
		bIsPresentingOptions = false;
		return;
	}

	ClearOptions();
	ActiveOptionWidgets.Empty();

	if (!OptionWidgetClass)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnOptionsPresenter: No OptionWidgetClass set"));
		return;
	}

	if (!OptionsContainer)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnOptionsPresenter: No OptionsContainer set"));
		return;
	}

	if (bShowLastLine && bHasLastLine)
	{
		FString ProcessedText = ProcessLastLineText(LastSeenLine);

		if (LastLineCharacterNameWidget && LastLineCharacterNameContainer)
		{
			if (!LastSeenLine.CharacterName.IsEmpty())
			{
				LastLineCharacterNameWidget->SetText(FText::FromString(LastSeenLine.CharacterName));
				LastLineCharacterNameContainer->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				LastLineCharacterNameContainer->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (LastLineTextWidget)
		{
			LastLineTextWidget->SetText(FText::FromString(ProcessedText));
		}

		if (LastLineContainer)
		{
			LastLineContainer->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (LastLineContainer)
	{
		LastLineContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	int32 WidgetIndex = 0;
	for (int32 OptionIdx = 0; OptionIdx < Options.Options.Num(); OptionIdx++)
	{
		const FYarnOption& Option = Options.Options[OptionIdx];

		if (!Option.bIsAvailable && !bShowUnavailableOptions)
		{
			continue;
		}

		UYarnOptionWidget* Widget = nullptr;

		if (WidgetIndex < OptionWidgetPool.Num())
		{
			Widget = OptionWidgetPool[WidgetIndex];
		}

		if (!Widget)
		{
			Widget = CreateOptionWidget();
			if (Widget)
			{
				OptionWidgetPool.Add(Widget);
			}
		}

		if (Widget)
		{
			FYarnOption DisplayOption = Option;
			if (!Option.bIsAvailable && bStrikethroughUnavailable)
			{
				FString StrikethroughText = FString::Printf(TEXT("<s>%s</s>"), *Option.Line.TextWithoutCharacterName.ToString());
				DisplayOption.Line.TextWithoutCharacterName = FText::FromString(StrikethroughText);
			}

			Widget->SetupOption(DisplayOption, OptionIdx);
			Widget->SetVisibility(ESlateVisibility::Visible);

			if (!Option.bIsAvailable)
			{
				Widget->SetOptionUnavailable();
			}

			ActiveOptionWidgets.Add(Widget);
		}

		WidgetIndex++;
	}

	SelectedOptionIndex = FindFirstAvailableOptionIndex();

	SetOptionsContainerVisible(true);

	if (bUseFadeEffect && FadeWidget)
	{
		StartFadeIn();
	}
	else
	{
		bOptionsVisible = true;
		UpdateSelectionVisuals();
		OnOptionsDisplayComplete.Broadcast();
	}

	SetComponentTickEnabled(true);
}

void UYarnOptionsPresenter::OnHurryUpRequested_Implementation()
{
	if (bIsFading && bFadingIn)
	{
		FadeProgress = 1.0f;
		SetFadeOpacity(1.0f);
		OnFadeComplete();
	}
}

void UYarnOptionsPresenter::HandleOptionSelected(int32 OptionIndex)
{
	if (!bIsPresentingOptions || bHasSubmittedSelection)
	{
		return;
	}

	if (OptionIndex < 0 || OptionIndex >= CurrentOptions.Options.Num())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnOptionsPresenter: Invalid option index %d"), OptionIndex);
		return;
	}

	if (!CurrentOptions.Options[OptionIndex].bIsAvailable)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnOptionsPresenter: Selected unavailable option %d"), OptionIndex);
		return;
	}

	bHasSubmittedSelection = true;
	bIsPresentingOptions = false;

	OnOptionSelectedBP.Broadcast(OptionIndex);

	if (bUseFadeEffect && FadeWidget)
	{
		StartFadeOut();

		if (DialogueRunner)
		{
			DialogueRunner->SelectOption(OptionIndex);
		}
	}
	else
	{
		ClearOptions();
		SetOptionsContainerVisible(false);
		bOptionsVisible = false;
		SetComponentTickEnabled(false);

		if (DialogueRunner)
		{
			DialogueRunner->SelectOption(OptionIndex);
		}

		OnOptionsDismissed.Broadcast();
	}
}

void UYarnOptionsPresenter::SelectOptionByIndex(int32 OptionIndex)
{
	if (!bOptionsVisible || bHasSubmittedSelection)
	{
		return;
	}

	if (OptionIndex < 0 || OptionIndex >= CurrentOptions.Options.Num())
	{
		return;
	}

	if (!CurrentOptions.Options[OptionIndex].bIsAvailable)
	{
		return;
	}

	SelectedOptionIndex = OptionIndex;
	UpdateSelectionVisuals();
}

void UYarnOptionsPresenter::SelectNextOption()
{
	if (!bOptionsVisible || bHasSubmittedSelection)
	{
		return;
	}

	int32 NextIndex = FindNextAvailableOptionIndex(SelectedOptionIndex, true);
	if (NextIndex != -1 && NextIndex != SelectedOptionIndex)
	{
		SelectedOptionIndex = NextIndex;
		UpdateSelectionVisuals();
	}
}

void UYarnOptionsPresenter::SelectPreviousOption()
{
	if (!bOptionsVisible || bHasSubmittedSelection)
	{
		return;
	}

	int32 PrevIndex = FindNextAvailableOptionIndex(SelectedOptionIndex, false);
	if (PrevIndex != -1 && PrevIndex != SelectedOptionIndex)
	{
		SelectedOptionIndex = PrevIndex;
		UpdateSelectionVisuals();
	}
}

void UYarnOptionsPresenter::ConfirmSelectedOption()
{
	if (!bOptionsVisible || bHasSubmittedSelection || SelectedOptionIndex == -1)
	{
		return;
	}

	HandleOptionSelected(SelectedOptionIndex);
}

UYarnOptionWidget* UYarnOptionsPresenter::CreateOptionWidget()
{
	if (!OptionWidgetClass || !OptionsContainer)
	{
		return nullptr;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnOptionsPresenter: No player controller found"));
		return nullptr;
	}

	UYarnOptionWidget* Widget = CreateWidget<UYarnOptionWidget>(PC, OptionWidgetClass);
	if (Widget)
	{
		OptionsContainer->AddChild(Widget);
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}

	return Widget;
}

void UYarnOptionsPresenter::ClearOptions()
{
	for (UYarnOptionWidget* Widget : OptionWidgetPool)
	{
		if (Widget)
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	ActiveOptionWidgets.Empty();
	SelectedOptionIndex = -1;
}

void UYarnOptionsPresenter::SetOptionsContainerVisible(bool bVisible)
{
	if (OptionsContainer)
	{
		OptionsContainer->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UYarnOptionsPresenter::SetFadeOpacity(float Opacity)
{
	if (FadeWidget)
	{
		FadeWidget->SetRenderOpacity(FMath::Clamp(Opacity, 0.0f, 1.0f));
	}
}

void UYarnOptionsPresenter::StartFadeIn()
{
	bIsFading = true;
	bFadingIn = true;
	FadeProgress = 0.0f;
	SetFadeOpacity(0.0f);
	bOptionsVisible = false;
}

void UYarnOptionsPresenter::StartFadeOut()
{
	bIsFading = true;
	bFadingIn = false;
	FadeProgress = 0.0f;
	bOptionsVisible = false;
}

void UYarnOptionsPresenter::OnFadeComplete()
{
	bIsFading = false;

	if (bFadingIn)
	{
		bOptionsVisible = true;
		UpdateSelectionVisuals();
		OnOptionsDisplayComplete.Broadcast();
	}
	else
	{
		ClearOptions();
		SetOptionsContainerVisible(false);
		SetComponentTickEnabled(false);
		OnOptionsDismissed.Broadcast();
	}
}

void UYarnOptionsPresenter::UpdateFade(float DeltaTime)
{
	float Duration = bFadingIn ? FadeInDuration : FadeOutDuration;

	if (Duration <= 0.0f)
	{
		FadeProgress = 1.0f;
	}
	else
	{
		FadeProgress += DeltaTime / Duration;
	}

	if (FadeProgress >= 1.0f)
	{
		FadeProgress = 1.0f;
		float FinalOpacity = bFadingIn ? 1.0f : 0.0f;
		SetFadeOpacity(FinalOpacity);
		OnFadeComplete();
	}
	else
	{
		float Opacity;
		if (bFadingIn)
		{
			Opacity = FMath::InterpEaseOut(0.0f, 1.0f, FadeProgress, 2.0f);
		}
		else
		{
			Opacity = FMath::InterpEaseIn(1.0f, 0.0f, FadeProgress, 2.0f);
		}
		SetFadeOpacity(Opacity);
	}
}

void UYarnOptionsPresenter::HandleKeyboardInput()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	if (PC->WasInputKeyJustPressed(NavigateUpKey) || PC->WasInputKeyJustPressed(NavigateUpKeyAlt))
	{
		SelectPreviousOption();
	}

	if (PC->WasInputKeyJustPressed(NavigateDownKey) || PC->WasInputKeyJustPressed(NavigateDownKeyAlt))
	{
		SelectNextOption();
	}

	if (PC->WasInputKeyJustPressed(ConfirmKey) || PC->WasInputKeyJustPressed(ConfirmKeyAlt))
	{
		ConfirmSelectedOption();
	}
}

int32 UYarnOptionsPresenter::FindFirstAvailableOptionIndex() const
{
	for (int32 i = 0; i < CurrentOptions.Options.Num(); i++)
	{
		if (CurrentOptions.Options[i].bIsAvailable)
		{
			return i;
		}
	}
	return -1;
}

int32 UYarnOptionsPresenter::FindNextAvailableOptionIndex(int32 FromIndex, bool bSearchForward) const
{
	if (CurrentOptions.Options.Num() == 0)
	{
		return -1;
	}

	int32 NumOptions = CurrentOptions.Options.Num();
	int32 CurrentIndex = FromIndex;

	for (int32 i = 0; i < NumOptions; i++)
	{
		if (bSearchForward)
		{
			CurrentIndex = (CurrentIndex + 1) % NumOptions;
		}
		else
		{
			CurrentIndex = (CurrentIndex - 1 + NumOptions) % NumOptions;
		}

		if (CurrentOptions.Options[CurrentIndex].bIsAvailable)
		{
			return CurrentIndex;
		}
	}

	// No available option found, return current if it's available
	if (FromIndex >= 0 && FromIndex < NumOptions && CurrentOptions.Options[FromIndex].bIsAvailable)
	{
		return FromIndex;
	}

	return -1;
}

void UYarnOptionsPresenter::UpdateSelectionVisuals()
{
	// ActiveOptionWidgets may not be 1:1 with Options if unavailable options are hidden
	TMap<int32, UYarnOptionWidget*> IndexToWidget;
	for (UYarnOptionWidget* Widget : ActiveOptionWidgets)
	{
		if (Widget)
		{
			IndexToWidget.Add(Widget->OptionIndex, Widget);
		}
	}

	for (auto& Pair : IndexToWidget)
	{
		UYarnOptionWidget* Widget = Pair.Value;
		if (Pair.Key == SelectedOptionIndex)
		{
			Widget->OnOptionSelected();
		}
		else
		{
			Widget->OnOptionDeselected();
		}
	}
}

FString UYarnOptionsPresenter::ProcessLastLineText(const FYarnLocalizedLine& Line) const
{
	FString LineText;

	// If we have a separate character name widget, use text without character name
	if (LastLineCharacterNameWidget)
	{
		LineText = Line.TextWithoutCharacterName.ToString();
	}
	else
	{
		// Include character name in the text if present
		if (!Line.CharacterName.IsEmpty())
		{
			LineText = FString::Printf(TEXT("%s: %s"), *Line.CharacterName, *Line.TextWithoutCharacterName.ToString());
		}
		else
		{
			LineText = Line.Text.ToString();
		}
	}

	return LineText;
}

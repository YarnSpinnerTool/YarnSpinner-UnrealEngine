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

#include "YarnWidgetPresenter.h"
#include "YarnDialogueWidget.h"
#include "YarnDialogueRunner.h"
#include "YarnSpinnerModule.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

UYarnWidgetPresenter::UYarnWidgetPresenter()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UYarnWidgetPresenter::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: BeginPlay"));
	// Widget is created lazily in OnDialogueStarted to handle AutoStart timing
}

void UYarnWidgetPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DialogueWidget)
	{
		DialogueWidget->RemoveFromParent();
		DialogueWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UYarnWidgetPresenter::CreateWidget()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: CreateWidget called"));

	if (DialogueWidget)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: Widget already exists"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnWidgetPresenter: No world!"));
		return;
	}

	// Get the player controller for proper widget ownership
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnWidgetPresenter: No player controller!"));
		return;
	}

	// Use custom class or default
	TSubclassOf<UYarnDialogueWidget> ClassToUse = WidgetClass;
	if (!ClassToUse)
	{
		ClassToUse = UYarnDialogueWidget::StaticClass();
	}

	UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: Creating widget of class %s"), *ClassToUse->GetName());

	UUserWidget* CreatedWidget = ::CreateWidget<UUserWidget>(PC, ClassToUse);
	DialogueWidget = Cast<UYarnDialogueWidget>(CreatedWidget);
	if (DialogueWidget)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: Widget created successfully"));

		// Apply appearance settings
		DialogueWidget->TypewriterSpeed = TypewriterSpeed;
		DialogueWidget->BackgroundColor = BackgroundColor;
		DialogueWidget->TextColor = TextColor;
		DialogueWidget->CharacterNameColor = CharacterNameColor;

		// Bind events
		DialogueWidget->OnContinue.AddDynamic(this, &UYarnWidgetPresenter::HandleWidgetContinue);
		DialogueWidget->OnOptionChosen.AddDynamic(this, &UYarnWidgetPresenter::HandleOptionChosen);

		// Add to viewport but keep hidden until dialogue starts
		DialogueWidget->AddToViewport(100);
		DialogueWidget->HideDialogue();
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: Widget added to viewport (hidden)"));
	}
	else
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnWidgetPresenter: Failed to create widget!"));
	}
}

void UYarnWidgetPresenter::OnDialogueStarted_Implementation()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: OnDialogueStarted"));

	// Create widget on-demand when dialogue starts (handles AutoStart timing)
	if (!DialogueWidget)
	{
		CreateWidget();
	}

	if (DialogueWidget)
	{
		DialogueWidget->ShowDialogue();
	}
}

void UYarnWidgetPresenter::OnDialogueComplete_Implementation()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: OnDialogueComplete"));
	if (DialogueWidget)
	{
		DialogueWidget->HideDialogue();
	}
}

void UYarnWidgetPresenter::RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry)
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnWidgetPresenter: RunLine Character='%s' Text='%s'"),
		*Line.CharacterName, *Line.Text.ToString());

	if (DialogueWidget)
	{
		DialogueWidget->ShowLine(Line.CharacterName, Line.Text.ToString(), TypewriterSpeed > 0);
	}
	else
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnWidgetPresenter: No dialogue widget!"));
	}
}

void UYarnWidgetPresenter::RunOptions_Implementation(const FYarnOptionSet& Options)
{
	if (DialogueWidget)
	{
		DialogueWidget->ShowOptions(Options.Options);
	}
}

void UYarnWidgetPresenter::OnHurryUpRequested_Implementation()
{
	if (DialogueWidget)
	{
		DialogueWidget->SkipTypewriter();
	}
}

void UYarnWidgetPresenter::HandleWidgetContinue()
{
	OnLinePresentationComplete();
}

void UYarnWidgetPresenter::HandleOptionChosen(int32 OptionIndex)
{
	OnOptionSelected(OptionIndex);
}

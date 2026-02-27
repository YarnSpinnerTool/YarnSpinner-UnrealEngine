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

#include "YarnInputHandler.h"
#include "YarnDialogueRunner.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UYarnInputHandler::UYarnInputHandler()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UYarnInputHandler::BeginPlay()
{
	Super::BeginPlay();

	// try to find dialogue runner if not set
	if (!DialogueRunner)
	{
		DialogueRunner = GetOwner()->FindComponentByClass<UYarnDialogueRunner>();
	}
}

void UYarnInputHandler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UYarnInputHandler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInputEnabled || !DialogueRunner || !DialogueRunner->IsDialogueRunning())
	{
		return;
	}

	// update cooldown
	TimeSinceLastInput += DeltaTime;

	// clean up old rapid press times
	float CurrentTime = GetWorld()->GetTimeSeconds();
	float Window = RapidPressWindow;
	RecentPressTimes.RemoveAll([CurrentTime, Window](const float& Time) -> bool
	{
		return (CurrentTime - Time) > Window;
	});

	// handle input based on mode
	if (InputMode == EYarnInputMode::LegacyKeyCode)
	{
		bool bAdvancePressed = IsAdvanceKeyPressed();
		bool bCancelPressed = IsCancelKeyPressed();

		// detect key down (not held)
		if (bAdvancePressed && !bAdvanceKeyWasPressed)
		{
			ProcessAdvanceInput();
		}

		if (bCancelPressed && !bCancelKeyWasPressed)
		{
			ProcessCancelInput();
		}

		bAdvanceKeyWasPressed = bAdvancePressed;
		bCancelKeyWasPressed = bCancelPressed;
	}
	// enhanced input is handled via input action bindings
}

void UYarnInputHandler::SetInputEnabled(bool bEnabled)
{
	bInputEnabled = bEnabled;
}

void UYarnInputHandler::TriggerAdvance()
{
	ProcessAdvanceInput();
}

void UYarnInputHandler::TriggerCancel()
{
	ProcessCancelInput();
}

void UYarnInputHandler::ProcessAdvanceInput()
{
	if (!DialogueRunner || !DialogueRunner->IsDialogueRunning())
	{
		return;
	}

	// check cooldown
	if (TimeSinceLastInput < InputCooldown)
	{
		return;
	}

	TimeSinceLastInput = 0.0f;

	// track for rapid press detection
	if (TrackRapidPress())
	{
		// rapid press threshold met - cancel dialogue
		DialogueRunner->StopDialogue();
		return;
	}

	// check if we should hurry up first
	if (bHurryUpBeforeAdvance)
	{
		// check if line is still being displayed (typewriter active)
		FYarnLineCancellationToken& Token = DialogueRunner->GetCurrentCancellationToken();

		if (!Token.IsHurryUpRequested())
		{
			// first press - hurry up
			DialogueRunner->RequestHurryUp();
		}
		else
		{
			// already hurried - advance
			DialogueRunner->RequestNextLine();
		}
	}
	else
	{
		// always advance
		DialogueRunner->RequestNextLine();
	}
}

void UYarnInputHandler::ProcessCancelInput()
{
	if (!DialogueRunner || !DialogueRunner->IsDialogueRunning())
	{
		return;
	}

	// check cooldown
	if (TimeSinceLastInput < InputCooldown)
	{
		return;
	}

	TimeSinceLastInput = 0.0f;

	// cancel dialogue
	DialogueRunner->StopDialogue();
}

bool UYarnInputHandler::IsAdvanceKeyPressed() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	return PC->IsInputKeyDown(AdvanceKey) || PC->IsInputKeyDown(AlternateAdvanceKey);
}

bool UYarnInputHandler::IsCancelKeyPressed() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	return PC->IsInputKeyDown(CancelKey);
}

bool UYarnInputHandler::TrackRapidPress()
{
	if (PressesToCancel <= 0)
	{
		return false;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	RecentPressTimes.Add(CurrentTime);

	return RecentPressTimes.Num() >= PressesToCancel;
}

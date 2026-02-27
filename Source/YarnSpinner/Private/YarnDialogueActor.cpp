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

#include "YarnDialogueActor.h"
#include "YarnProgram.h"

AYarnDialogueActor::AYarnDialogueActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create the dialogue runner component
	DialogueRunner = CreateDefaultSubobject<UYarnDialogueRunner>(TEXT("DialogueRunner"));

	// Create the widget presenter component
	WidgetPresenter = CreateDefaultSubobject<UYarnWidgetPresenter>(TEXT("WidgetPresenter"));
}

void AYarnDialogueActor::BeginPlay()
{
	// Set up the dialogue runner BEFORE calling Super::BeginPlay()
	// because Super::BeginPlay() triggers component BeginPlay, which needs YarnProject to be set
	if (DialogueRunner)
	{
		if (YarnProject)
		{
			DialogueRunner->YarnProject = YarnProject;
		}

		// Disable component's auto-start - the actor handles auto-start instead
		DialogueRunner->bAutoStart = false;

		// Register the presenter with the dialogue runner
		DialogueRunner->DialoguePresenters.Add(WidgetPresenter);

		// Bind to dialogue events
		DialogueRunner->OnDialogueStart.AddDynamic(this, &AYarnDialogueActor::HandleDialogueStarted);
		DialogueRunner->OnDialogueComplete.AddDynamic(this, &AYarnDialogueActor::HandleDialogueComplete);
	}

	// Apply appearance settings to the presenter
	if (WidgetPresenter)
	{
		WidgetPresenter->TypewriterSpeed = TypewriterSpeed;
		WidgetPresenter->BackgroundColor = BackgroundColor;
		WidgetPresenter->TextColor = TextColor;
		WidgetPresenter->CharacterNameColor = CharacterNameColor;
	}

	Super::BeginPlay();

	// Auto-start if configured (must be after Super::BeginPlay so VM is set up)
	if (bAutoStart && !StartNode.IsEmpty())
	{
		StartDialogue(StartNode);
	}
}

void AYarnDialogueActor::StartDialogue(const FString& NodeName)
{
	if (DialogueRunner)
	{
		DialogueRunner->StartDialogue(NodeName);
	}
}

void AYarnDialogueActor::StopDialogue()
{
	if (DialogueRunner)
	{
		DialogueRunner->StopDialogue();
	}
}

bool AYarnDialogueActor::IsDialogueRunning() const
{
	return DialogueRunner && DialogueRunner->IsDialogueRunning();
}

void AYarnDialogueActor::HandleDialogueStarted()
{
	OnDialogueStarted.Broadcast();
}

void AYarnDialogueActor::HandleDialogueComplete()
{
	OnDialogueCompleted.Broadcast();
}

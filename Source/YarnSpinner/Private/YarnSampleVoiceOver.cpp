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

#include "YarnSampleVoiceOver.h"
#include "YarnProgram.h"
#include "YarnSpinnerModule.h"

AYarnSampleVoiceOver::AYarnSampleVoiceOver()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create the dialogue runner component
	DialogueRunner = CreateDefaultSubobject<UYarnDialogueRunner>(TEXT("DialogueRunner"));

	// Create the widget presenter component (for text display)
	WidgetPresenter = CreateDefaultSubobject<UYarnWidgetPresenter>(TEXT("WidgetPresenter"));

	// Create our custom voice-over presenter that uses DataTable lookup
	VoiceOverPresenter = CreateDefaultSubobject<UYarnDataTableVoiceOverPresenter>(TEXT("VoiceOverPresenter"));
}

void AYarnSampleVoiceOver::BeginPlay()
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

		// Register both presenters with the dialogue runner
		DialogueRunner->DialoguePresenters.Add(WidgetPresenter);
		DialogueRunner->DialoguePresenters.Add(VoiceOverPresenter);

		// Bind to dialogue events
		DialogueRunner->OnDialogueStart.AddDynamic(this, &AYarnSampleVoiceOver::HandleDialogueStarted);
		DialogueRunner->OnDialogueComplete.AddDynamic(this, &AYarnSampleVoiceOver::HandleDialogueComplete);
	}

	// Apply appearance settings to the widget presenter
	if (WidgetPresenter)
	{
		WidgetPresenter->TypewriterSpeed = TypewriterSpeed;
		WidgetPresenter->BackgroundColor = BackgroundColor;
		WidgetPresenter->TextColor = TextColor;
		WidgetPresenter->CharacterNameColor = CharacterNameColor;
	}

	// Apply settings to the voice-over presenter
	if (VoiceOverPresenter)
	{
		VoiceOverPresenter->FadeOutTimeOnInterrupt = FadeOutTimeOnInterrupt;
		VoiceOverPresenter->WaitTimeBeforeStart = WaitTimeBeforeStart;
		VoiceOverPresenter->WaitTimeAfterComplete = WaitTimeAfterComplete;

		// The voice-over presenter should NOT advance lines on its own
		// (the widget presenter handles user interaction for line advancement)
		VoiceOverPresenter->bEndLineWhenVoiceOverComplete = false;

		// Set up the owner reference for DataTable lookup
		if (UYarnDataTableVoiceOverPresenter* DTPresenter = Cast<UYarnDataTableVoiceOverPresenter>(VoiceOverPresenter))
		{
			DTPresenter->OwnerActor = this;
		}
	}

	Super::BeginPlay();

	// Auto-start if configured (must be after Super::BeginPlay so VM is set up)
	if (bAutoStart && !StartNode.IsEmpty())
	{
		StartDialogue(StartNode);
	}
}

void AYarnSampleVoiceOver::StartDialogue(const FString& NodeName)
{
	if (DialogueRunner)
	{
		DialogueRunner->StartDialogue(NodeName);
	}
}

void AYarnSampleVoiceOver::StopDialogue()
{
	if (DialogueRunner)
	{
		DialogueRunner->StopDialogue();
	}
}

bool AYarnSampleVoiceOver::IsDialogueRunning() const
{
	return DialogueRunner && DialogueRunner->IsDialogueRunning();
}

USoundBase* AYarnSampleVoiceOver::GetVoiceOverClipForLine(const FString& LineID) const
{
	if (!VoiceOverTable)
	{
		return nullptr;
	}

	// Look up the row in the DataTable using line ID as row name
	static const FString ContextString(TEXT("VoiceOverLookup"));
	FYarnVoiceOverEntry* Entry = VoiceOverTable->FindRow<FYarnVoiceOverEntry>(FName(*LineID), ContextString);

	if (Entry)
	{
		return Entry->AudioClip;
	}

	return nullptr;
}

void AYarnSampleVoiceOver::HandleDialogueStarted()
{
	OnDialogueStarted.Broadcast();
}

void AYarnSampleVoiceOver::HandleDialogueComplete()
{
	OnDialogueCompleted.Broadcast();
}

// ========== UYarnDataTableVoiceOverPresenter ==========

USoundBase* UYarnDataTableVoiceOverPresenter::GetVoiceOverClip_Implementation(const FYarnLocalizedLine& Line)
{
	if (OwnerActor)
	{
		USoundBase* Clip = OwnerActor->GetVoiceOverClipForLine(Line.RawLine.LineID);
		if (Clip)
		{
			return Clip;
		}
	}

	// Fall back to base implementation (metadata lookup)
	return Super::GetVoiceOverClip_Implementation(Line);
}

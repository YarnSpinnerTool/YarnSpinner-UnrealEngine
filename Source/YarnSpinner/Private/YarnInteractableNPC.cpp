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

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// our own header - must be included first for unreal header tool
#include "YarnInteractableNPC.h"

// components we use
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"

// dialogue runner for starting conversations
#include "YarnDialogueRunner.h"

// logging macros
#include "YarnSpinnerModule.h"

// for finding the player pawn
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// ============================================================================
// AYarnInteractableNPC
// ============================================================================
//
// this is an NPC that players can approach and interact with to trigger
// yarn dialogue. it has a sphere collision for detecting player proximity
// and a text prompt that appears when the player is in range.

// ----------------------------------------------------------------------------
// constructor
// ----------------------------------------------------------------------------

AYarnInteractableNPC::AYarnInteractableNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the body capsule as root - this is what line traces hit
	BodyCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCapsule"));
	SetRootComponent(BodyCapsule);
	BodyCapsule->SetCapsuleHalfHeight(88.0f);
	BodyCapsule->SetCapsuleRadius(34.0f);
	BodyCapsule->SetCollisionProfileName(TEXT("Pawn"));
	BodyCapsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// create the interaction sphere for proximity detection (larger than body)
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// create the skeletal mesh for the NPC's visual representation
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetRelativeLocation(FVector(0, 0, -88)); // align with capsule bottom

	// try to load the UE5 mannequin as the default mesh
	// this path works for UE5 projects with the mannequin content
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMesh(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny"));
	if (MannequinMesh.Succeeded())
	{
		MeshComponent->SetSkeletalMesh(MannequinMesh.Object);
	}

	// create the interaction prompt text that floats above the NPC
	InteractionPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InteractionPrompt"));
	InteractionPrompt->SetupAttachment(RootComponent);
	InteractionPrompt->SetRelativeLocation(FVector(0, 0, 120)); // above the NPC's head
	InteractionPrompt->SetHorizontalAlignment(EHTA_Center);
	InteractionPrompt->SetVerticalAlignment(EVRTA_TextCenter);
	InteractionPrompt->SetWorldSize(20.0f);
	InteractionPrompt->SetTextRenderColor(FColor::White);
	InteractionPrompt->SetVisibility(false); // hidden by default
}

// ----------------------------------------------------------------------------
// lifecycle
// ----------------------------------------------------------------------------

void AYarnInteractableNPC::BeginPlay()
{
	Super::BeginPlay();

	// update the sphere radius in case it was changed in the editor
	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(InteractionRadius);

		// bind overlap events
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AYarnInteractableNPC::OnInteractionSphereBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AYarnInteractableNPC::OnInteractionSphereEndOverlap);
	}

	// set up the interaction prompt text
	if (InteractionPrompt)
	{
		InteractionPrompt->SetText(FText::FromString(InteractionPromptText));
		InteractionPrompt->SetVisibility(false);
	}
}

// ----------------------------------------------------------------------------
// interaction
// ----------------------------------------------------------------------------

bool AYarnInteractableNPC::CanInteract() const
{
	// can interact if:
	// - interaction is enabled
	// - player is in range
	// - not already in a conversation
	return bCanBeInteractedWith && bPlayerInRange && !bIsInConversation;
}

bool AYarnInteractableNPC::Interact(UYarnDialogueRunner* DialogueRunner)
{
	if (!CanInteract())
	{
		return false;
	}

	if (!DialogueRunner)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("InteractableNPC: Cannot interact - no DialogueRunner provided"));
		return false;
	}

	if (DialogueNodeName.IsEmpty())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("InteractableNPC: Cannot interact - no DialogueNodeName set"));
		return false;
	}

	// mark as in conversation
	bIsInConversation = true;

	// hide the interaction prompt during dialogue
	SetPromptVisible(false);

	// store the runner so we can unbind later
	ActiveDialogueRunner = DialogueRunner;

	// bind to dialogue complete event
	DialogueRunner->OnDialogueComplete.AddDynamic(this, &AYarnInteractableNPC::HandleDialogueComplete);

	// start the dialogue
	UE_LOG(LogYarnSpinner, Log, TEXT("InteractableNPC '%s': Starting dialogue node '%s'"), *CharacterDisplayName, *DialogueNodeName);
	DialogueRunner->StartDialogue(DialogueNodeName);

	return true;
}

void AYarnInteractableNPC::OnDialogueEnded_Implementation()
{
	// default implementation does nothing
	// subclasses can override to add custom behaviour
}

void AYarnInteractableNPC::SetPromptVisible(bool bShow)
{
	if (InteractionPrompt)
	{
		InteractionPrompt->SetVisibility(bShow);
	}
}

// ----------------------------------------------------------------------------
// overlap events
// ----------------------------------------------------------------------------

void AYarnInteractableNPC::OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// check if the overlapping actor is a player pawn
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && OtherActor == PC->GetPawn())
	{
		bPlayerInRange = true;

		// show the interaction prompt if we can be interacted with
		if (bCanBeInteractedWith && !bIsInConversation)
		{
			SetPromptVisible(true);
		}

		UE_LOG(LogYarnSpinner, Verbose, TEXT("InteractableNPC '%s': Player entered range"), *CharacterDisplayName);
	}
}

void AYarnInteractableNPC::OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// check if the leaving actor is a player pawn
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && OtherActor == PC->GetPawn())
	{
		bPlayerInRange = false;

		// hide the interaction prompt
		SetPromptVisible(false);

		UE_LOG(LogYarnSpinner, Verbose, TEXT("InteractableNPC '%s': Player left range"), *CharacterDisplayName);
	}
}

// ----------------------------------------------------------------------------
// dialogue events
// ----------------------------------------------------------------------------

void AYarnInteractableNPC::HandleDialogueComplete()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("InteractableNPC '%s': Dialogue complete"), *CharacterDisplayName);

	// mark as no longer in conversation
	bIsInConversation = false;

	// unbind from the dialogue complete event
	if (ActiveDialogueRunner)
	{
		ActiveDialogueRunner->OnDialogueComplete.RemoveDynamic(this, &AYarnInteractableNPC::HandleDialogueComplete);
		ActiveDialogueRunner = nullptr;
	}

	// show the interaction prompt again if player is still in range
	if (bPlayerInRange && bCanBeInteractedWith)
	{
		SetPromptVisible(true);
	}

	// call the blueprint-overridable event
	OnDialogueEnded();
}

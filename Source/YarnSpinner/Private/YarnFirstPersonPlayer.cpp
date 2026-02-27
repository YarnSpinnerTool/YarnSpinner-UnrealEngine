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
#include "YarnFirstPersonPlayer.h"

// components we use
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"

// yarn spinner components
#include "YarnDialogueRunner.h"
#include "YarnWidgetPresenter.h"
#include "YarnInputHandler.h"
#include "YarnProgram.h"

// NPC class for interaction
#include "YarnInteractableNPC.h"

// logging
#include "YarnSpinnerModule.h"

// version detection for cross-engine compatibility
#include "YarnSpinnerVersion.h"

// enhanced input (UE5+ only)
#if YARNSPINNER_WITH_ENHANCED_INPUT
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#endif

// player controller
#include "GameFramework/PlayerController.h"

// collision queries
#include "Engine/World.h"

// ============================================================================
// AYarnFirstPersonPlayer
// ============================================================================
//
// this is a first-person player pawn with built-in yarn dialogue support.
// it handles WASD movement, mouse look, and NPC interaction via line trace.

// ----------------------------------------------------------------------------
// constructor
// ----------------------------------------------------------------------------

AYarnFirstPersonPlayer::AYarnFirstPersonPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the capsule for collision
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->SetCapsuleHalfHeight(88.0f);
	CapsuleComponent->SetCapsuleRadius(34.0f);
	CapsuleComponent->SetCollisionProfileName(TEXT("Pawn"));

	// create the first-person camera
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(CapsuleComponent);
	CameraComponent->SetRelativeLocation(FVector(0, 0, 64)); // eye height
	CameraComponent->bUsePawnControlRotation = true;

	// create the dialogue runner (shared with NPCs)
	DialogueRunner = CreateDefaultSubobject<UYarnDialogueRunner>(TEXT("DialogueRunner"));

	// create the widget presenter for text display
	WidgetPresenter = CreateDefaultSubobject<UYarnWidgetPresenter>(TEXT("WidgetPresenter"));

	// create the input handler for dialogue controls
	InputHandler = CreateDefaultSubobject<UYarnInputHandler>(TEXT("InputHandler"));
}

// ----------------------------------------------------------------------------
// lifecycle
// ----------------------------------------------------------------------------

void AYarnFirstPersonPlayer::BeginPlay()
{
	Super::BeginPlay();

	// configure the dialogue runner
	if (DialogueRunner)
	{
		// assign the yarn project
		if (YarnProject)
		{
			DialogueRunner->YarnProject = YarnProject;
		}

		// don't auto-start - NPCs will trigger dialogue
		DialogueRunner->bAutoStart = false;

		// add the widget presenter
		if (WidgetPresenter)
		{
			DialogueRunner->DialoguePresenters.Add(WidgetPresenter);
		}

		// bind dialogue events for movement locking
		DialogueRunner->OnDialogueStart.AddDynamic(this, &AYarnFirstPersonPlayer::OnDialogueStarted);
		DialogueRunner->OnDialogueComplete.AddDynamic(this, &AYarnFirstPersonPlayer::OnDialogueEnded);
	}

	// configure the input handler
	if (InputHandler && DialogueRunner)
	{
		InputHandler->DialogueRunner = DialogueRunner;
	}

	// input setup happens in PossessedBy() since BeginPlay runs before possession
	UE_LOG(LogYarnSpinner, Log, TEXT("FirstPersonPlayer: BeginPlay complete, waiting for possession"));
}

void AYarnFirstPersonPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	UE_LOG(LogYarnSpinner, Log, TEXT("FirstPersonPlayer: PossessedBy called with controller: %s"),
		NewController ? *NewController->GetName() : TEXT("nullptr"));

	// configure input now that we have a controller
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
#if YARNSPINNER_WITH_ENHANCED_INPUT
		// set up enhanced input mapping context if available (UE5+)
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (UInputMappingContext* MappingContext = Cast<UInputMappingContext>(InputMappingContext))
			{
				Subsystem->AddMappingContext(MappingContext, 0);
				UE_LOG(LogYarnSpinner, Log, TEXT("FirstPersonPlayer: Added input mapping context"));
			}
		}
#endif

		// configure for first-person gameplay - capture mouse, hide cursor
		PC->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(true);
		PC->SetInputMode(InputMode);

		UE_LOG(LogYarnSpinner, Log, TEXT("FirstPersonPlayer: Input configured successfully"));
	}
}

void AYarnFirstPersonPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// update the cached looked-at NPC
	AYarnInteractableNPC* NewLookedAtNPC = GetLookedAtNPC();
	if (NewLookedAtNPC != CurrentLookedAtNPC)
	{
		CurrentLookedAtNPC = NewLookedAtNPC;
	}

	// direct key polling for movement (works without input configuration)
	if (bMovementEnabled)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			// poll WASD keys directly
			float ForwardInput = 0.0f;
			float RightInput = 0.0f;

			if (PC->IsInputKeyDown(EKeys::W)) ForwardInput += 1.0f;
			if (PC->IsInputKeyDown(EKeys::S)) ForwardInput -= 1.0f;
			if (PC->IsInputKeyDown(EKeys::D)) RightInput += 1.0f;
			if (PC->IsInputKeyDown(EKeys::A)) RightInput -= 1.0f;

			// apply movement
			if (!FMath::IsNearlyZero(ForwardInput) || !FMath::IsNearlyZero(RightInput))
			{
				const FRotator ControlRotation = GetControlRotation();
				const FRotator YawRotation(0, ControlRotation.Yaw, 0);

				const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
				const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

				FVector Movement = ForwardDir * ForwardInput + RightDir * RightInput;
				Movement.Normalize();
				Movement *= MovementSpeed * DeltaTime;

				AddActorWorldOffset(Movement, true);
			}

			// poll mouse for looking
			float MouseX, MouseY;
			PC->GetInputMouseDelta(MouseX, MouseY);
			if (!FMath::IsNearlyZero(MouseX) || !FMath::IsNearlyZero(MouseY))
			{
				PC->AddYawInput(MouseX * LookSensitivity);
				PC->AddPitchInput(-MouseY * LookSensitivity);
			}

			// poll E key for interaction
			static bool bWasEPressed = false;
			bool bIsEPressed = PC->IsInputKeyDown(EKeys::E);
			if (bIsEPressed && !bWasEPressed)
			{
				TryInteract();
			}
			bWasEPressed = bIsEPressed;
		}
	}

	// clear accumulated input (for enhanced input path)
	MovementInput = FVector2D::ZeroVector;
	LookInput = FVector2D::ZeroVector;
}

// ----------------------------------------------------------------------------
// input setup
// ----------------------------------------------------------------------------

void AYarnFirstPersonPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

#if YARNSPINNER_WITH_ENHANCED_INPUT
	// try enhanced input first (UE5+)
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// bind movement action
		if (UInputAction* MoveInputAction = Cast<UInputAction>(MoveAction))
		{
			EnhancedInput->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AYarnFirstPersonPlayer::HandleMoveInput);
		}

		// bind look action
		if (UInputAction* LookInputAction = Cast<UInputAction>(LookAction))
		{
			EnhancedInput->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AYarnFirstPersonPlayer::HandleLookInput);
		}

		// bind interact action
		if (UInputAction* InteractInputAction = Cast<UInputAction>(InteractAction))
		{
			EnhancedInput->BindAction(InteractInputAction, ETriggerEvent::Started, this, &AYarnFirstPersonPlayer::HandleInteractInput);
		}
	}
#endif

	// also set up legacy input as fallback (works on UE4 and UE5)
	PlayerInputComponent->BindAxis("MoveForward", this, &AYarnFirstPersonPlayer::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AYarnFirstPersonPlayer::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &AYarnFirstPersonPlayer::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AYarnFirstPersonPlayer::LookUp);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AYarnFirstPersonPlayer::OnInteractPressed);
}

// ----------------------------------------------------------------------------
// enhanced input handlers (UE5+ only)
// ----------------------------------------------------------------------------

#if YARNSPINNER_WITH_ENHANCED_INPUT
void AYarnFirstPersonPlayer::HandleMoveInput(const FInputActionValue& Value)
{
	if (!bMovementEnabled)
	{
		return;
	}

	MovementInput = Value.Get<FVector2D>();
}

void AYarnFirstPersonPlayer::HandleLookInput(const FInputActionValue& Value)
{
	if (!bMovementEnabled)
	{
		return;
	}

	const FVector2D LookValue = Value.Get<FVector2D>();

	// apply look input to controller rotation
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->AddYawInput(LookValue.X * LookSensitivity);
		PC->AddPitchInput(-LookValue.Y * LookSensitivity);
	}
}

void AYarnFirstPersonPlayer::HandleInteractInput(const FInputActionValue& Value)
{
	TryInteract();
}
#endif

// ----------------------------------------------------------------------------
// legacy input handlers
// ----------------------------------------------------------------------------

void AYarnFirstPersonPlayer::MoveForward(float Value)
{
	if (bMovementEnabled && !FMath::IsNearlyZero(Value))
	{
		MovementInput.Y = Value;
	}
}

void AYarnFirstPersonPlayer::MoveRight(float Value)
{
	if (bMovementEnabled && !FMath::IsNearlyZero(Value))
	{
		MovementInput.X = Value;
	}
}

void AYarnFirstPersonPlayer::Turn(float Value)
{
	if (bMovementEnabled && !FMath::IsNearlyZero(Value))
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->AddYawInput(Value * LookSensitivity);
		}
	}
}

void AYarnFirstPersonPlayer::LookUp(float Value)
{
	if (bMovementEnabled && !FMath::IsNearlyZero(Value))
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->AddPitchInput(-Value * LookSensitivity);
		}
	}
}

void AYarnFirstPersonPlayer::OnInteractPressed()
{
	TryInteract();
}

// ----------------------------------------------------------------------------
// interaction
// ----------------------------------------------------------------------------

AYarnInteractableNPC* AYarnFirstPersonPlayer::GetLookedAtNPC() const
{
	if (!CameraComponent)
	{
		return nullptr;
	}

	// perform line trace from camera forward
	const FVector Start = CameraComponent->GetComponentLocation();
	const FVector End = Start + CameraComponent->GetForwardVector() * InteractionTraceDistance;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
	{
		// check if we hit an interactable NPC
		if (AYarnInteractableNPC* NPC = Cast<AYarnInteractableNPC>(HitResult.GetActor()))
		{
			return NPC;
		}
	}

	return nullptr;
}

bool AYarnFirstPersonPlayer::TryInteract()
{
	// don't allow interaction during dialogue
	if (DialogueRunner && DialogueRunner->IsDialogueRunning())
	{
		return false;
	}

	// check if we're looking at an NPC
	AYarnInteractableNPC* NPC = GetLookedAtNPC();
	if (!NPC)
	{
		return false;
	}

	// try to interact with the NPC
	return NPC->Interact(DialogueRunner);
}

void AYarnFirstPersonPlayer::SetMovementEnabled(bool bEnable)
{
	bMovementEnabled = bEnable;

	// also control input mode
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (bEnable)
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
		else
		{
			// during dialogue, show cursor for clicking options
			PC->SetInputMode(FInputModeGameAndUI());
			PC->bShowMouseCursor = true;
		}
	}
}

// ----------------------------------------------------------------------------
// dialogue events
// ----------------------------------------------------------------------------

void AYarnFirstPersonPlayer::OnDialogueStarted()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("FirstPersonPlayer: Dialogue started - disabling movement"));
	SetMovementEnabled(false);
}

void AYarnFirstPersonPlayer::OnDialogueEnded()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("FirstPersonPlayer: Dialogue ended - enabling movement"));
	SetMovementEnabled(true);
}

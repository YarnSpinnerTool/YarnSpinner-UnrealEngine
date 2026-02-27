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

#pragma once

// ============================================================================
// YarnFirstPersonPlayer.h
// ============================================================================
//
// A first-person player pawn with built-in dialogue support. Includes WASD
// movement, mouse look, and the ability to interact with Yarn NPCs using
// line traces.
//
// HOW IT WORKS:
// 1. Player looks at an NPC within interaction distance.
// 2. Line trace from camera detects the NPC's collision.
// 3. NPC shows "Press E to Talk" prompt.
// 4. Player presses interact key (E).
// 5. Movement is disabled, dialogue starts.
// 6. When dialogue ends, movement is re-enabled.
//
// BLUEPRINT VS C++ USAGE:
// Blueprint users:
//   - Spawn this pawn in your level (or use AYarnMultiNPCDemo).
//   - Configure YarnProject, MovementSpeed, LookSensitivity.
//   - Optionally override OnInteract for custom interaction handling.
//
// C++ users:
//   - Can subclass for custom player behaviour.
//   - Can access DialogueRunner, WidgetPresenter, InputHandler components.
//   - Can call TryInteract() directly to trigger interaction.

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// unreal core - provides fundamental types
#include "CoreMinimal.h"

// APawn - base class for player-controlled actors
#include "GameFramework/Pawn.h"

// version detection for cross-engine compatibility
#include "YarnSpinnerVersion.h"

// required by unreal's reflection system
#include "YarnFirstPersonPlayer.generated.h"

class UCapsuleComponent;
class UCameraComponent;
class UYarnDialogueRunner;
class UYarnWidgetPresenter;
class UYarnInputHandler;
class UYarnProject;
class AYarnInteractableNPC;

#if YARNSPINNER_WITH_ENHANCED_INPUT
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
#endif

// ============================================================================
// AYarnFirstPersonPlayer
// ============================================================================

/**
 * a first-person player pawn with integrated yarn dialogue support.
 *
 * features:
 * - WASD movement + mouse look
 * - line trace to detect interactable NPCs
 * - E key to interact with looked-at NPC
 * - automatic movement lock during dialogue
 * - built-in dialogue runner, widget presenter, and input handler
 *
 * @see AYarnInteractableNPC for the NPCs this player can interact with
 * @see AYarnMultiNPCDemo for a complete demo setup
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API AYarnFirstPersonPlayer : public APawn
{
	GENERATED_BODY()

public:
	AYarnFirstPersonPlayer();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	// ========================================================================
	// components
	// ========================================================================

	/**
	 * the capsule that handles collision with the world.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* CapsuleComponent;

	/**
	 * the first-person camera at eye level.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* CameraComponent;

	/**
	 * the dialogue runner that executes yarn scripts.
	 * shared with NPCs - when an NPC starts dialogue, it uses this runner.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UYarnDialogueRunner* DialogueRunner;

	/**
	 * the widget presenter that displays dialogue text on screen.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UYarnWidgetPresenter* WidgetPresenter;

	/**
	 * the input handler for dialogue advancement (space/enter to continue).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UYarnInputHandler* InputHandler;

	// ========================================================================
	// configuration - yarn spinner settings
	// ========================================================================

	/**
	 * the yarn project containing dialogue for all NPCs.
	 * assign this to give NPCs access to dialogue content.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	UYarnProject* YarnProject;

	// ========================================================================
	// configuration - movement settings
	// ========================================================================

	/**
	 * walking speed in units per second.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float MovementSpeed = 600.0f;

	/**
	 * mouse look sensitivity multiplier.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float LookSensitivity = 1.0f;

	// ========================================================================
	// configuration - interaction settings
	// ========================================================================

	/**
	 * how far the line trace extends to detect interactable NPCs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "50.0"))
	float InteractionTraceDistance = 300.0f;

	/**
	 * whether movement is currently enabled.
	 * automatically disabled during dialogue.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bMovementEnabled = true;

	// ========================================================================
	// enhanced input configuration (UE5+ only)
	// ========================================================================
	// On UE4 these properties exist but are non-functional (Enhanced Input requires UE5)

	/**
	 * the input mapping context for player controls (UE5+ only).
	 * if not set, uses legacy input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UObject* InputMappingContext;

	/**
	 * input action for movement (WASD) (UE5+ only).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UObject* MoveAction;

	/**
	 * input action for looking (mouse) (UE5+ only).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UObject* LookAction;

	/**
	 * input action for interaction (E key) (UE5+ only).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UObject* InteractAction;

	// ========================================================================
	// interaction methods
	// ========================================================================

	/**
	 * get the NPC the player is currently looking at (if any).
	 * uses a line trace from the camera forward.
	 *
	 * @return the NPC being looked at, or nullptr if none
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AYarnInteractableNPC* GetLookedAtNPC() const;

	/**
	 * attempt to interact with the NPC the player is looking at.
	 *
	 * @return true if interaction was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	/**
	 * enable or disable player movement.
	 * typically called when dialogue starts/ends.
	 *
	 * @param bEnable true to enable movement, false to disable
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetMovementEnabled(bool bEnable);

	/**
	 * get the dialogue runner component.
	 * NPCs use this to start their dialogue.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Yarn Spinner")
	UYarnDialogueRunner* GetDialogueRunner() const { return DialogueRunner; }

protected:
	// ========================================================================
	// input handling
	// ========================================================================

#if YARNSPINNER_WITH_ENHANCED_INPUT
	/**
	 * handle movement input (WASD) - Enhanced Input.
	 */
	void HandleMoveInput(const FInputActionValue& Value);

	/**
	 * handle look input (mouse) - Enhanced Input.
	 */
	void HandleLookInput(const FInputActionValue& Value);

	/**
	 * handle interact input (E key) - Enhanced Input.
	 */
	void HandleInteractInput(const FInputActionValue& Value);
#endif

	/**
	 * legacy input: move forward/backward.
	 */
	void MoveForward(float Value);

	/**
	 * legacy input: move right/left.
	 */
	void MoveRight(float Value);

	/**
	 * legacy input: look up/down.
	 */
	void LookUp(float Value);

	/**
	 * legacy input: turn left/right.
	 */
	void Turn(float Value);

	/**
	 * legacy input: interact key pressed.
	 */
	void OnInteractPressed();

	// ========================================================================
	// dialogue events
	// ========================================================================

	/**
	 * called when dialogue starts. disables movement.
	 */
	UFUNCTION()
	void OnDialogueStarted();

	/**
	 * called when dialogue ends. re-enables movement.
	 */
	UFUNCTION()
	void OnDialogueEnded();

	// ========================================================================
	// internal state
	// ========================================================================

	/** the NPC we're currently looking at (cached from tick) */
	UPROPERTY()
	AYarnInteractableNPC* CurrentLookedAtNPC;

	/** movement input accumulated this frame */
	FVector2D MovementInput;

	/** look input accumulated this frame */
	FVector2D LookInput;
};

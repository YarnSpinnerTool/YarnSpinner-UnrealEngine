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
// YarnInputHandler.h
// ============================================================================
//
// Handles player input for controlling dialogue flow. Translates button
// presses into dialogue runner commands (hurry up, advance, cancel).
//
// BLUEPRINT VS C++ USAGE:
// Blueprint users:
//   - Add this component alongside your dialogue runner.
//   - Configure input keys/actions in the editor.
//   - Use SetInputEnabled() to disable input during cutscenes, etc.
//
// C++ users:
//   - Can call TriggerAdvance() / TriggerCancel() directly.
//   - Can subclass for custom input handling.
//
// INPUT FLOW:
// The two-tier system works like this:
//   1. First press: hurry up (completes typewriter instantly).
//   2. Second press: advance to next line.
//   3. Rapid presses: optionally cancel dialogue entirely.
//
// This can be disabled by setting bHurryUpBeforeAdvance = false.

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// Unreal core - provides fundamental types.
#include "CoreMinimal.h"

// UActorComponent - this is a component attached to an actor.
#include "Components/ActorComponent.h"

// Version detection for cross-engine compatibility.
#include "YarnSpinnerVersion.h"

// UInputAction - for Enhanced Input System support (UE5+ only).
#if YARNSPINNER_WITH_ENHANCED_INPUT
#include "InputAction.h"
#endif

// Required by Unreal's reflection system.
#include "YarnInputHandler.generated.h"

class UYarnDialogueRunner;

#if YARNSPINNER_WITH_ENHANCED_INPUT
class UInputAction;
#endif

// ============================================================================
// EYarnInputMode
// ============================================================================

/**
 * Input mode for the yarn input handler.
 * Determines how player input is detected.
 */
UENUM(BlueprintType)
enum class EYarnInputMode : uint8
{
	/** Use Enhanced Input System actions (UE5 recommended). */
	EnhancedInput UMETA(DisplayName = "Enhanced Input"),

	/** Use legacy key codes (simpler setup, UE4 compatible). */
	LegacyKeyCode UMETA(DisplayName = "Legacy Key Code"),

	/** No automatic input - control dialogue via code only. */
	None UMETA(DisplayName = "None")
};

// ============================================================================
// UYarnInputHandler
// ============================================================================

/**
 * Handles player input for dialogue advancement.
 *
 * Listens for input and translates it to dialogue commands:
 *   - Hurry up (speed up typewriter, first press)
 *   - Advance to next line (second press)
 *   - Cancel dialogue (rapid presses or cancel key)
 *
 * Supports both Enhanced Input System (UE5) and legacy key codes.
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnInputHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	UYarnInputHandler();

	// ------------------------------------------------------------------------
	// lifecycle
	// ------------------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------------
	// configuration
	// ------------------------------------------------------------------------

	/**
	 * The dialogue runner to control.
	 * Set this in the editor or via code before BeginPlay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input")
	UYarnDialogueRunner* DialogueRunner;

	/** Input mode to use (enhanced input, legacy keys, or none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input")
	EYarnInputMode InputMode = EYarnInputMode::LegacyKeyCode;

	// ------------------------------------------------------------------------
	// enhanced input settings (UE5+ only)
	// ------------------------------------------------------------------------
	// These are used when InputMode == EnhancedInput.
	// Create input actions in your project and assign them here.
	// On UE4 these properties exist but are non-functional (Enhanced Input requires UE5).

	/** Input action for advancing dialogue / hurrying up (UE5+ only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Enhanced Input", meta = (EditCondition = "InputMode == EYarnInputMode::EnhancedInput"))
	UObject* AdvanceAction;

	/** Input action for cancelling dialogue (UE5+ only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Enhanced Input", meta = (EditCondition = "InputMode == EYarnInputMode::EnhancedInput"))
	UObject* CancelAction;

	// ------------------------------------------------------------------------
	// legacy key code settings
	// ------------------------------------------------------------------------
	// These are used when InputMode == LegacyKeyCode.
	// Simpler to set up, works with any key.

	/** Key to advance dialogue / hurry up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Legacy", meta = (EditCondition = "InputMode == EYarnInputMode::LegacyKeyCode"))
	FKey AdvanceKey = EKeys::SpaceBar;

	/** Alternative key to advance dialogue (e.g., enter in addition to space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Legacy", meta = (EditCondition = "InputMode == EYarnInputMode::LegacyKeyCode"))
	FKey AlternateAdvanceKey = EKeys::Enter;

	/** Key to cancel dialogue (immediately stops dialogue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Legacy", meta = (EditCondition = "InputMode == EYarnInputMode::LegacyKeyCode"))
	FKey CancelKey = EKeys::Escape;

	// ------------------------------------------------------------------------
	// behaviour settings
	// ------------------------------------------------------------------------

	/**
	 * If true, first press hurries up (completes typewriter), second press advances.
	 * If false, every press advances immediately (skipping hurry-up phase).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Behaviour")
	bool bHurryUpBeforeAdvance = true;

	/**
	 * Number of rapid presses required to cancel dialogue.
	 * Set to 0 to disable cancel-by-pressing (only cancel key/action works).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Behaviour")
	int32 PressesToCancel = 3;

	/**
	 * Time window for counting rapid presses (seconds).
	 * Presses outside this window don't count towards cancellation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Behaviour")
	float RapidPressWindow = 1.0f;

	/**
	 * Minimum time between input processing (seconds).
	 * Prevents double-press issues from key bounce.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input|Behaviour")
	float InputCooldown = 0.1f;

	/** Whether input handling is currently enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Input")
	bool bInputEnabled = true;

	// ------------------------------------------------------------------------
	// blueprint-callable methods
	// ------------------------------------------------------------------------

	/**
	 * Enable or disable input handling.
	 * Useful for cutscenes or when you want to control dialogue via code.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Input")
	void SetInputEnabled(bool bEnabled);

	/**
	 * Manually trigger an advance input.
	 * Use this to advance dialogue from custom input handling.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Input")
	void TriggerAdvance();

	/**
	 * Manually trigger a cancel input.
	 * Immediately stops the dialogue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Input")
	void TriggerCancel();

protected:
	// ------------------------------------------------------------------------
	// internal input processing
	// ------------------------------------------------------------------------

	/** Process advance input (hurry up or advance depending on state). */
	void ProcessAdvanceInput();

	/** Process cancel input (stop dialogue). */
	void ProcessCancelInput();

	/** Check if advance key is pressed (legacy input). */
	bool IsAdvanceKeyPressed() const;

	/** Check if cancel key is pressed (legacy input). */
	bool IsCancelKeyPressed() const;

private:
	// ------------------------------------------------------------------------
	// internal state
	// ------------------------------------------------------------------------

	/** Time since last input was processed (for cooldown). */
	float TimeSinceLastInput = 0.0f;

	/** Whether line is fully displayed (determines hurry vs advance). */
	bool bLineFullyDisplayed = false;

	/** Timestamps of recent presses (for rapid-press tracking). */
	TArray<float> RecentPressTimes;

	/** Track rapid presses and return true if threshold met. */
	bool TrackRapidPress();

	/** Whether advance key was pressed last frame (for edge detection). */
	bool bAdvanceKeyWasPressed = false;

	/** Whether cancel key was pressed last frame (for edge detection). */
	bool bCancelKeyWasPressed = false;
};

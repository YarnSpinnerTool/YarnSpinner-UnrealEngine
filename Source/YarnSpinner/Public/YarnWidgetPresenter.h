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
// YarnWidgetPresenter.h
// ============================================================================
//
// A ready-to-use presenter that displays dialogue using UMG widgets. It creates
// a UYarnDialogueWidget (or your custom subclass) and handles all the interaction
// between the dialogue runner and the widget.
//
// BLUEPRINT VS C++ USAGE:
// Blueprint users:
//   - Add this component to your actor (or use AYarnVoiceOverDemo).
//   - Configure appearance via the properties (TypewriterSpeed, colours, etc.).
//   - Optionally create a custom UYarnDialogueWidget blueprint for advanced UI.
//
// C++ users:
//   - Can subclass this for custom behaviour.
//   - Can subclass UYarnDialogueWidget for custom UI.
//
// QUICK START:
//   1. Add UYarnWidgetPresenter to your dialogue runner's presenters array.
//   2. Run the game - a default dialogue UI will appear.
//   3. Customise the appearance via the properties.
//   4. For advanced customisation, create a UYarnDialogueWidget blueprint.

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// Unreal core - provides fundamental types.
#include "CoreMinimal.h"

// UYarnDialoguePresenter - base class we inherit from.
#include "YarnDialoguePresenter.h"

// Required by Unreal's reflection system.
#include "YarnWidgetPresenter.generated.h"

class UYarnDialogueWidget;

// ============================================================================
// UYarnWidgetPresenter
// ============================================================================

/**
 * A dialogue presenter that displays dialogue using a UMG widget.
 *
 * Automatically creates and manages a UYarnDialogueWidget for displaying
 * dialogue text, character names, and option buttons.
 *
 * @see UYarnDialogueWidget for the widget that handles the actual display
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnWidgetPresenter : public UYarnDialoguePresenter
{
	GENERATED_BODY()

public:
	UYarnWidgetPresenter();

	// ------------------------------------------------------------------------
	// lifecycle
	// ------------------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ------------------------------------------------------------------------
	// UYarnDialoguePresenter overrides
	// ------------------------------------------------------------------------

	virtual void OnDialogueStarted_Implementation() override;
	virtual void OnDialogueComplete_Implementation() override;
	virtual void RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry) override;
	virtual void RunOptions_Implementation(const FYarnOptionSet& Options) override;
	virtual void OnHurryUpRequested_Implementation() override;

	// ------------------------------------------------------------------------
	// appearance settings
	// ------------------------------------------------------------------------
	// Configure these in the editor to customise the look of your dialogue UI.

	/**
	 * Typewriter speed (characters per second).
	 * Set to 0 for instant text display (no typewriter effect).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Appearance")
	float TypewriterSpeed = 50.0f;

	/** Background colour for the dialogue box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Appearance")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.9f);

	/** Text colour for dialogue lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Appearance")
	FLinearColor TextColor = FLinearColor::White;

	/** Text colour for character names (displayed above dialogue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Appearance")
	FLinearColor CharacterNameColor = FLinearColor(0.8f, 0.6f, 0.2f, 1.0f);

	// ------------------------------------------------------------------------
	// widget settings
	// ------------------------------------------------------------------------

	/**
	 * The widget class to use for the dialogue UI.
	 * Defaults to UYarnDialogueWidget. Set this to your own blueprint
	 * widget class for custom UI.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Widget")
	TSubclassOf<UYarnDialogueWidget> WidgetClass;

	/**
	 * Get the current dialogue widget instance.
	 * @return the widget, or nullptr if dialogue hasn't started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	UYarnDialogueWidget* GetDialogueWidget() const { return DialogueWidget; }

protected:
	/** The dialogue widget instance created at runtime. */
	UPROPERTY()
	UYarnDialogueWidget* DialogueWidget;

	/** Create the widget instance if it doesn't exist. */
	void CreateWidget();

	/** Callback when the widget signals continue (player pressed advance). */
	UFUNCTION()
	void HandleWidgetContinue();

	/** Callback when the widget signals an option was chosen. */
	UFUNCTION()
	void HandleOptionChosen(int32 OptionIndex);
};

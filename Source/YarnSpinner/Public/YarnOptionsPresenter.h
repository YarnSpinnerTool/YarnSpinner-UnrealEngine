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

#include "CoreMinimal.h"
#include "YarnDialoguePresenter.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Blueprint/UserWidget.h"
#include "YarnOptionsPresenter.generated.h"

// ============================================================================
// UYarnOptionWidget
// ============================================================================
//
// WHAT THIS IS:
// A widget representing a single dialogue option that the player can select.
// Create a Blueprint widget that derives from this class to customize appearance.
//
// HOW IT WORKS:
// 1. UYarnOptionsPresenter creates instances of this widget for each option
// 2. SetupOption is called with the option data and index
// 3. when the player clicks the button, HandleButtonClicked notifies the presenter
//
// BLUEPRINT USAGE:
// 1. create a new Widget Blueprint
// 2. set parent class to YarnOptionWidget
// 3. add a TextBlock named "OptionText" and a Button named "OptionButton"
// 4. customize appearance as desired
// 5. assign the widget class to UYarnOptionsPresenter.OptionWidgetClass

/**
 * A single option button widget for dialogue choices.
 *
 * Create a Blueprint widget that derives from this class to customize
 * the appearance of dialogue options. The widget must contain:
 * - A TextBlock named "OptionText" to display the option text
 * - A Button named "OptionButton" for player interaction
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnOptionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * The text widget that displays the option text.
	 * Required - must be named "OptionText" in the Blueprint widget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner", meta = (BindWidget))
	UTextBlock* OptionText;

	/**
	 * The button widget that the user clicks.
	 * Required - must be named "OptionButton" in the Blueprint widget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner", meta = (BindWidget))
	UButton* OptionButton;

	/**
	 * The index of this option in the option set.
	 * Used when reporting selection back to the presenter.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner")
	int32 OptionIndex;

	/**
	 * Whether this option is available for selection.
	 * Unavailable options have failed their condition check.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner")
	bool bIsAvailable;

	/**
	 * Whether this option is currently selected/highlighted.
	 * Used for keyboard navigation.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner")
	bool bIsSelected;

	/**
	 * Set up the option widget with data.
	 *
	 * Override this in Blueprint to customize how options are displayed.
	 * The default implementation sets the text and enables/disables the button.
	 *
	 * @param InOption The option data containing text and availability.
	 * @param InIndex The index of this option in the option set.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner")
	void SetupOption(const FYarnOption& InOption, int32 InIndex);
	virtual void SetupOption_Implementation(const FYarnOption& InOption, int32 InIndex);

	/**
	 * Called when this option should appear unavailable.
	 *
	 * Override this in Blueprint to customize unavailable appearance.
	 * The default implementation disables the button.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner")
	void SetOptionUnavailable();
	virtual void SetOptionUnavailable_Implementation();

	/**
	 * Called when this option becomes selected (keyboard navigation).
	 *
	 * Override this in Blueprint to customize selected appearance.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner")
	void OnOptionSelected();
	virtual void OnOptionSelected_Implementation();

	/**
	 * Called when this option becomes deselected (keyboard navigation).
	 *
	 * Override this in Blueprint to restore normal appearance.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner")
	void OnOptionDeselected();
	virtual void OnOptionDeselected_Implementation();

	/**
	 * Programmatically trigger this option's selection.
	 * Called when the player presses confirm while this option is highlighted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void SubmitOption();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleButtonClicked();

	/** The owning options presenter. Cached for quick access. */
	UPROPERTY()
	class UYarnOptionsPresenter* OwningPresenter;

	/** Find the owning presenter through the hierarchy */
	class UYarnOptionsPresenter* FindOwningPresenter();
};

// ============================================================================
// Delegates
// ============================================================================

/**
 * Delegate called when an option is selected.
 * @param OptionIndex The index of the selected option.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYarnOptionSelectedBP, int32, OptionIndex);

/**
 * Delegate called when option display is complete (after fade-in).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnOptionsDisplayComplete);

/**
 * Delegate called when options are dismissed (after fade-out).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnOptionsDismissed);

// ============================================================================
// UYarnOptionsPresenter
// ============================================================================
//
// WHAT THIS IS:
// A dialogue presenter that displays dialogue options using UMG widgets.
//
// HOW IT WORKS:
// 1. receives options from the dialogue runner via RunOptions
// 2. creates/reuses UYarnOptionWidget instances for each option
// 3. displays them in a container panel
// 4. handles selection via button click or keyboard
// 5. reports selection back to the dialogue runner
//
// BLUEPRINT USAGE:
// 1. create a UYarnOptionWidget Blueprint for option appearance
// 2. add UYarnOptionsPresenter component to your dialogue actor
// 3. set OptionWidgetClass to your custom widget
// 4. set OptionsContainer to a panel widget in your UI
// 5. optionally configure last line display and fade effects

/**
 * A dialogue presenter that displays options using UMG widgets.
 *
 * This presenter creates option button widgets and manages their display.
 * Configure it with a widget class to use for options.
 *
 * @see UYarnOptionWidget for customizing option appearance
 * @see UYarnDialoguePresenter for the base presenter interface
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnOptionsPresenter : public UYarnDialoguePresenter
{
	GENERATED_BODY()

public:
	UYarnOptionsPresenter();

	// ========================================================================
	// UActorComponent interface
	// ========================================================================

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================================================
	// UYarnDialoguePresenter interface
	// ========================================================================

	virtual void OnDialogueStarted_Implementation() override;
	virtual void OnDialogueComplete_Implementation() override;
	virtual void RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry) override;
	virtual void RunOptions_Implementation(const FYarnOptionSet& Options) override;
	virtual void OnHurryUpRequested_Implementation() override;

	// ========================================================================
	// Widget Configuration
	// ========================================================================

	/**
	 * The widget class to use for option buttons.
	 * Must derive from UYarnOptionWidget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Widgets")
	TSubclassOf<UYarnOptionWidget> OptionWidgetClass;

	/**
	 * The container panel to add option widgets to.
	 * Options will be added as children of this panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Widgets")
	UPanelWidget* OptionsContainer;

	/**
	 * The root widget to apply fade effects to.
	 * If not set, fading is disabled even if bUseFadeEffect is true.
	 * This should typically be the parent of your options container.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Widgets")
	UWidget* FadeWidget;

	// ========================================================================
	// Behaviour Configuration
	// ========================================================================

	/**
	 * Whether to show unavailable options (greyed out).
	 *
	 * When true, options whose conditions failed are still displayed
	 * but disabled, so players can see what they could have chosen.
	 * When false, unavailable options are hidden entirely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Behaviour")
	bool bShowUnavailableOptions = false;

	/**
	 * Whether to apply strikethrough formatting to unavailable options.
	 *
	 * When true, unavailable option text will have strikethrough styling
	 * to clearly indicate they cannot be selected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Behaviour", meta = (EditCondition = "bShowUnavailableOptions"))
	bool bStrikethroughUnavailable = true;

	// ========================================================================
	// Last Line Configuration
	// ========================================================================
	// Optionally shows the last line of dialogue before the options appeared,
	// providing context for the player's choice.

	/**
	 * Whether to show the last line before options.
	 *
	 * When true, the last line of dialogue that appeared before the options
	 * will be displayed above or alongside the options.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Last Line")
	bool bShowLastLine = false;

	/**
	 * The text widget that displays the last line text.
	 * Only used when bShowLastLine is true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Last Line", meta = (EditCondition = "bShowLastLine"))
	UTextBlock* LastLineTextWidget;

	/**
	 * The container for the last line display.
	 * Will be shown/hidden based on whether there is a last line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Last Line", meta = (EditCondition = "bShowLastLine"))
	UWidget* LastLineContainer;

	/**
	 * The text widget that displays the character name for the last line.
	 * Optional - if not set, character name will be included in the line text.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Last Line", meta = (EditCondition = "bShowLastLine"))
	UTextBlock* LastLineCharacterNameWidget;

	/**
	 * The container for the last line character name.
	 * Will be shown/hidden based on whether the last line has a character name.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Last Line", meta = (EditCondition = "bShowLastLine"))
	UWidget* LastLineCharacterNameContainer;

	// ========================================================================
	// Fade Configuration
	// ========================================================================

	/**
	 * Whether to fade the options UI in and out.
	 *
	 * When true, options will fade in when displayed and fade out when
	 * an option is selected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Fade")
	bool bUseFadeEffect = true;

	/**
	 * Duration of the fade-in effect in seconds.
	 * The options become interactive after the fade completes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Fade", meta = (EditCondition = "bUseFadeEffect", ClampMin = "0.0"))
	float FadeInDuration = 0.25f;

	/**
	 * Duration of the fade-out effect in seconds.
	 * Dialogue continues after the fade completes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Fade", meta = (EditCondition = "bUseFadeEffect", ClampMin = "0.0"))
	float FadeOutDuration = 0.1f;

	// ========================================================================
	// Keyboard Navigation Configuration
	// ========================================================================

	/**
	 * Whether to enable keyboard navigation of options.
	 *
	 * When true, players can use Up/Down to navigate and Enter/Space to select.
	 * The first available option is auto-selected when options appear.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Keyboard")
	bool bEnableKeyboardNavigation = true;

	/**
	 * Input action for moving selection up.
	 * Leave empty to use default (Up Arrow and W keys).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Keyboard", meta = (EditCondition = "bEnableKeyboardNavigation"))
	FKey NavigateUpKey = EKeys::Up;

	/**
	 * Alternative input action for moving selection up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Keyboard", meta = (EditCondition = "bEnableKeyboardNavigation"))
	FKey NavigateUpKeyAlt = EKeys::W;

	/**
	 * Input action for moving selection down.
	 * Leave empty to use default (Down Arrow and S keys).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Keyboard", meta = (EditCondition = "bEnableKeyboardNavigation"))
	FKey NavigateDownKey = EKeys::Down;

	/**
	 * Alternative input action for moving selection down.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Keyboard", meta = (EditCondition = "bEnableKeyboardNavigation"))
	FKey NavigateDownKeyAlt = EKeys::S;

	/**
	 * Input action for confirming selection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Keyboard", meta = (EditCondition = "bEnableKeyboardNavigation"))
	FKey ConfirmKey = EKeys::Enter;

	/**
	 * Alternative input action for confirming selection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Keyboard", meta = (EditCondition = "bEnableKeyboardNavigation"))
	FKey ConfirmKeyAlt = EKeys::SpaceBar;

	// ========================================================================
	// Events
	// ========================================================================

	/**
	 * Called when an option is selected via the UI.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnOptionSelectedBP OnOptionSelectedBP;

	/**
	 * Called when options have finished displaying (after fade-in).
	 */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnOptionsDisplayComplete OnOptionsDisplayComplete;

	/**
	 * Called when options have been dismissed (after fade-out).
	 */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnOptionsDismissed OnOptionsDismissed;

	// ========================================================================
	// Public Methods
	// ========================================================================

	/**
	 * Called by option widgets when selected.
	 * @param OptionIndex The index of the selected option.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void HandleOptionSelected(int32 OptionIndex);

	/**
	 * Get the currently highlighted option index.
	 * @return The index of the highlighted option, or -1 if none.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	int32 GetSelectedOptionIndex() const { return SelectedOptionIndex; }

	/**
	 * Select an option by index (keyboard navigation).
	 * @param OptionIndex The index to select.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void SelectOptionByIndex(int32 OptionIndex);

	/**
	 * Move selection to the next available option.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void SelectNextOption();

	/**
	 * Move selection to the previous available option.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void SelectPreviousOption();

	/**
	 * Confirm the currently selected option.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void ConfirmSelectedOption();

	/**
	 * Check if options are currently being displayed.
	 * @return True if options are visible and interactive.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	bool AreOptionsVisible() const { return bOptionsVisible; }

protected:
	// ========================================================================
	// Internal State
	// ========================================================================

	/** Pool of option widgets for reuse */
	UPROPERTY()
	TArray<UYarnOptionWidget*> OptionWidgetPool;

	/** Currently active (visible) option widgets */
	UPROPERTY()
	TArray<UYarnOptionWidget*> ActiveOptionWidgets;

	/** The last line seen (for showing before options) */
	FYarnLocalizedLine LastSeenLine;

	/** Whether we have a last line to show */
	bool bHasLastLine;

	/** Whether options are currently visible */
	bool bOptionsVisible;

	/** Whether we're currently fading */
	bool bIsFading;

	/** Whether we're fading in (true) or out (false) */
	bool bFadingIn;

	/** Current fade progress (0-1) */
	float FadeProgress;

	/** The currently highlighted option index for keyboard navigation */
	int32 SelectedOptionIndex;

	/** Whether a selection has been submitted (prevents double-selection) */
	bool bHasSubmittedSelection;

	/** Markup name for truncating last line text */
	static const FString TruncateLastLineMarkupName;

	// ========================================================================
	// Internal Methods
	// ========================================================================

	/** Create a new option widget */
	UYarnOptionWidget* CreateOptionWidget();

	/** Clear all visible options */
	void ClearOptions();

	/** Set visibility of the options container */
	void SetOptionsContainerVisible(bool bVisible);

	/** Set the fade widget opacity */
	void SetFadeOpacity(float Opacity);

	/** Start fade in effect */
	void StartFadeIn();

	/** Start fade out effect */
	void StartFadeOut();

	/** Called when fade completes */
	void OnFadeComplete();

	/** Update fade animation */
	void UpdateFade(float DeltaTime);

	/** Handle keyboard input for navigation */
	void HandleKeyboardInput();

	/** Find the first available option index */
	int32 FindFirstAvailableOptionIndex() const;

	/** Find the next available option index from current */
	int32 FindNextAvailableOptionIndex(int32 FromIndex, bool bSearchForward) const;

	/** Update visual selection state on option widgets */
	void UpdateSelectionVisuals();

	/** Process the last line text (handle truncation markup) */
	FString ProcessLastLineText(const FYarnLocalizedLine& Line) const;
};

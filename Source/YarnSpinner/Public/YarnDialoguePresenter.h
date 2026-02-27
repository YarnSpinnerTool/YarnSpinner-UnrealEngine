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

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// Unreal core - provides fundamental types like FString, TArray, etc.
#include "CoreMinimal.h"

// UActorComponent - presenters are components that can be attached to actors.
#include "Components/ActorComponent.h"

// Core yarn types - FYarnValue, FYarnLine, FYarnLocalizedLine, FYarnOptionSet.
// These are the data structures passed to presenters for display.
#include "YarnSpinnerCore.h"

// FYarnLineCancellationToken - allows presenters to check if the player has
// requested hurry-up or skip.
#include "YarnCancellationToken.h"

// Required by Unreal's reflection system
#include "YarnDialoguePresenter.generated.h"

class UYarnDialogueRunner;

// ============================================================================
// UYarnDialoguePresenter
// ============================================================================
//
// WHAT THIS IS:
// Presenters are responsible for showing dialogue content to the player. They
// receive lines and options from the dialogue runner and display them however
// makes sense for your game - text boxes, voice-over, animations, etc.
//
// BLUEPRINT VS C++ USAGE:
// Blueprint users:
//   - derive a blueprint from this class (or UYarnWidgetPresenter)
//   - override RunLine to display text (call OnLinePresentationComplete when done)
//   - override RunOptions to show choices (call OnOptionSelected with the index)
//   - use IsHurryUpRequested() to check if animations should skip
//
// C++ users:
//   - derive from this class and override _Implementation methods
//   - or use the built-in UYarnWidgetPresenter and UYarnVoiceOverPresenter
//   - can access DialogueRunner directly for more control
//
// MULTIPLE PRESENTERS:
// You can (and often should) have multiple presenters:
//   - one for text display (e.g., UYarnWidgetPresenter)
//   - one for voice-over audio (e.g., UYarnVoiceOverPresenter)
//   - one for character animations
// They all receive the same content and run in parallel.
/**
 * base class for dialogue presenters.
 *
 * presenters display lines and options to the player. you can have multiple
 * presenters working together - one for text, one for audio, etc.
 *
 * @see UYarnWidgetPresenter for a ready-to-use text presenter
 * @see UYarnVoiceOverPresenter for voice-over playback
 */
UCLASS(Abstract, ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnDialoguePresenter : public UActorComponent
{
	GENERATED_BODY()

public:
	UYarnDialoguePresenter();

	// ========================================================================
	// lifecycle
	// ========================================================================

	/**
	 * called when the component is being destroyed or the level is ending.
	 * cleans up any pending timers (auto-advance) to prevent crashes.
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========================================================================
	// dialogue lifecycle events
	// ========================================================================
	// These are called by the dialogue runner at various points. Override them
	// to respond to dialogue state changes (e.g., show/hide your UI).

	/**
	 * called when dialogue starts.
	 * use this to prepare your ui - show the dialogue canvas, fade in, etc.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void OnDialogueStarted();
	virtual void OnDialogueStarted_Implementation();

	/**
	 * called when dialogue completes.
	 * use this to clean up your ui - hide the dialogue canvas, fade out, etc.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void OnDialogueComplete();
	virtual void OnDialogueComplete_Implementation();

	/**
	 * called when a new node starts executing.
	 * useful for tracking which conversation branch we're in, or for
	 * triggering node-specific effects.
	 * @param NodeName the name of the node being entered.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void OnNodeEnter(const FString& NodeName);
	virtual void OnNodeEnter_Implementation(const FString& NodeName);

	/**
	 * called when a node finishes executing.
	 * @param NodeName the name of the node being exited.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void OnNodeExit(const FString& NodeName);
	virtual void OnNodeExit_Implementation(const FString& NodeName);

	/**
	 * called when a node is about to start, providing line IDs for pre-loading.
	 * use this to pre-load localisation, audio, or other assets for upcoming lines.
	 * @param LineIDs the line IDs that will be needed in this node.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void OnPrepareForLines(const TArray<FString>& LineIDs);
	virtual void OnPrepareForLines_Implementation(const TArray<FString>& LineIDs);

	// ========================================================================
	// content presentation (override these)
	// ========================================================================
	// These are the main methods you'll override to display dialogue content.
	// When you're done presenting, you MUST call the corresponding completion
	// method (OnLinePresentationComplete or OnOptionSelected).

	/**
	 * present a line of dialogue to the player.
	 *
	 * this is called for every line in the yarn script. override this to
	 * display the text, play animations, etc. when you're done (or the player
	 * skips), you MUST call OnLinePresentationComplete().
	 *
	 * @param Line the localised line to present (includes text, character name, metadata).
	 * @param bCanHurry whether the player can hurry up this line presentation.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void RunLine(const FYarnLocalizedLine& Line, bool bCanHurry);
	virtual void RunLine_Implementation(const FYarnLocalizedLine& Line, bool bCanHurry);

	/**
	 * present a set of options for the player to choose from.
	 *
	 * this is called when yarn reaches a shortcut option or jump choice.
	 * override this to display the options. when the player selects one,
	 * call OnOptionSelected(index).
	 *
	 * @param Options the set of options to present.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void RunOptions(const FYarnOptionSet& Options);
	virtual void RunOptions_Implementation(const FYarnOptionSet& Options);

	/**
	 * called when the player requests hurry-up (usually first button press).
	 * speed up any ongoing presentation - complete typewriter instantly,
	 * skip delays, etc. but don't advance to the next line yet.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void OnHurryUpRequested();
	virtual void OnHurryUpRequested_Implementation();

	/**
	 * called when the player requests next content (usually second button press).
	 * finish current line presentation immediately and move on.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void OnNextLineRequested();
	virtual void OnNextLineRequested_Implementation();

	/**
	 * called when the player requests hurry-up for options display.
	 * speed up any ongoing option presentation - complete animations,
	 * skip delays, etc. but don't make a selection yet.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Presenter")
	void OnOptionsHurryUpRequested();
	virtual void OnOptionsHurryUpRequested_Implementation();

	// ========================================================================
	// completion signalling (call these when you're done)
	// ========================================================================
	// You must call one of these when you finish presenting content. The dialogue
	// runner waits for all presenters to signal completion before continuing to
	// the next content.

	/**
	 * call this when you've finished presenting a line.
	 *
	 * this tells the dialogue runner that this presenter is done with the
	 * current line. the runner waits for ALL presenters to call this before
	 * moving to the next content.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Presenter")
	void OnLinePresentationComplete();

	/**
	 * call this when the player selects an option.
	 *
	 * this tells the dialogue runner which option was chosen and triggers
	 * continuation to that option's content.
	 *
	 * @param OptionIndex the 0-based index of the selected option.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Presenter")
	void OnOptionSelected(int32 OptionIndex);

	/**
	 * request that the dialogue runner continue to the next content.
	 * this is a lower-level method - usually you'll use OnLinePresentationComplete.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Presenter")
	void RequestContinue();

	// ========================================================================
	// dialogue runner access
	// ========================================================================

	/**
	 * get the dialogue runner this presenter is attached to.
	 * @return the dialogue runner, or nullptr if not attached.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Presenter")
	UYarnDialogueRunner* GetDialogueRunner() const { return DialogueRunner; }

	// ========================================================================
	// cancellation token access
	// ========================================================================
	// Use these to check if the player has requested hurry-up or skip.
	// This is how the two-tier interruption system works (see YarnCancellationToken.h).

	/**
	 * check if hurry-up has been requested.
	 *
	 * when true, you should speed up presentation (e.g., complete typewriter
	 * instantly, skip delays) but NOT finish and advance yet.
	 *
	 * in blueprints, poll this periodically during presentation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Presenter")
	bool IsHurryUpRequested() const;

	/**
	 * check if next content has been requested.
	 *
	 * when true, you should finish presentation immediately and call
	 * OnLinePresentationComplete.
	 *
	 * in blueprints, poll this periodically during presentation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Presenter")
	bool IsNextContentRequested() const;

	/**
	 * check if hurry-up has been requested for options.
	 *
	 * when true, you should speed up option presentation (e.g., complete
	 * animations, skip delays) but NOT make a selection yet.
	 *
	 * in blueprints, poll this periodically during options presentation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Presenter")
	bool IsOptionHurryUpRequested() const;

	/**
	 * check if option selection should be cancelled.
	 *
	 * when true, the options presentation is being cancelled (e.g., dialogue
	 * is stopping). clean up your options UI immediately.
	 *
	 * in blueprints, poll this periodically during options presentation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Presenter")
	bool IsOptionNextContentRequested() const;

	// ========================================================================
	// auto-advance mode
	// ========================================================================
	// Auto-advance automatically moves to the next line after a delay. This is
	// useful for cutscenes, visual novels, or accessibility features.
	//
	// The delay is calculated as: min(MinDelay + CharCount * TimePerChar, MaxDelay).
	// This means longer lines get more reading time, up to a maximum.

	/**
	 * whether auto-advance mode is enabled.
	 * when enabled, lines will automatically advance after a calculated delay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Auto Advance")
	bool bAutoAdvanceEnabled = false;

	/**
	 * the minimum time to wait before auto-advancing (seconds).
	 * even short lines will wait at least this long.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Auto Advance", meta = (EditCondition = "bAutoAdvanceEnabled", ClampMin = "0.0"))
	float AutoAdvanceMinDelay = 2.0f;

	/**
	 * additional time per character for auto-advance (seconds).
	 * gives longer lines more reading time.
	 * total delay = MinDelay + (CharacterCount * TimePerCharacter)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Auto Advance", meta = (EditCondition = "bAutoAdvanceEnabled", ClampMin = "0.0"))
	float AutoAdvanceTimePerCharacter = 0.05f;

	/**
	 * maximum time to wait before auto-advancing (seconds).
	 * prevents extremely long waits on very long lines.
	 * set to 0 for no maximum.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Auto Advance", meta = (EditCondition = "bAutoAdvanceEnabled", ClampMin = "0.0"))
	float AutoAdvanceMaxDelay = 10.0f;

	/**
	 * enable or disable auto-advance mode at runtime.
	 * @param bEnabled whether to enable auto-advance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Auto Advance")
	void SetAutoAdvanceEnabled(bool bEnabled);

	/**
	 * check if auto-advance mode is currently enabled.
	 * @return true if auto-advance is enabled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Auto Advance")
	bool IsAutoAdvanceEnabled() const { return bAutoAdvanceEnabled; }

	/**
	 * calculate the auto-advance delay for a line based on character count.
	 * uses the formula: clamp(MinDelay + CharCount * TimePerChar, MinDelay, MaxDelay)
	 * @param CharacterCount the number of characters in the line.
	 * @return the delay in seconds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Auto Advance")
	float CalculateAutoAdvanceDelay(int32 CharacterCount) const;

	/**
	 * start the auto-advance timer for the current line.
	 * call this when line presentation completes and you want auto-advance to kick in.
	 * the timer will fire OnLinePresentationComplete when it expires.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Auto Advance")
	void StartAutoAdvanceTimer();

	/**
	 * cancel any pending auto-advance timer.
	 * useful if the player manually advances or if dialogue is interrupted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Auto Advance")
	void CancelAutoAdvanceTimer();

protected:
	// ========================================================================
	// internal state
	// ========================================================================
	// These are accessible from subclasses and blueprint-readable for debugging.

	/** the dialogue runner this presenter is attached to. set by the runner. */
	UPROPERTY()
	UYarnDialogueRunner* DialogueRunner;

	/** whether we're currently presenting a line (between RunLine and completion) */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Presenter")
	bool bIsPresentingLine = false;

	/** whether we're currently presenting options (between RunOptions and selection) */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Presenter")
	bool bIsPresentingOptions = false;

	/** the current line being presented. access this in subclasses for the text, character, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Presenter")
	FYarnLocalizedLine CurrentLine;

	/** the current options being presented. access this in subclasses for option text. */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Presenter")
	FYarnOptionSet CurrentOptions;

	// Dialogue runner needs access to internal methods
	friend class UYarnDialogueRunner;

	/** set the dialogue runner (called by the runner when presenter is registered) */
	void SetDialogueRunner(UYarnDialogueRunner* Runner);

	/** internal method to start line presentation. sets up state before calling RunLine. */
	void Internal_RunLine(const FYarnLocalizedLine& Line, bool bCanHurry);

	/** internal method to start options presentation. sets up state before calling RunOptions. */
	void Internal_RunOptions(const FYarnOptionSet& Options);

private:
	// ========================================================================
	// auto-advance timer
	// ========================================================================

	/** timer handle for auto-advance. cleared in EndPlay to prevent crashes. */
	FTimerHandle AutoAdvanceTimerHandle;

	/** called when auto-advance timer fires. advances to next line. */
	void OnAutoAdvanceTimerFired();
};

// ============================================================================
// UYarnLineProvider
// ============================================================================
//
// WHAT THIS IS:
// Line providers are responsible for looking up the text for dialogue lines.
// The base implementation just returns the text from the yarn project's base
// string table. The builtin line provider (UYarnBuiltinLineProvider in
// YarnLocalization.h) adds full localisation support.
//
// WHEN TO OVERRIDE:
// - if you have a custom localisation system (e.g., not using string tables)
// - if you want to load text from a different source (database, web service)
// - if you want to modify text before display (censoring, name substitution)

/**
 * base class for line providers. handles text lookup for dialogue lines.
 *
 * the default implementation returns text from the yarn project's base string
 * table. for localisation support, use UYarnBuiltinLineProvider instead.
 *
 * @see UYarnBuiltinLineProvider for full localisation support
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API UYarnLineProvider : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * set the yarn project to use for looking up text.
	 * this must be called before GetLocalizedLine will work.
	 * @param Project the yarn project containing line text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Line Provider")
	virtual void SetYarnProject(UYarnProject* Project);

	/**
	 * get the localised text for a line.
	 *
	 * override this in subclasses or blueprints to provide custom text lookup.
	 * the default implementation returns text from the yarn project's base
	 * string table without any localisation.
	 *
	 * @param Line the raw line data (contains LineID and substitutions).
	 * @return the localised line ready for presentation.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Line Provider")
	FYarnLocalizedLine GetLocalizedLine(const FYarnLine& Line);
	virtual FYarnLocalizedLine GetLocalizedLine_Implementation(const FYarnLine& Line);

protected:
	/** the yarn project used for text lookup */
	UPROPERTY()
	UYarnProject* YarnProject;
};

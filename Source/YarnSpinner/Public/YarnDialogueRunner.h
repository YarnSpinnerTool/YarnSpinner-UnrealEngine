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
// Includes
// ----------------------------------------------------------------------------

// Unreal core - provides fundamental types like FString, TArray, etc
#include "CoreMinimal.h"

// UActorComponent - base class. Dialogue runner is a component you attach to
// an actor.
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"

// Core yarn types - FYarnValue, FYarnLine, FYarnLocalizedLine, FYarnOptionSet,
// FYarnCommand. These are the basic data structures used throughout.
#include "YarnSpinnerCore.h"

// UYarnProject - the compiled dialogue asset that contains nodes, lines, and
// localisations. You assign one of these in the editor.
#include "YarnProgram.h"

// IYarnVariableStorage - interface for storing yarn variables. The runner
// creates an in-memory storage by default, but you can provide your own.
#include "YarnVariableStorage.h"

// FYarnLineCancellationToken - allows presenters to check if the player has
// requested hurry-up or skip.
#include "YarnCancellationToken.h"

#include "YarnDialogueInstance.h"

// EYarnLineCompletionRequest, FOnYarnLineFinished, FOnYarnOptionSelected -
// shared types crossing the presenter <-> runner boundary.
#include "YarnPresenterTypes.h"

// EYarnSaliencyStrategy, IYarnSaliencyStrategy - for smart content selection.
// Saliency helps pick the best content when multiple options are available.
#include "YarnSaliency.h"

// IYarnSmartVariableEvaluator - for evaluating compiled smart variable nodes.
// The dialogue runner implements this interface.
#include "YarnSmartVariables.h"

// Required by Unreal's reflection system
#include "YarnDialogueRunner.generated.h"

class UYarnDialoguePresenter;
class UYarnLineProvider;
class IYarnSaliencyStrategy;

// ============================================================================
// Blueprint delegates
// ============================================================================
// These are the event types that blueprints can bind to. We use dynamic
// multicast delegates so multiple listeners can subscribe in blueprints.

/** Fired when a node starts executing. Passes the node name. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYarnNodeStartBP, const FString&, NodeName);

/** Fired when a node finishes executing. Passes the node name. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYarnNodeCompleteBP, const FString&, NodeName);

/** Fired when dialogue begins (first node starts). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnDialogueStartBP);

/** Fired when dialogue ends (no more content). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnDialogueCompleteBP);

/** Fired when a command is received but no handler is registered. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYarnUnhandledCommandBP, const FString&, CommandText);

// ============================================================================
// UYarnDialogueRunner
// ============================================================================
// WHAT THIS IS:
// The dialogue runner is the main entry point for Yarn Spinner in your game.
// HOW IT WORKS:
// 1. Attach this component to an actor
// 2. Assign a yarn project (compiled dialogue)
// 3. Add one or more presenters (to display lines/options)
// 4. Call StartDialogue("NodeName") or set bAutoStart = true
// BLUEPRINT VS C++ USAGE:
// - All control methods are BlueprintCallable (StartDialogue, StopDialogue, etc)
// - All events are BlueprintAssignable (OnDialogueStart, OnNodeStart, etc)
// - Configuration properties are editable in the editor
// - Command handlers and custom functions are C++ only (use AddCommandHandler)
// - For Blueprint command handling, bind to OnUnhandledCommand and parse there

 /**
 * The Yarn Spinner dialogue runner component.
 * This is the main component for running Yarn Spinner dialogue in your game.
 * Attach this to an actor and configure it with a yarn project and presenters.
 * The dialogue runner implements IYarnSmartVariableEvaluator so it can evaluate
 * smart variables and sets itself as the evaluator on variable storage.
 * @see UYarnDialoguePresenter for displaying dialogue
 * @see UYarnProject for the compiled dialogue asset
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable)
class YARNSPINNER_API UYarnDialogueRunner : public UActorComponent, public IYarnSmartVariableEvaluator
{
	GENERATED_BODY()

public:
	UYarnDialogueRunner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========================================================================
	// Configuration - assign these in the editor
	// ========================================================================

	 /**
	 * The yarn project containing the dialogue to run.
	 * This is required - without a project, there's nothing to run.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	TObjectPtr<UYarnProject> YarnProject;

	 /**
	 * The variable storage to use for yarn variables ($variableName).
	 * If not set, an in-memory storage will be created automatically.
	 * Provide your own to persist variables across sessions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner", meta = (MustImplement = "/Script/YarnSpinner.YarnVariableStorage"))
	TScriptInterface<IYarnVariableStorage> VariableStorage;

	 /**
	 * The dialogue presenters that will display lines and options.
	 * You can have multiple - e.g., one for text, one for voice-over.
	 * All presenters receive all content and handle it in parallel.
	 * The Details panel can't assign component instances to a raw object
	 * array — wire presenters via DialoguePresenterReferences in the editor,
	 * or add them to this array at runtime from Blueprints/C++.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Yarn Spinner")
	TArray<TObjectPtr<UYarnDialoguePresenter>> DialoguePresenters;

	 /**
	 * Editor-assigned presenter components, resolved into DialoguePresenters
	 * at BeginPlay. Supports presenters on this actor or (via the picker)
	 * any other actor in the level.
	 */
	UPROPERTY(EditAnywhere, Category = "Yarn Spinner", meta = (UseComponentPicker, AllowAnyActor, AllowedClasses = "/Script/YarnSpinner.YarnDialoguePresenter", DisplayName = "Dialogue Presenters"))
	TArray<FComponentReference> DialoguePresenterReferences;

	 /**
	 * The line provider for localisation.
	 * If not set, uses base text from the yarn project.
	 * Set this to a UYarnBuiltinLineProvider for full localisation support.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	TObjectPtr<UYarnLineProvider> LineProvider;

	 /**
	 * Objects containing command handler functions for Blueprint-based commands.
	 * Add any UObject (Blueprint, Actor, etc) here. When a command is executed,
	 * Yarn Spinner looks for a function with the same name as the command.
	 * For example, <<play_sound "boom">> looks for a function named "play_sound".
	 * Function parameters are automatically converted from strings:
	 * - FString parameters receive the raw argument
	 * - float/int parameters are parsed from numeric strings
	 * - bool parameters accept "true"/"false"
	 * - FVector, FRotator, etc use UE's standard text parsing
	 * This allows 100% Blueprint-based command handling with no C++ required.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Yarn Spinner|Commands", meta = (DisplayName = "Command Handler Objects"))
	TArray<TObjectPtr<UObject>> CommandHandlerObjects;

	 /**
	 * Whether to start dialogue automatically when the game starts.
	 * If true, starts from StartNode on BeginPlay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Behaviour")
	bool bAutoStart = false;

	 /**
	 * The node to start from when auto-starting or calling StartDialogueFromStart.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Behaviour", meta = (EditCondition = "bAutoStart"))
	FString StartNode = TEXT("Start");

	 /**
	 * If true, when the player selects an option, it will be run as a line
	 * (with voice-over, etc) before continuing to the next content.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Behaviour")
	bool bRunSelectedOptionAsLine = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Behaviour")
	bool bAllowOptionFallthrough = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Behaviour")
	bool bContinueOnUnhandledCommand = true;

	 /**
	 * The saliency strategy for smart content selection.
	 * Used when yarn scripts use the <<select>> command or similar.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Behaviour")
	EYarnSaliencyStrategy SaliencyStrategy = EYarnSaliencyStrategy::RandomBestLeastRecentlyViewed;

	 /**
	 * Whether to log verbose debug information to the output log.
	 * Useful for debugging dialogue flow issues.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Debug")
	bool bVerboseLogging = false;

	// ========================================================================
	// Events - bind to these in Blueprints or C++
	// ========================================================================

	/** Fired when a node starts executing */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnNodeStartBP OnNodeStart;

	/** Fired when a node finishes executing */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnNodeCompleteBP OnNodeComplete;

	/** Fired when dialogue begins */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnDialogueStartBP OnDialogueStart;

	/** Fired when dialogue ends */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnDialogueCompleteBP OnDialogueComplete;

	 /**
	 * Fired when a command is not handled by any registered handler.
	 * Useful for Blueprint-based command handling - parse CommandText yourself.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnUnhandledCommandBP OnUnhandledCommand;

	// ========================================================================
	// Dialogue control - call these to control dialogue flow
	// ========================================================================

	 /**
	 * Change which yarn project this runner uses at runtime.
	 * Stops any active dialogue, updates the project reference, rebinds the
	 * virtual machine, and resets presentation state. Existing variables in
	 * storage are preserved (only initial value lookups change).
	 * Useful for level transitions, DLC content, or modding support.
	 * @param NewYarnProject The new yarn project to use
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void SetYarnProject(UYarnProject* NewYarnProject);

	 /**
	 * Start running dialogue from the specified node.
	 * @param NodeName The name of the node to start from
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void StartDialogue(const FString& NodeName);

	 /**
	 * Start running dialogue from the default start node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void StartDialogueFromStart();

	 /**
	 * Stop the current dialogue immediately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void StopDialogue();

	 /**
	 * Check if dialogue is currently running.
	 * @return true if dialogue is active
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner")
	bool IsDialogueRunning() const;

	 /**
	 * Continue to the next content after a presenter signals completion.
	 * Typically called by presenters, but can be called manually.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void Continue();

	 /**
	 * Request that presenters speed up their presentation.
	 * E.g., complete a typewriter effect instantly but don't advance.
	 * Presenters check IsHurryUpRequested() to respond.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void RequestHurryUp();

	 /**
	 * Request that presenters finish and advance to the next content.
	 * E.g., skip the current line entirely.
	 * Presenters check IsNextContentRequested() to respond.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void RequestNextLine();

	 /**
	 * Request that option presenters speed up their display.
	 * This signals option presenters to hurry up their presentation, similar
	 * to RequestHurryUp() for lines. Useful for speeding up option animations
	 * or typewriter effects on option text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void RequestHurryUpOption();

	 /**
	 * Get a cancellation token observing the current line's cancellation
	 * source. Presenters use this to check hurry-up and skip requests.
	 * Returned by value: the token itself is a lightweight handle. The
	 * actual state lives in the runner's UYarnCancellationTokenSource.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	FYarnLineCancellationToken GetCurrentCancellationToken();

	 /**
	 * Get a cancellation token observing the current options presentation's
	 * cancellation source. Option presenters use this for hurry-up checks.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	FYarnLineCancellationToken GetCurrentOptionsCancellationToken();

	/** Const version of hurry-up check for presenters */
	bool IsHurryUpRequested() const;

	/** Const version of next-content check for presenters */
	bool IsNextContentRequested() const;

	/** Const version of option hurry-up check for presenters */
	bool IsOptionHurryUpRequested() const;

	/** Const version of option next-content check for presenters */
	bool IsOptionNextContentRequested() const;

	 /**
	 * Select an option by its index.
	 * Call this when the player chooses an option from RunOptions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void SelectOption(int32 OptionIndex);

	 /**
	 * Get the name of the currently executing node.
	 * @return The node name, or empty if not running
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner")
	FString GetCurrentNodeName() const;

	// ========================================================================
	// Command and function registration (C++ only)
	// ========================================================================
	// For Blueprint command handling, use OnUnhandledCommand event instead

	 /**
	 * Add a handler for a specific command (C++ only).
	 * When yarn executes <<commandName args>>, your handler is called.
	 * @param CommandName The command name (without <<>>)
	 * @param Handler The function to call with command parameters
	 */
	void AddCommandHandler(const FString& CommandName, TFunction<void(const TArray<FString>&)> Handler);

	 /**
	 * Remove a command handler (C++ only).
	 * @param CommandName The command name to remove
	 */
	void RemoveCommandHandler(const FString& CommandName);

	 /**
	 * Add a blocking command handler (C++ only). Like AddCommandHandler,
	 * but dialogue does not continue when the handler returns — it stays
	 * paused until CompleteBlockingCommand() is called. This is the same
	 * mechanism the built-in <<wait>> command uses.
	 * @param CommandName The command name (without <<>>)
	 * @param Handler The function to call with command parameters
	 */
	void AddBlockingCommandHandler(const FString& CommandName, TFunction<void(const TArray<FString>&)> Handler);

	 /**
	 * Resume dialogue after a blocking command handler has finished its
	 * work. Safe to call from Blueprints; does nothing unless a blocking
	 * command is currently holding the dialogue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Commands")
	void CompleteBlockingCommand();

	 /**
	 * Add a function callable from yarn scripts (C++ only).
	 * Yarn can then call {functionName(args)} in expressions.
	 * @param FunctionName The function name
	 * @param Function The function implementation
	 * @param ParameterCount Expected parameter count
	 */
	void AddFunction(const FString& FunctionName, TFunction<FYarnValue(const TArray<FYarnValue>&)> Function, int32 ParameterCount);

	 /**
	 * Remove a function (C++ only).
	 * @param FunctionName The function name to remove
	 */
	void RemoveFunction(const FString& FunctionName);

	void ReportFunctionError(const FString& Message);

	// ========================================================================
	// Saliency strategy
	// ========================================================================

	 /**
	 * Install a custom saliency strategy.
	 * Replaces whichever strategy was created from the SaliencyStrategy enum.
	 * Pass any UObject implementing IYarnSaliencyStrategy (C++ or Blueprint).
	 * Safe to call before or after BeginPlay - if called before, the runner's
	 * BeginPlay won't overwrite it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Saliency")
	void SetSaliencyStrategy(TScriptInterface<IYarnSaliencyStrategy> InStrategy);

	/** Get the currently active saliency strategy. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Saliency")
	TScriptInterface<IYarnSaliencyStrategy> GetSaliencyStrategy() const { return Instance ? Instance->GetActiveSaliencyStrategy() : TScriptInterface<IYarnSaliencyStrategy>(); }

	// ========================================================================
	// IYarnSmartVariableEvaluator implementation
	// ========================================================================

	virtual bool TryGetSmartVariableAsBool(const FString& Name, bool& OutResult) override;
	virtual bool TryGetSmartVariableAsFloat(const FString& Name, float& OutResult) override;
	virtual bool TryGetSmartVariableAsString(const FString& Name, FString& OutResult) override;
	virtual bool TryGetSmartVariable(const FString& Name, FYarnValue& OutResult) override;

	 /**
	 * Called by a presenter when it finishes presenting a line.
	 * Legacy back-compat path. New flow is the FOnYarnLineFinished callback
	 * bound in HandleLine; this stays so presenters that bypass the new
	 * flow (e.g. legacy code that calls back via the runner pointer) still
	 * work.
	 */
	void NotifyPresenterLineComplete();

	 /**
	 * Fired by a presenter via its FOnYarnLineFinished delegate when it
	 * finishes presenting a line. Receives the presenter's request about
	 * the line as a whole (see EYarnLineCompletionRequest).
	 * Decrements the outstanding count, honours an EndLine request by
	 * cancelling the current content source, and calls Continue once all
	 * presenters have signalled.
	 */
	UFUNCTION()
	void HandlePresenterLineFinished(EYarnLineCompletionRequest Request);

	 /**
	 * Fired by a presenter via its FOnYarnOptionSelected delegate when the
	 * player picks an option. Forwards to SelectOption.
	 */
	UFUNCTION()
	void HandlePresenterOptionSelected(int32 OptionIndex);

protected:
	// ========================================================================
	// Internal state
	// ========================================================================

	UPROPERTY(Transient)
	TObjectPtr<UYarnDialogueInstance> Instance;

	UYarnDialogueInstance* GetOrCreateInstance();
};

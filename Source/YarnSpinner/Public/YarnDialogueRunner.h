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

// Core yarn types - FYarnValue, FYarnLine, FYarnLocalizedLine, FYarnOptionSet,
// FYarnCommand. These are the basic data structures used throughout.
#include "YarnSpinnerCore.h"

// UYarnProject - the compiled dialogue asset that contains nodes, lines, and
// localisations. You assign one of these in the editor.
#include "YarnProgram.h"

// IYarnVariableStorage - interface for storing yarn variables. The runner
// creates an in-memory storage by default, but you can provide your own.
#include "YarnVariableStorage.h"

// FYarnVirtualMachine - the core VM that executes yarn bytecode. This is an
// internal implementation detail, not exposed to blueprints.
#include "YarnVirtualMachine.h"

// FYarnLineCancellationToken - allows presenters to check if the player has
// requested hurry-up or skip.
#include "YarnCancellationToken.h"

// EYarnSaliencyStrategy, IYarnSaliencyStrategy - for smart content selection.
// Saliency helps pick the best content when multiple options are available.
#include "YarnSaliency.h"

// IYarnSmartVariableEvaluator - for evaluating compiled smart variable nodes.
// The dialogue runner implements this interface.
#include "YarnSmartVariables.h"

// Version detection for cross-engine compatibility (UE4/UE5)
#include "YarnSpinnerVersion.h"

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
//
// WHAT THIS IS:
// The dialogue runner is the main entry point for Yarn Spinner in your game.
//
// HOW IT WORKS:
// 1. Attach this component to an actor
// 2. Assign a yarn project (compiled dialogue)
// 3. Add one or more presenters (to display lines/options)
// 4. Call StartDialogue("NodeName") or set bAutoStart = true
//
// BLUEPRINT VS C++ USAGE:
// - All control methods are BlueprintCallable (StartDialogue, StopDialogue, etc)
// - All events are BlueprintAssignable (OnDialogueStart, OnNodeStart, etc)
// - Configuration properties are editable in the editor
// - Command handlers and custom functions are C++ only (use AddCommandHandler)
// - For Blueprint command handling, bind to OnUnhandledCommand and parse there

/**
 * The Yarn Spinner dialogue runner component.
 *
 * This is the main component for running Yarn Spinner dialogue in your game.
 * Attach this to an actor and configure it with a yarn project and presenters.
 *
 * The dialogue runner implements IYarnSmartVariableEvaluator so it can evaluate
 * smart variables and sets itself as the evaluator on variable storage.
 *
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
	UYarnProject* YarnProject;

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
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner", meta = (UseComponentPicker))
	TArray<UYarnDialoguePresenter*> DialoguePresenters;

	/**
	 * The line provider for localisation.
	 * If not set, uses base text from the yarn project.
	 * Set this to a UYarnBuiltinLineProvider for full localisation support.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	UYarnLineProvider* LineProvider;

	/**
	 * Objects containing command handler functions for Blueprint-based commands.
	 *
	 * Add any UObject (Blueprint, Actor, etc) here. When a command is executed,
	 * Yarn Spinner looks for a function with the same name as the command.
	 * For example, <<play_sound "boom">> looks for a function named "play_sound".
	 *
	 * Function parameters are automatically converted from strings:
	 * - FString parameters receive the raw argument
	 * - float/int parameters are parsed from numeric strings
	 * - bool parameters accept "true"/"false"
	 * - FVector, FRotator, etc use UE's standard text parsing
	 *
	 * This allows 100% Blueprint-based command handling with no C++ required.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Yarn Spinner|Commands", meta = (DisplayName = "Command Handler Objects"))
	TArray<UObject*> CommandHandlerObjects;

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
	 *
	 * Stops any active dialogue, updates the project reference, rebinds the
	 * virtual machine, and resets presentation state. Existing variables in
	 * storage are preserved (only initial value lookups change).
	 *
	 * Useful for level transitions, DLC content, or modding support.
	 *
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
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
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
	 *
	 * This signals option presenters to hurry up their presentation, similar
	 * to RequestHurryUp() for lines. Useful for speeding up option animations
	 * or typewriter effects on option text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void RequestHurryUpOption();

	/**
	 * Get the current cancellation token for line presentation.
	 * Presenters use this to check hurry-up and skip requests.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	FYarnLineCancellationToken& GetCurrentCancellationToken();

	/**
	 * Get the current cancellation token for options presentation.
	 * Option presenters use this to check hurry-up and skip requests.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	FYarnLineCancellationToken& GetCurrentOptionsCancellationToken();

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
	 * @param OptionIndex The index of the selected option (0-based)
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void SelectOption(int32 OptionIndex);

	/**
	 * Get the name of the currently executing node.
	 * @return The node name, or empty if not running
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
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

	// ========================================================================
	// IYarnSmartVariableEvaluator implementation
	// ========================================================================

	virtual bool TryGetSmartVariableAsBool(const FString& Name, bool& OutResult) override;
	virtual bool TryGetSmartVariableAsFloat(const FString& Name, float& OutResult) override;
	virtual bool TryGetSmartVariableAsString(const FString& Name, FString& OutResult) override;
	virtual bool TryGetSmartVariable(const FString& Name, FYarnValue& OutResult) override;

protected:
	// ========================================================================
	// Internal state
	// ========================================================================

	/** The virtual machine that executes yarn bytecode */
	FYarnVirtualMachine VirtualMachine;

	/** Registered command handlers (command name -> handler function) */
	TMap<FString, TFunction<void(const TArray<FString>&)>> CommandHandlers;

	/** Registered yarn functions (function name -> implementation) */
	TMap<FString, TFunction<FYarnValue(const TArray<FYarnValue>&)>> Functions;

	/** Parameter counts for registered functions */
	TMap<FString, int32> FunctionParameterCounts;

	/** Whether hurry-up has been requested */
	bool bHurryUpRequested = false;

	/** Whether next-line has been requested */
	bool bNextLineRequested = false;

	/**
	 * Count of presenters that are still presenting the current line.
	 * Decremented by NotifyPresenterLineComplete; Continue() is called when it reaches 0.
	 */
	int32 ActiveLinePresenterCount = 0;

	/** Current options being displayed (stored for bRunSelectedOptionAsLine) */
	FYarnOptionSet CurrentLocalizedOptions;

	/** Pending option index when running selected option as line (-1 = none) */
	int32 PendingSelectedOptionIndex = -1;

	// ========================================================================
	// Internal methods
	// ========================================================================

	/** Configure the VM with callbacks */
	void SetupVirtualMachine();

	/** Handle a line from the VM - localises and sends to presenters */
	void HandleLine(const FYarnLine& Line);

	/** Handle options from the VM - localises and sends to presenters */
	void HandleOptions(const FYarnOptionSet& Options);

	/** Handle a command from the VM - dispatches to handlers */
	void HandleCommand(const FYarnCommand& Command);

	/**
	 * Try to dispatch a command to a Blueprint handler object.
	 * Searches CommandHandlerObjects for a function matching the command name.
	 * @param Command The command to dispatch
	 * @return true if a handler was found and invoked
	 */
	bool TryDispatchToBlueprintHandler(const FYarnCommand& Command);

	/** Handle node start - fires event */
	void HandleNodeStart(const FString& NodeName);

	/** Handle node complete - fires event */
	void HandleNodeComplete(const FString& NodeName);

	/** Handle dialogue complete - fires event, cleans up */
	void HandleDialogueComplete();

	/** Call a registered function */
	FYarnValue CallFunction(const FString& FunctionName, const TArray<FYarnValue>& Parameters);

	/** Check if a function is registered */
	bool FunctionExists(const FString& FunctionName);

	/** Get parameter count for a function */
	int32 GetFunctionParameterCount(const FString& FunctionName);

	/** Register built-in functions (visited, visited_count, etc) */
	void RegisterBuiltInFunctions();

	/** Get a localised line using the line provider */
	FYarnLocalizedLine GetLocalizedLine(const FYarnLine& Line);

	/** Timer handle for <<wait>> command */
	FTimerHandle WaitTimerHandle;

	/** Called when wait timer completes */
	void OnWaitComplete();

	/** Cancellation token source for line presentation */
	UPROPERTY()
	UYarnCancellationTokenSource* CancellationTokenSource;

	/** Cancellation token source for options presentation */
	UPROPERTY()
	UYarnCancellationTokenSource* OptionsCancellationTokenSource;

	/** The active saliency strategy instance */
	UPROPERTY()
	TScriptInterface<IYarnSaliencyStrategy> ActiveSaliencyStrategy;

	/** Create saliency strategy based on SaliencyStrategy enum */
	void CreateSaliencyStrategy();

	/** Candidates for current saliency selection */
	TArray<FYarnSaliencyCandidate> SaliencyCandidates;

	/** Add a saliency candidate */
	void HandleAddSaliencyCandidate(const FYarnSaliencyCandidate& Candidate);

	/** Select best saliency candidate (called from DialogueRunner methods) */
	bool HandleSelectSaliencyCandidate(FYarnSaliencyCandidate& OutSelectedCandidate);

	/** Select best saliency candidate (called from VM via delegate) */
	bool HandleVMSelectSaliencyCandidate(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate);

	/** Notify strategy that content was selected (called from VM after validation) */
	void HandleContentWasSelected(const FYarnSaliencyCandidate& SelectedCandidate);

	/** Handle prepare for lines (pre-loading for upcoming lines) */
	void HandlePrepareForLines(const TArray<FString>& LineIDs);

	/**
	 * Called by a presenter when it finishes presenting a line.
	 * Decrements ActiveLinePresenterCount and calls Continue() when all presenters are done.
	 */
	void NotifyPresenterLineComplete();

	/** Clear saliency candidates */
	void ClearSaliencyCandidates();
};

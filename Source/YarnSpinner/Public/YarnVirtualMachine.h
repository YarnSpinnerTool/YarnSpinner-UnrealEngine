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
#include "YarnSpinnerCore.h"
#include "YarnProgram.h"
#include "YarnVariableStorage.h"
#include "YarnSaliency.h"
#include "YarnVirtualMachine.generated.h"

/**
 * The execution state of the virtual machine.
 */
UENUM(BlueprintType)
enum class EYarnExecutionState : uint8
{
	/** The VM is not running */
	Stopped UMETA(DisplayName = "Stopped"),

	/** The VM is waiting for an option to be selected */
	WaitingOnOptionSelection UMETA(DisplayName = "Waiting On Option Selection"),

	/** The VM has delivered content and is waiting for Continue to be called */
	WaitingForContinue UMETA(DisplayName = "Waiting For Continue"),

	/** The VM is currently delivering content to handlers */
	DeliveringContent UMETA(DisplayName = "Delivering Content"),

	/** The VM is executing instructions */
	Running UMETA(DisplayName = "Running"),

	/** The VM has encountered an error */
	Error UMETA(DisplayName = "Error")
};

/**
 * Delegate for when a line needs to be presented.
 */
DECLARE_DELEGATE_OneParam(FOnYarnLine, const FYarnLine&);

/**
 * Delegate for when options need to be presented.
 */
DECLARE_DELEGATE_OneParam(FOnYarnOptions, const FYarnOptionSet&);

/**
 * Delegate for when a command needs to be executed.
 */
DECLARE_DELEGATE_OneParam(FOnYarnCommand, const FYarnCommand&);

/**
 * Delegate for when a node starts.
 */
DECLARE_DELEGATE_OneParam(FOnYarnNodeStart, const FString&);

/**
 * Delegate for when a node completes.
 */
DECLARE_DELEGATE_OneParam(FOnYarnNodeComplete, const FString&);

/**
 * Delegate for when the dialogue completes.
 */
DECLARE_DELEGATE(FOnYarnDialogueComplete);

/**
 * Delegate for preparing lines (pre-loading localisation).
 */
DECLARE_DELEGATE_OneParam(FOnYarnPrepareForLines, const TArray<FString>&);

/**
 * Delegate for calling a function.
 * @param FunctionName The name of the function to call.
 * @param Parameters The parameters to pass.
 * @return The return value.
 */
DECLARE_DELEGATE_RetVal_TwoParams(FYarnValue, FOnYarnCallFunction, const FString&, const TArray<FYarnValue>&);

/**
 * Delegate to check if a function exists.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnYarnFunctionExists, const FString&);

/**
 * Delegate to get the expected parameter count for a function.
 */
DECLARE_DELEGATE_RetVal_OneParam(int32, FOnYarnFunctionParamCount, const FString&);

/**
 * Delegate to select the best saliency candidate using the configured strategy.
 * @param Candidates The list of candidates to choose from.
 * @param OutSelectedCandidate The selected candidate (if any).
 * @return True if a candidate was selected.
 */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnYarnSelectSaliencyCandidate, const TArray<FYarnSaliencyCandidate>&, FYarnSaliencyCandidate&);

/**
 * Delegate to notify that saliency content was selected.
 * Called after a saliency candidate is selected and committed.
 * @param SelectedCandidate The candidate that was selected.
 */
DECLARE_DELEGATE_OneParam(FOnYarnContentWasSelected, const FYarnSaliencyCandidate&);

/**
 * The Yarn Spinner virtual machine.
 *
 * Executes compiled Yarn programs by processing instructions
 * and delivering content to registered handlers.
 */
USTRUCT()
struct YARNSPINNER_API FYarnVirtualMachine
{
	GENERATED_BODY()

	FYarnVirtualMachine();

	/**
	 * Set the program to execute.
	 * @param InProgram The compiled Yarn program.
	 */
	void SetProgram(const FYarnProgram& InProgram);

	/**
	 * Get the current program.
	 * @return The current program.
	 */
	const FYarnProgram& GetProgram() const { return Program; }

	/**
	 * Set the node to start execution from.
	 * Clears state (stack, options) by default.
	 * @param NodeName The name of the node to run.
	 * @return True if the node exists and was set.
	 */
	bool SetNode(const FString& NodeName);

	/**
	 * Set the node to start execution from with control over state clearing.
	 * @param NodeName The name of the node to run.
	 * @param bClearState If true, clears stack and options. Use false for detour returns.
	 * @return True if the node exists and was set.
	 */
	bool SetNode(const FString& NodeName, bool bClearState);

	/**
	 * Get the current node name.
	 * @return The name of the currently executing node.
	 */
	const FString& GetCurrentNodeName() const { return CurrentNodeName; }

	/**
	 * Get the current execution state.
	 * @return The current state of the VM.
	 */
	EYarnExecutionState GetExecutionState() const { return ExecutionState; }

	/**
	 * Check if the VM is currently active (not stopped).
	 * @return True if the VM is running.
	 */
	bool IsActive() const { return ExecutionState != EYarnExecutionState::Stopped; }

	/**
	 * Continue execution after content has been handled.
	 * @return True if execution continued successfully.
	 */
	bool Continue();

	/**
	 * Stop execution immediately.
	 */
	void Stop();

	/**
	 * Set the selected option after options have been presented.
	 * @param OptionIndex The index of the selected option.
	 */
	void SetSelectedOption(int32 OptionIndex);

	/**
	 * Signal that content delivery is complete.
	 * Call this when async content presentation is finished.
	 */
	void SignalContentComplete();

	/** Handler called when a line is ready to be presented */
	FOnYarnLine LineHandler;

	/** Handler called when options are ready to be presented */
	FOnYarnOptions OptionsHandler;

	/** Handler called when a command needs to be executed */
	FOnYarnCommand CommandHandler;

	/** Handler called when a node starts */
	FOnYarnNodeStart NodeStartHandler;

	/** Handler called when a node completes */
	FOnYarnNodeComplete NodeCompleteHandler;

	/** Handler called when dialogue completes */
	FOnYarnDialogueComplete DialogueCompleteHandler;

	/** Handler called to prepare for upcoming lines */
	FOnYarnPrepareForLines PrepareForLinesHandler;

	/** Handler to call functions */
	FOnYarnCallFunction CallFunctionHandler;

	/** Handler to check if a function exists */
	FOnYarnFunctionExists FunctionExistsHandler;

	/** Handler to get expected parameter count */
	FOnYarnFunctionParamCount FunctionParamCountHandler;

	/** Handler to select best saliency candidate using configured strategy */
	FOnYarnSelectSaliencyCandidate SelectSaliencyCandidateHandler;

	/** Handler to notify that saliency content was selected (for tracking view counts) */
	FOnYarnContentWasSelected ContentWasSelectedHandler;

	/** Variable storage for reading/writing variables */
	TScriptInterface<IYarnVariableStorage> VariableStorage;

	/**
	 * Expand substitutions in a string.
	 * @param TemplateString The string with {0}, {1} placeholders.
	 * @param Substitutions The values to substitute.
	 * @return The string with substitutions applied.
	 */
	static FString ExpandSubstitutions(const FString& TemplateString, const TArray<FString>& Substitutions);

private:
	/** The program being executed */
	FYarnProgram Program;

	/** The current node being executed */
	const FYarnNode* CurrentNode = nullptr;

	/** The name of the current node */
	FString CurrentNodeName;

	/** The current instruction pointer */
	int32 InstructionPointer = 0;

	/** The current execution state */
	EYarnExecutionState ExecutionState = EYarnExecutionState::Stopped;

	/** The value stack */
	TArray<FYarnValue> Stack;

	/** The current set of options */
	TArray<FYarnOption> CurrentOptions;

	/** The return stack for detours */
	TArray<TPair<FString, int32>> ReturnStack;

	/** The saliency candidates */
	TArray<FYarnSaliencyCandidate> SaliencyCandidates;

	/** Maximum call stack depth for detours (prevents infinite recursion) */
	static constexpr int32 MaxCallStackDepth = 100;

	/** Re-entrancy guard - true when Continue() is executing */
	bool bIsContinuing = false;

	/** Set the execution state (auto-resets state when set to Stopped) */
	void SetExecutionState(EYarnExecutionState NewState);

	/** Reset internal state (stack, options, but not call stack) */
	void ResetState();

	/** Handle returning from a node (fires complete event, updates tracking variable) */
	void ReturnFromNode(const FYarnNode* Node);

	/** Check if we can continue execution */
	bool CheckCanContinue() const;

	/** Run a single instruction */
	bool RunInstruction(const FYarnInstruction& Instruction);

	/** Push a value onto the stack */
	void Push(const FYarnValue& Value);

	/** Pop a value from the stack */
	FYarnValue Pop();

	/** Peek at the top of the stack */
	const FYarnValue& Peek() const;

	/** Pop multiple string values */
	TArray<FString> PopStrings(int32 Count);
};

// Special value indicating no option was selected (for fallthrough)
static constexpr int32 YarnNoOptionSelected = -1;

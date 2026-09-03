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

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"

#include "YarnSpinnerCore.h"

#include "YarnVirtualMachine.h"

#include "YarnCancellationToken.h"

// EYarnLineCompletionRequest, FOnYarnLineFinished, FOnYarnOptionSelected.
#include "YarnPresenterTypes.h"

#include "YarnSaliency.h"

#include "YarnDialogueInstance.generated.h"

class UYarnDialogueRunner;

// ============================================================================

UCLASS()
class YARNSPINNER_API UYarnDialogueInstance : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UYarnDialogueRunner* InOwner);

	// ========================================================================
	// ========================================================================

	void StartDialogue(const FString& NodeName);
	void StopDialogue();
	bool IsDialogueRunning() const;
	void Continue();
	void RequestHurryUp();
	void RequestNextLine();
	void RequestHurryUpOption();
	void SelectOption(int32 OptionIndex);
	FString GetCurrentNodeName() const;

	FYarnLineCancellationToken GetCurrentCancellationToken();
	FYarnLineCancellationToken GetCurrentOptionsCancellationToken();
	bool IsHurryUpRequested() const;
	bool IsNextContentRequested() const;
	bool IsOptionHurryUpRequested() const;
	bool IsOptionNextContentRequested() const;

	void SetupVirtualMachine();

	void ResetForNewProject();

	// ========================================================================
	// ========================================================================

	void AddCommandHandler(const FString& CommandName, TFunction<void(const TArray<FString>&)> Handler);
	void RemoveCommandHandler(const FString& CommandName);
	void AddBlockingCommandHandler(const FString& CommandName, TFunction<void(const TArray<FString>&)> Handler);
	void CompleteBlockingCommand();

	void AddFunction(const FString& FunctionName, TFunction<FYarnValue(const TArray<FYarnValue>&)> Function, int32 ParameterCount);
	void RemoveFunction(const FString& FunctionName);
	void ReportFunctionError(const FString& Message);

	// ========================================================================
	// ========================================================================

	void SetSaliencyStrategy(TScriptInterface<IYarnSaliencyStrategy> InStrategy);
	TScriptInterface<IYarnSaliencyStrategy> GetActiveSaliencyStrategy() const { return ActiveSaliencyStrategy; }

	// ========================================================================
	// ========================================================================

	bool TryGetSmartVariableAsBool(const FString& Name, bool& OutResult);
	bool TryGetSmartVariableAsFloat(const FString& Name, float& OutResult);
	bool TryGetSmartVariableAsString(const FString& Name, FString& OutResult);
	bool TryGetSmartVariable(const FString& Name, FYarnValue& OutResult);

	// ========================================================================
	// ========================================================================

	UFUNCTION()
	void HandlePresenterLineFinished(EYarnLineCompletionRequest Request);

	UFUNCTION()
	void HandlePresenterOptionSelected(int32 OptionIndex);

private:
	TWeakObjectPtr<UYarnDialogueRunner> Owner;

	UWorld* GetOwnerWorld() const;

	/** The virtual machine that executes yarn bytecode */
	UPROPERTY(Transient)
	FYarnVirtualMachine VirtualMachine;

	/** Registered command handlers (command name -> handler function) */
	TMap<FString, TFunction<void(const TArray<FString>&)>> CommandHandlers;

	/** Command names registered via AddBlockingCommandHandler. */
	TSet<FString> BlockingCommandNames;

	/** True while a blocking command is holding the dialogue. */
	bool bBlockingCommandPending = false;

	/** Registered yarn functions (function name -> implementation) */
	TMap<FString, TFunction<FYarnValue(const TArray<FYarnValue>&)>> Functions;

	/** Parameter counts for registered functions */
	TMap<FString, int32> FunctionParameterCounts;

	int32 ActiveLinePresenterCount = 0;

	/** Current options being displayed (stored for bRunSelectedOptionAsLine) */
	FYarnOptionSet CurrentLocalizedOptions;

	/** Pending option index when running selected option as line (-1 = none) */
	int32 PendingSelectedOptionIndex = -1;

	void HandleLine(const FYarnLine& Line);
	void HandleOptions(const FYarnOptionSet& Options);
	void HandleCommand(const FYarnCommand& Command);

	bool TryDispatchToBlueprintHandler(const FYarnCommand& Command, bool& bOutRequestedBlocking);
	bool TryDispatchToWorldActor(const FYarnCommand& Command, bool& bOutRequestedBlocking);
	void InvokeCommandFunction(UObject* Target, UFunction* Function, const TArray<FString>& Args, bool& bOutRequestedBlocking);

	void ContinueDeferred();

	void WarnIfReservedCommandName(const FString& CommandName) const;

	bool bCustomSaliencyStrategyInstalled = false;

	EYarnSaliencyStrategy CreatedSaliencyStrategyType = EYarnSaliencyStrategy::RandomBestLeastRecentlyViewed;

	void HandleNodeStart(const FString& NodeName);
	void HandleNodeComplete(const FString& NodeName);
	void HandleDialogueComplete();

	FYarnValue CallFunction(const FString& FunctionName, const TArray<FYarnValue>& Parameters);
	FYarnValue CallFunctionForSmartVariable(const FString& FunctionName, const TArray<FYarnValue>& Parameters);
	bool FunctionExists(const FString& FunctionName);
	int32 GetFunctionParameterCount(const FString& FunctionName);
	void RegisterBuiltInFunctions();

	bool TakeFunctionError(FString& OutMessage);

	FString LastFunctionError;

	FYarnLocalizedLine GetLocalizedLine(const FYarnLine& Line);

	/** Timer handle for <<wait>> command */
	FTimerHandle WaitTimerHandle;

	/** Called when wait timer completes */
	void OnWaitComplete();

	// ------------------------------------------------------------------------
	// Two-level cancellation: dialogue → content
	// ------------------------------------------------------------------------
	// One source for the conversation as a whole, and a fresh linked source
	// for each line / options block. Cancelling the dialogue source cascades
	// down to whatever per-content source is current. Cancelling a per-content
	// source ("skip this line") does not bubble up to the dialogue.

	/** Lives for the duration of a single conversation. Cancel to tear the
	 *  whole thing down (StopDialogue). */
	UPROPERTY()
	TObjectPtr<UYarnCancellationTokenSource> DialogueCancellationSource;

	/** Per-line source, linked to DialogueCancellationSource. Replaced with
	 *  a fresh one at the start of each line. */
	UPROPERTY()
	TObjectPtr<UYarnCancellationTokenSource> CancellationTokenSource;

	/** Per-options source, linked to DialogueCancellationSource. Replaced
	 *  with a fresh one at the start of each options block. */
	UPROPERTY()
	TObjectPtr<UYarnCancellationTokenSource> OptionsCancellationTokenSource;

	/** The active saliency strategy instance */
	UPROPERTY()
	TScriptInterface<IYarnSaliencyStrategy> ActiveSaliencyStrategy;

	void CreateSaliencyStrategy();

	/** Candidates for current saliency selection */
	TArray<FYarnSaliencyCandidate> SaliencyCandidates;

	/** Add a saliency candidate */
	void HandleAddSaliencyCandidate(const FYarnSaliencyCandidate& Candidate);

	bool HandleSelectSaliencyCandidate(FYarnSaliencyCandidate& OutSelectedCandidate);

	/** Select best saliency candidate (called from VM via delegate) */
	bool HandleVMSelectSaliencyCandidate(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate);

	/** Notify strategy that content was selected (called from VM after validation) */
	void HandleContentWasSelected(const FYarnSaliencyCandidate& SelectedCandidate);

	/** Handle prepare for lines (pre-loading for upcoming lines) */
	void HandlePrepareForLines(const TArray<FString>& LineIDs);

	/** Clear saliency candidates */
	void ClearSaliencyCandidates();

	friend class UYarnDialogueRunner;
};

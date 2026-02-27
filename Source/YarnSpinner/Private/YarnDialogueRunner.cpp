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

#include "YarnDialogueRunner.h"
#include "YarnDialoguePresenter.h"
#include "YarnLocalization.h"
#include "YarnSmartVariables.h"
#include "YarnSpinnerModule.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include <cmath>

UYarnDialogueRunner::UYarnDialogueRunner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UYarnDialogueRunner::BeginPlay()
{
	Super::BeginPlay();

	// Create a default variable storage if none provided - must be done before SetupVirtualMachine
	if (!VariableStorage.GetInterface())
	{
		UYarnInMemoryVariableStorage* DefaultStorage = NewObject<UYarnInMemoryVariableStorage>(GetOwner());
		if (DefaultStorage)
		{
			DefaultStorage->RegisterComponent();
			VariableStorage.SetInterface(DefaultStorage);
			VariableStorage.SetObject(DefaultStorage);
		}
	}

	// Set the yarn project on variable storage
	if (VariableStorage.GetInterface() && YarnProject)
	{
		VariableStorage->SetYarnProject(YarnProject);
	}

	// Set ourselves as the smart variable evaluator on variable storage
	if (VariableStorage.GetInterface())
	{
		VariableStorage->SetSmartVariableEvaluator(this);
	}

	// Now set up VM (which needs VariableStorage to be set)
	SetupVirtualMachine();
	RegisterBuiltInFunctions();

	// Create cancellation token source for lines
	CancellationTokenSource = NewObject<UYarnCancellationTokenSource>(this);

	// Create cancellation token source for options
	OptionsCancellationTokenSource = NewObject<UYarnCancellationTokenSource>(this);

	// Create saliency strategy
	CreateSaliencyStrategy();

	// Set up presenters
	for (UYarnDialoguePresenter* Presenter : DialoguePresenters)
	{
		if (Presenter)
		{
			Presenter->SetDialogueRunner(this);
		}
	}

	// Set up line provider - use built-in provider by default
	if (!LineProvider)
	{
		LineProvider = NewObject<UYarnBuiltinLineProvider>(this);
	}
	if (LineProvider && YarnProject)
	{
		LineProvider->SetYarnProject(YarnProject);
	}

	// Auto-start if configured
	if (bAutoStart && YarnProject)
	{
		StartDialogueFromStart();
	}
}

void UYarnDialogueRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDialogue();
	Super::EndPlay(EndPlayReason);
}

void UYarnDialogueRunner::SetYarnProject(UYarnProject* NewYarnProject)
{
	if (!NewYarnProject)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("SetYarnProject: null project"));
		return;
	}

	if (IsDialogueRunning())
	{
		StopDialogue();
	}

	YarnProject = NewYarnProject;

	// Variable storage needs to know about the new project for initial values
	if (VariableStorage.GetInterface())
	{
		VariableStorage->SetYarnProject(YarnProject);
	}

	SetupVirtualMachine();

	if (LineProvider)
	{
		LineProvider->SetYarnProject(YarnProject);
	}

	// Reset presentation state
	ActiveLinePresenterCount = 0;
	CurrentLocalizedOptions.Options.Empty();
	PendingSelectedOptionIndex = -1;
	SaliencyCandidates.Empty();

	// Reset cancellation tokens
	if (CancellationTokenSource)
	{
		CancellationTokenSource->GetToken();
	}
	if (OptionsCancellationTokenSource)
	{
		OptionsCancellationTokenSource->GetToken();
	}
}

void UYarnDialogueRunner::SetupVirtualMachine()
{
	if (!YarnProject)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnDialogueRunner: No Yarn Project set"));
		return;
	}

	VirtualMachine.SetProgram(YarnProject->Program);
	VirtualMachine.VariableStorage = VariableStorage;

	VirtualMachine.LineHandler.BindUObject(this, &UYarnDialogueRunner::HandleLine);
	VirtualMachine.OptionsHandler.BindUObject(this, &UYarnDialogueRunner::HandleOptions);
	VirtualMachine.CommandHandler.BindUObject(this, &UYarnDialogueRunner::HandleCommand);
	VirtualMachine.NodeStartHandler.BindUObject(this, &UYarnDialogueRunner::HandleNodeStart);
	VirtualMachine.NodeCompleteHandler.BindUObject(this, &UYarnDialogueRunner::HandleNodeComplete);
	VirtualMachine.DialogueCompleteHandler.BindUObject(this, &UYarnDialogueRunner::HandleDialogueComplete);

	VirtualMachine.CallFunctionHandler.BindUObject(this, &UYarnDialogueRunner::CallFunction);
	VirtualMachine.FunctionExistsHandler.BindUObject(this, &UYarnDialogueRunner::FunctionExists);
	VirtualMachine.FunctionParamCountHandler.BindUObject(this, &UYarnDialogueRunner::GetFunctionParameterCount);
	VirtualMachine.SelectSaliencyCandidateHandler.BindUObject(this, &UYarnDialogueRunner::HandleVMSelectSaliencyCandidate);
	VirtualMachine.ContentWasSelectedHandler.BindUObject(this, &UYarnDialogueRunner::HandleContentWasSelected);
	VirtualMachine.PrepareForLinesHandler.BindUObject(this, &UYarnDialogueRunner::HandlePrepareForLines);
}

void UYarnDialogueRunner::StartDialogue(const FString& NodeName)
{
	if (!YarnProject)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnDialogueRunner: Cannot start dialogue - no Yarn Project set"));
		return;
	}

	// Debug: log available nodes
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Starting dialogue at node '%s'"), *NodeName);
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: YarnProject has %d nodes"), YarnProject->Program.Nodes.Num());
	for (const auto& Pair : YarnProject->Program.Nodes)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("  - Node '%s' has %d instructions"), *Pair.Key, Pair.Value.Instructions.Num());
	}

	if (!YarnProject->HasNode(NodeName))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnDialogueRunner: Cannot start dialogue - node '%s' not found"), *NodeName);
		return;
	}

	const FYarnNode* Node = YarnProject->Program.GetNode(NodeName);
	if (Node)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Node '%s' found with %d instructions"), *NodeName, Node->Instructions.Num());
	}

	bHurryUpRequested = false;
	bNextLineRequested = false;

	// Events fire before presenters are notified
	OnDialogueStart.Broadcast();

	// Notify presenters that dialogue is starting
	// Copy the array to prevent issues if a presenter modifies the list during iteration
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Notifying %d presenters"), DialoguePresenters.Num());
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			Presenter->OnDialogueStarted();
		}
	}

	// Set the node and start
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Setting node and starting VM"));
	if (VirtualMachine.SetNode(NodeName))
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Node set, calling Continue()"));
		VirtualMachine.Continue();
	}
	else
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnDialogueRunner: Failed to set node '%s' on VM"), *NodeName);
	}
}

void UYarnDialogueRunner::StartDialogueFromStart()
{
	StartDialogue(StartNode);
}

void UYarnDialogueRunner::StopDialogue()
{
	// Clear any pending wait timer so it doesn't fire after dialogue stops
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaitTimerHandle);
	}

	// Cancel all cancellation tokens so presenters know to stop
	if (CancellationTokenSource)
	{
		CancellationTokenSource->CancelAll();
	}
	if (OptionsCancellationTokenSource)
	{
		OptionsCancellationTokenSource->CancelAll();
	}

	// Stop() will trigger DialogueCompleteHandler which notifies presenters
	VirtualMachine.Stop();
}

bool UYarnDialogueRunner::IsDialogueRunning() const
{
	return VirtualMachine.IsActive();
}

void UYarnDialogueRunner::Continue()
{
	// Check if we have a pending option selection (from bRunSelectedOptionAsLine)
	if (PendingSelectedOptionIndex >= 0)
	{
		int32 OptionIndex = PendingSelectedOptionIndex;
		PendingSelectedOptionIndex = -1;

		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Continue() completing pending option selection: %d"), OptionIndex);

		CurrentLocalizedOptions.Options.Empty();

		// Complete the pending option selection
		VirtualMachine.SetSelectedOption(OptionIndex);
	}

	VirtualMachine.Continue();
}

void UYarnDialogueRunner::RequestHurryUp()
{
	bHurryUpRequested = true;

	// Mark hurry-up on the cancellation token
	if (CancellationTokenSource)
	{
		CancellationTokenSource->RequestHurryUp();
	}

	// Copy the array to prevent issues if a presenter modifies the list during iteration
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			Presenter->OnHurryUpRequested();
		}
	}
}

void UYarnDialogueRunner::RequestNextLine()
{
	bNextLineRequested = true;

	// Mark next content on the cancellation token
	if (CancellationTokenSource)
	{
		CancellationTokenSource->RequestNextContent();
	}

	// Copy the array to prevent issues if a presenter modifies the list during iteration
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			Presenter->OnNextLineRequested();
		}
	}
}

void UYarnDialogueRunner::RequestHurryUpOption()
{
	if (!OptionsCancellationTokenSource)
	{
		return;
	}

	OptionsCancellationTokenSource->RequestHurryUp();

	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: RequestHurryUpOption called"));
	}

	// Copy the array to prevent issues if a presenter modifies the list during iteration
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			Presenter->OnOptionsHurryUpRequested();
		}
	}
}

void UYarnDialogueRunner::SelectOption(int32 OptionIndex)
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: SelectOption(%d) called, bRunSelectedOptionAsLine=%s, StoredOptions=%d"),
		OptionIndex, bRunSelectedOptionAsLine ? TEXT("true") : TEXT("false"), CurrentLocalizedOptions.Options.Num());

	// Run the selected option's text as a line before continuing, so it gets full presentation
	if (bRunSelectedOptionAsLine && OptionIndex >= 0 && OptionIndex < CurrentLocalizedOptions.Options.Num())
	{
		const FYarnOption& SelectedOption = CurrentLocalizedOptions.Options[OptionIndex];

		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Running selected option as line: LineID='%s' Text='%s'"),
			*SelectedOption.Line.RawLine.LineID, *SelectedOption.Line.Text.ToString());

		// Store the pending option; selection completes when the line presentation finishes
		PendingSelectedOptionIndex = OptionIndex;

		// When presenters complete, they'll call Continue() which will handle the pending option
		TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
		for (UYarnDialoguePresenter* Presenter : PresentersCopy)
		{
			if (Presenter)
			{
				Presenter->Internal_RunLine(SelectedOption.Line, true);
			}
		}

		// Don't call Continue() here - wait for presenters to finish
		return;
	}

	CurrentLocalizedOptions.Options.Empty();

	VirtualMachine.SetSelectedOption(OptionIndex);
	VirtualMachine.Continue();
}

FString UYarnDialogueRunner::GetCurrentNodeName() const
{
	return VirtualMachine.GetCurrentNodeName();
}

void UYarnDialogueRunner::HandleLine(const FYarnLine& Line)
{
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: HandleLine called with LineID='%s'"), *Line.LineID);

	FYarnLocalizedLine LocalizedLine = GetLocalizedLine(Line);
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Localised line: Character='%s' Text='%s'"),
		*LocalizedLine.CharacterName, *LocalizedLine.Text.ToString());

	bHurryUpRequested = false;
	bNextLineRequested = false;

	// Create a fresh cancellation token for this line
	if (CancellationTokenSource)
	{
		CancellationTokenSource->GetToken(); // Reset the token for the new line
	}

	// All presenters must complete before we continue to the next content.
	ActiveLinePresenterCount = 0;

	// Send to all presenters - copy array to prevent issues if a presenter modifies the list during iteration
	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Sending to %d presenters"), DialoguePresenters.Num());
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			ActiveLinePresenterCount++;
		}
	}

	UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: %d active presenters for this line"), ActiveLinePresenterCount);

	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Calling Internal_RunLine on presenter %s"), *Presenter->GetName());
			Presenter->Internal_RunLine(LocalizedLine, true);
		}
	}

	// If no presenters, continue immediately
	if (ActiveLinePresenterCount == 0)
	{
		Continue();
	}
}

void UYarnDialogueRunner::NotifyPresenterLineComplete()
{
	ActiveLinePresenterCount--;

	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Presenter line complete (%d remaining)"), ActiveLinePresenterCount);
	}

	if (ActiveLinePresenterCount <= 0)
	{
		ActiveLinePresenterCount = 0;
		Continue();
	}
}

void UYarnDialogueRunner::HandleOptions(const FYarnOptionSet& Options)
{
	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Showing %d options"), Options.Options.Num());
	}

	// Reset options cancellation token for the new options presentation
	if (OptionsCancellationTokenSource)
	{
		OptionsCancellationTokenSource->GetToken(); // Reset the token
	}

	// Localise all options and store for potential use by bRunSelectedOptionAsLine
	CurrentLocalizedOptions.Options.Empty();
	for (const FYarnOption& Option : Options.Options)
	{
		FYarnOption LocalizedOption = Option;
		LocalizedOption.Line = GetLocalizedLine(Option.Line.RawLine);
		CurrentLocalizedOptions.Options.Add(LocalizedOption);
	}

	// Send to all presenters - copy array to prevent issues if a presenter modifies the list during iteration
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			Presenter->Internal_RunOptions(CurrentLocalizedOptions);
		}
	}
}

void UYarnDialogueRunner::HandleCommand(const FYarnCommand& Command)
{
	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Command '%s'"), *Command.CommandText);
	}

	// Check for registered handler
	if (TFunction<void(const TArray<FString>&)>* Handler = CommandHandlers.Find(Command.CommandName))
	{
		(*Handler)(Command.Parameters);
		// Defer Continue() to next tick to avoid recursion when called from within RunInstruction
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UYarnDialogueRunner> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick([WeakThis]()
			{
				if (WeakThis.IsValid() && WeakThis->IsDialogueRunning())
				{
					WeakThis->VirtualMachine.Continue();
				}
			});
		}
		return;
	}

	// Check for built-in commands
	if (Command.CommandName == TEXT("wait"))
	{
		// Parse the duration parameter (default 1 second if not specified)
		float Duration = 1.0f;
		if (Command.Parameters.Num() > 0)
		{
			Duration = FCString::Atof(*Command.Parameters[0]);
		}

		if (bVerboseLogging)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Wait command for %.2f seconds"), Duration);
		}

		// Use a timer to delay continuation
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(WaitTimerHandle);
			World->GetTimerManager().SetTimer(
				WaitTimerHandle,
				this,
				&UYarnDialogueRunner::OnWaitComplete,
				Duration,
				false
			);
		}
		else
		{
			// No world available, just continue immediately
			VirtualMachine.Continue();
		}
		return;
	}

	if (Command.CommandName == TEXT("stop"))
	{
		if (bVerboseLogging)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Stop command - ending dialogue"));
		}
		StopDialogue();
		return;
	}

	// Try Blueprint command handler objects
	if (TryDispatchToBlueprintHandler(Command))
	{
		// Defer Continue() to next tick to avoid recursion when called from within RunInstruction
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UYarnDialogueRunner> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick([WeakThis]()
			{
				if (WeakThis.IsValid() && WeakThis->IsDialogueRunning())
				{
					WeakThis->VirtualMachine.Continue();
				}
			});
		}
		return;
	}

	// Unhandled command
	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnDialogueRunner: Unhandled command '%s'"), *Command.CommandName);
	}

	OnUnhandledCommand.Broadcast(Command.CommandText);

	// Dialogue pauses on unhandled commands. The game is responsible for calling
	// Continue() if it wants to proceed (e.g., via an OnUnhandledCommand handler).
}

void UYarnDialogueRunner::HandleNodeStart(const FString& NodeName)
{
	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Node start '%s'"), *NodeName);
	}

	// Visit tracking is handled in VM's ReturnFromNode (on node completion), not here on start.

	// Events fire before presenters are notified
	OnNodeStart.Broadcast(NodeName);

	// Copy the array to prevent issues if a presenter modifies the list during iteration
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			Presenter->OnNodeEnter(NodeName);
		}
	}
}

void UYarnDialogueRunner::HandleNodeComplete(const FString& NodeName)
{
	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Node complete '%s'"), *NodeName);
	}

	// Events fire before presenters are notified
	OnNodeComplete.Broadcast(NodeName);

	// Copy the array to prevent issues if a presenter modifies the list during iteration
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			Presenter->OnNodeExit(NodeName);
		}
	}
}

void UYarnDialogueRunner::HandleDialogueComplete()
{
	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Dialogue complete"));
	}

	// For DialogueComplete, presenters are notified first, then the event fires.
	// This is the opposite order from NodeStart/NodeComplete/DialogueStart.
	TArray<UYarnDialoguePresenter*> PresentersCopy = DialoguePresenters;
	for (UYarnDialoguePresenter* Presenter : PresentersCopy)
	{
		if (Presenter)
		{
			Presenter->OnDialogueComplete();
		}
	}

	// Then fire the event
	OnDialogueComplete.Broadcast();
}

bool UYarnDialogueRunner::TryDispatchToBlueprintHandler(const FYarnCommand& Command)
{
	for (UObject* HandlerObject : CommandHandlerObjects)
	{
		if (!HandlerObject)
		{
			continue;
		}

		// Look for a function with the same name as the command
		UFunction* Function = HandlerObject->FindFunction(FName(*Command.CommandName));
		if (!Function)
		{
			continue;
		}

		if (bVerboseLogging)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Found Blueprint handler '%s' on '%s'"),
				*Command.CommandName, *HandlerObject->GetName());
		}

		// Allocate parameter buffer on the stack
		uint8* ParamBuffer = nullptr;
		if (Function->ParmsSize > 0)
		{
			ParamBuffer = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
			FMemory::Memzero(ParamBuffer, Function->ParmsSize);

			// Initialize all parameters with their default values
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (!It->HasAnyPropertyFlags(CPF_ZeroConstructor))
				{
					It->InitializeValue_InContainer(ParamBuffer);
				}
			}

			// Fill in parameters from command arguments
			int32 ParamIndex = 0;
			for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++It, ++ParamIndex)
			{
				if (ParamIndex < Command.Parameters.Num())
				{
					const FString& ArgString = Command.Parameters[ParamIndex];

					// ImportText converts the string to the appropriate property type
					// This handles FString, float, int, bool, FVector, FRotator, etc.
					const TCHAR* Result = It->ImportText_InContainer(*ArgString, ParamBuffer, HandlerObject, PPF_None);

					if (!Result && bVerboseLogging)
					{
						UE_LOG(LogYarnSpinner, Warning, TEXT("YarnDialogueRunner: Failed to convert parameter %d ('%s') for command '%s'"),
							ParamIndex, *ArgString, *Command.CommandName);
					}
				}
			}
		}

		HandlerObject->ProcessEvent(Function, ParamBuffer);

		// Clean up dynamically initialized parameter memory
		if (ParamBuffer)
		{
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->DestroyValue_InContainer(ParamBuffer);
			}
		}

		return true;
	}

	return false;
}

void UYarnDialogueRunner::AddCommandHandler(const FString& CommandName, TFunction<void(const TArray<FString>&)> Handler)
{
	CommandHandlers.Add(CommandName, Handler);
}

void UYarnDialogueRunner::RemoveCommandHandler(const FString& CommandName)
{
	CommandHandlers.Remove(CommandName);
}

void UYarnDialogueRunner::AddFunction(const FString& FunctionName, TFunction<FYarnValue(const TArray<FYarnValue>&)> Function, int32 ParameterCount)
{
	Functions.Add(FunctionName, Function);
	FunctionParameterCounts.Add(FunctionName, ParameterCount);
}

void UYarnDialogueRunner::RemoveFunction(const FString& FunctionName)
{
	Functions.Remove(FunctionName);
	FunctionParameterCounts.Remove(FunctionName);
}

FYarnValue UYarnDialogueRunner::CallFunction(const FString& FunctionName, const TArray<FYarnValue>& Parameters)
{
	if (TFunction<FYarnValue(const TArray<FYarnValue>&)>* Func = Functions.Find(FunctionName))
	{
		return (*Func)(Parameters);
	}

	UE_LOG(LogYarnSpinner, Warning, TEXT("YarnDialogueRunner: Unknown function '%s'"), *FunctionName);
	return FYarnValue();
}

bool UYarnDialogueRunner::FunctionExists(const FString& FunctionName)
{
	return Functions.Contains(FunctionName);
}

int32 UYarnDialogueRunner::GetFunctionParameterCount(const FString& FunctionName)
{
	const int32* Count = FunctionParameterCounts.Find(FunctionName);
	return Count ? *Count : 0;
}

void UYarnDialogueRunner::RegisterBuiltInFunctions()
{
	// ============================================
	// ARITHMETIC OPERATORS (used by compiler for +, -, *, /, %)
	// ============================================

	AddFunction(TEXT("Number.Add"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);
		return FYarnValue(Params[0].ConvertToNumber() + Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.Minus"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);
		return FYarnValue(Params[0].ConvertToNumber() - Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.Multiply"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);
		return FYarnValue(Params[0].ConvertToNumber() * Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.Divide"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);
		// Division by zero produces NaN/inf (no guard)
		return FYarnValue(Params[0].ConvertToNumber() / Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.Modulo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);
		// Uses integer modulo
		int32 Divisor = static_cast<int32>(Params[1].ConvertToNumber());
		if (Divisor == 0) return FYarnValue(0.0f);
		int32 Dividend = static_cast<int32>(Params[0].ConvertToNumber());
		return FYarnValue(static_cast<float>(Dividend % Divisor));
	}, 2);

	AddFunction(TEXT("Number.UnaryMinus"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(0.0f);
		return FYarnValue(-Params[0].ConvertToNumber());
	}, 1);

	// ============================================
	// COMPARISON OPERATORS (used by compiler for <, <=, >, >=, ==, !=)
	// ============================================

	AddFunction(TEXT("Number.EqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToNumber() == Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.NotEqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToNumber() != Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.GreaterThan"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToNumber() > Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.GreaterThanOrEqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToNumber() >= Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.LessThan"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToNumber() < Params[1].ConvertToNumber());
	}, 2);

	AddFunction(TEXT("Number.LessThanOrEqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToNumber() <= Params[1].ConvertToNumber());
	}, 2);

	// ============================================
	// BOOLEAN OPERATORS (used by compiler for &&, ||, !, ==, !=)
	// ============================================

	AddFunction(TEXT("Bool.EqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToBool() == Params[1].ConvertToBool());
	}, 2);

	AddFunction(TEXT("Bool.NotEqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToBool() != Params[1].ConvertToBool());
	}, 2);

	AddFunction(TEXT("Bool.And"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToBool() && Params[1].ConvertToBool());
	}, 2);

	AddFunction(TEXT("Bool.Or"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToBool() || Params[1].ConvertToBool());
	}, 2);

	AddFunction(TEXT("Bool.Not"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(false);
		return FYarnValue(!Params[0].ConvertToBool());
	}, 1);

	AddFunction(TEXT("Bool.Xor"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToBool() != Params[1].ConvertToBool());
	}, 2);

	// ============================================
	// ENUM OPERATORS
	// ============================================

	AddFunction(TEXT("Enum.EqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		// At VM level, enum values are strings, so we compare as strings.
		return FYarnValue(Params[0].ConvertToString() == Params[1].ConvertToString());
	}, 2);

	AddFunction(TEXT("Enum.NotEqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToString() != Params[1].ConvertToString());
	}, 2);

	// ============================================
	// STRING OPERATORS
	// ============================================

	AddFunction(TEXT("String.Add"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(FString());
		return FYarnValue(Params[0].ConvertToString() + Params[1].ConvertToString());
	}, 2);

	AddFunction(TEXT("String.EqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToString() == Params[1].ConvertToString());
	}, 2);

	AddFunction(TEXT("String.NotEqualTo"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToString() != Params[1].ConvertToString());
	}, 2);

	// ============================================
	// YARN SPINNER BUILT-IN FUNCTIONS
	// ============================================

	// visited(node_name) - returns true if the node has been visited
	AddFunction(TEXT("visited"), [this](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(false);

		FString NodeName = Params[0].ConvertToString();
		FString VariableName = FString::Printf(TEXT("$Yarn.Internal.Visiting.%s"), *NodeName);

		FYarnValue Value;
		if (VariableStorage.GetInterface())
		{
			if (IYarnVariableStorage::Execute_TryGetValue(VariableStorage.GetObject(), VariableName, Value))
			{
				return FYarnValue(Value.ConvertToNumber() > 0);
			}
		}
		return FYarnValue(false);
	}, 1);

	// visited_count(node_name) - returns the number of times a node has been visited
	AddFunction(TEXT("visited_count"), [this](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(0.0f);

		FString NodeName = Params[0].ConvertToString();
		FString VariableName = FString::Printf(TEXT("$Yarn.Internal.Visiting.%s"), *NodeName);

		FYarnValue Value;
		if (VariableStorage.GetInterface())
		{
			if (IYarnVariableStorage::Execute_TryGetValue(VariableStorage.GetObject(), VariableName, Value))
			{
				return Value;
			}
		}
		return FYarnValue(0.0f);
	}, 1);

	// random() - returns a random number between 0 and 1
	AddFunction(TEXT("random"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		return FYarnValue(FMath::FRand());
	}, 0);

	// random_range(min, max) - returns a random integer-stepped value between min and max
	// Truncates min/max toward zero, then picks a random integer step and adds float min.
	// E.g., random_range(1.5, 5.7) produces [1.5, 5.5] in integer steps
	AddFunction(TEXT("random_range"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);

		float Min = Params[0].ConvertToNumber();
		float Max = Params[1].ConvertToNumber();
		// Truncation toward zero (not floor)
		int32 MinInt = static_cast<int32>(Min);
		int32 MaxInt = static_cast<int32>(Max);
		int32 Range = MaxInt - MinInt + 1;
		if (Range <= 0) Range = 1;
		// Returns [0, Range-1]
		int32 RandomValue = FMath::RandRange(0, Range - 1);
		return FYarnValue(static_cast<float>(RandomValue) + Min);
	}, 2);

	// dice(sides) - returns a random integer from 1 to sides
	AddFunction(TEXT("dice"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(1.0f);

		int32 Sides = FMath::RoundToInt(Params[0].ConvertToNumber());
		return FYarnValue(static_cast<float>(FMath::RandRange(1, Sides)));
	}, 1);

	// round(number) - rounds to nearest integer using banker's rounding (round-to-even)
	// E.g., round(2.5) = 2, round(3.5) = 4
	AddFunction(TEXT("round"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(0.0f);
		// std::nearbyint uses the current rounding mode (default FE_TONEAREST = round-to-even)
		return FYarnValue(std::nearbyintf(Params[0].ConvertToNumber()));
	}, 1);

	// round_places(number, places) - rounds to specified decimal places using banker's rounding
	AddFunction(TEXT("round_places"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);

		float Value = Params[0].ConvertToNumber();
		int32 Places = FMath::RoundToInt(Params[1].ConvertToNumber());
		float Multiplier = FMath::Pow(10.0f, static_cast<float>(Places));
		return FYarnValue(std::nearbyintf(Value * Multiplier) / Multiplier);
	}, 2);

	// floor(number) - rounds down
	AddFunction(TEXT("floor"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(0.0f);
		return FYarnValue(FMath::FloorToFloat(Params[0].ConvertToNumber()));
	}, 1);

	// ceil(number) - rounds up
	AddFunction(TEXT("ceil"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(0.0f);
		return FYarnValue(FMath::CeilToFloat(Params[0].ConvertToNumber()));
	}, 1);

	// inc(number) - increments by 1 (returns int)
	// If the value is already an integer, adds 1. Otherwise, rounds up to the next integer.
	AddFunction(TEXT("inc"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(1.0f);
		float Value = Params[0].ConvertToNumber();
		if (static_cast<float>(static_cast<int32>(Value)) == Value)
		{
			return FYarnValue(static_cast<float>(static_cast<int32>(Value) + 1));
		}
		else
		{
			return FYarnValue(static_cast<float>(FMath::CeilToInt(Value)));
		}
	}, 1);

	// dec(number) - decrements by 1 (returns int)
	// If the value is already an integer, subtracts 1. Otherwise, rounds down to the previous integer.
	AddFunction(TEXT("dec"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(-1.0f);
		float Value = Params[0].ConvertToNumber();
		if (static_cast<float>(static_cast<int32>(Value)) == Value)
		{
			return FYarnValue(static_cast<float>(static_cast<int32>(Value) - 1));
		}
		else
		{
			return FYarnValue(static_cast<float>(FMath::FloorToInt(Value)));
		}
	}, 1);

	// decimal(number) - returns the decimal portion
	AddFunction(TEXT("decimal"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(0.0f);
		float Value = Params[0].ConvertToNumber();
		return FYarnValue(Value - FMath::TruncToFloat(Value));
	}, 1);

	// int(number) - returns the integer portion (truncates toward zero)
	AddFunction(TEXT("int"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(0.0f);
		return FYarnValue(FMath::TruncToFloat(Params[0].ConvertToNumber()));
	}, 1);

	// ============================================
	// YARN SPINNER 3.1 ADDITIONAL FUNCTIONS
	// ============================================

	// min(a, b) - returns the minimum of two numbers
	AddFunction(TEXT("min"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);
		return FYarnValue(FMath::Min(Params[0].ConvertToNumber(), Params[1].ConvertToNumber()));
	}, 2);

	// max(a, b) - returns the maximum of two numbers
	AddFunction(TEXT("max"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);
		return FYarnValue(FMath::Max(Params[0].ConvertToNumber(), Params[1].ConvertToNumber()));
	}, 2);

	// random_range_float(min, max) - identical to random_range
	AddFunction(TEXT("random_range_float"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(0.0f);

		float Min = Params[0].ConvertToNumber();
		float Max = Params[1].ConvertToNumber();
		int32 MinInt = static_cast<int32>(Min);
		int32 MaxInt = static_cast<int32>(Max);
		int32 Range = MaxInt - MinInt + 1;
		if (Range <= 0) Range = 1;
		int32 RandomValue = FMath::RandRange(0, Range - 1);
		return FYarnValue(static_cast<float>(RandomValue) + Min);
	}, 2);

	// ============================================
	// TYPE CONVERSION FUNCTIONS
	// ============================================

	// string(v) - converts any value to string
	AddFunction(TEXT("string"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(FString());
		return FYarnValue(Params[0].ConvertToString());
	}, 1);

	// number(v) - converts any value to number
	AddFunction(TEXT("number"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(0.0f);
		return FYarnValue(Params[0].ConvertToNumber());
	}, 1);

	// bool(v) - converts any value to bool
	AddFunction(TEXT("bool"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(false);
		return FYarnValue(Params[0].ConvertToBool());
	}, 1);

	// ============================================
	// FORMAT FUNCTIONS
	// ============================================

	// format_invariant(v) - formats a number using invariant culture
	// Critical for embedding numbers in commands that need consistent formatting
	AddFunction(TEXT("format_invariant"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(FString());
		float Value = Params[0].ConvertToNumber();
		// Uses "G" format with 7 significant digits.
		// Outputs integers without a decimal point; uses scientific notation for very large/small values.
		return FYarnValue(FString::Printf(TEXT("%.7G"), Value));
	}, 1);

	// format(formatString, argument) - formats a value using the format string
	// Supports .NET-style format specifiers: {0}, {0:F2}, {0:N0}, {0:D4}, etc.
	AddFunction(TEXT("format"), [](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 2) return FYarnValue(FString());
		FString FormatString = Params[0].ConvertToString();

		// Check if there's a format specifier: {0:spec}
		int32 ColonPos = INDEX_NONE;
		int32 BraceStart = FormatString.Find(TEXT("{0"));
		if (BraceStart != INDEX_NONE)
		{
			int32 BraceEnd = FormatString.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, BraceStart);
			if (BraceEnd != INDEX_NONE)
			{
				FString PlaceholderContent = FormatString.Mid(BraceStart + 1, BraceEnd - BraceStart - 1);
				int32 LocalColon;
				if (PlaceholderContent.FindChar(TEXT(':'), LocalColon))
				{
					ColonPos = BraceStart + 1 + LocalColon;
				}
			}
		}

		if (ColonPos != INDEX_NONE)
		{
			// Has format specifier - extract it
			int32 BraceEnd = FormatString.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ColonPos);
			if (BraceEnd != INDEX_NONE)
			{
				FString Specifier = FormatString.Mid(ColonPos + 1, BraceEnd - ColonPos - 1);
				FString Placeholder = FormatString.Mid(BraceStart, BraceEnd - BraceStart + 1);
				FString Formatted;

				float NumValue = Params[1].ConvertToNumber();
				TCHAR SpecChar = Specifier.Len() > 0 ? FChar::ToUpper(Specifier[0]) : TEXT('G');
				int32 Precision = Specifier.Len() > 1 ? FCString::Atoi(*Specifier.Mid(1)) : -1;

				switch (SpecChar)
				{
				case TEXT('F'): // Fixed-point
					Formatted = FString::Printf(TEXT("%.*f"), Precision >= 0 ? Precision : 2, NumValue);
					break;
				case TEXT('N'): // Number with thousand separators
				{
					int32 DecPlaces = Precision >= 0 ? Precision : 2;
					FString NumStr = FString::Printf(TEXT("%.*f"), DecPlaces, FMath::Abs(NumValue));
					// Add thousand separators to integer part
					int32 DotPos;
					FString IntPart, FracPart;
					if (NumStr.FindChar(TEXT('.'), DotPos))
					{
						IntPart = NumStr.Left(DotPos);
						FracPart = NumStr.Mid(DotPos);
					}
					else
					{
						IntPart = NumStr;
					}
					FString WithSeparators;
					int32 Count = 0;
					for (int32 j = IntPart.Len() - 1; j >= 0; j--)
					{
						if (Count > 0 && Count % 3 == 0) WithSeparators = TEXT(",") + WithSeparators;
						WithSeparators = FString(1, &IntPart[j]) + WithSeparators;
						Count++;
					}
					Formatted = (NumValue < 0 ? TEXT("-") : TEXT("")) + WithSeparators + FracPart;
					break;
				}
				case TEXT('D'): // Decimal integer
				{
					int32 Width = Precision >= 0 ? Precision : 1;
					Formatted = FString::Printf(TEXT("%0*d"), Width, FMath::RoundToInt(NumValue));
					break;
				}
				case TEXT('P'): // Percent
				{
					int32 DecPlaces = Precision >= 0 ? Precision : 2;
					Formatted = FString::Printf(TEXT("%.*f %%"), DecPlaces, NumValue * 100.0f);
					break;
				}
				case TEXT('E'): // Scientific
					Formatted = FString::Printf(TEXT("%.*e"), Precision >= 0 ? Precision : 6, NumValue);
					break;
				case TEXT('G'): // General format - shortest representation
					Formatted = FString::Printf(TEXT("%.*G"), Precision >= 0 ? Precision : 7, NumValue);
					break;
				case TEXT('X'): // Hexadecimal
				{
					int32 IntVal = FMath::RoundToInt(NumValue);
					int32 Width = Precision >= 0 ? Precision : 0;
					if (Specifier.Len() > 0 && FChar::IsLower(Specifier[0]))
					{
						// lowercase x
						Formatted = FString::Printf(TEXT("%0*x"), Width, IntVal);
					}
					else
					{
						// uppercase X
						Formatted = FString::Printf(TEXT("%0*X"), Width, IntVal);
					}
					break;
				}
				case TEXT('C'): // Currency
				{
					int32 DecPlaces = Precision >= 0 ? Precision : 2;
					FString NumStr = FString::Printf(TEXT("%.*f"), DecPlaces, FMath::Abs(NumValue));
					int32 DotPos;
					FString IntPart, FracPart;
					if (NumStr.FindChar(TEXT('.'), DotPos))
					{
						IntPart = NumStr.Left(DotPos);
						FracPart = NumStr.Mid(DotPos);
					}
					else
					{
						IntPart = NumStr;
					}
					FString WithSeparators;
					int32 SepCount = 0;
					for (int32 j = IntPart.Len() - 1; j >= 0; j--)
					{
						if (SepCount > 0 && SepCount % 3 == 0) WithSeparators = TEXT(",") + WithSeparators;
						WithSeparators = FString(1, &IntPart[j]) + WithSeparators;
						SepCount++;
					}
					Formatted = (NumValue < 0 ? TEXT("($") : TEXT("$")) + WithSeparators + FracPart + (NumValue < 0 ? TEXT(")") : TEXT(""));
					break;
				}
				case TEXT('R'): // Round-trip - preserve full float precision
					Formatted = FString::Printf(TEXT("%.9G"), NumValue);
					break;
				default: // Unknown specifier - use general format
					Formatted = FString::Printf(TEXT("%.7G"), NumValue);
					break;
				}

				FString Result = FormatString.Replace(*Placeholder, *Formatted);
				return FYarnValue(Result);
			}
		}

		// No format specifier - simple {0} replacement
		FString ArgString = Params[1].ConvertToString();
		FString Result = FormatString.Replace(TEXT("{0}"), *ArgString);
		return FYarnValue(Result);
	}, 2);

	// ============================================
	// SALIENCY FUNCTIONS
	// ============================================

	// has_any_content(nodeGroup) - returns true if the node group has any available content
	AddFunction(TEXT("has_any_content"), [this](const TArray<FYarnValue>& Params) -> FYarnValue {
		if (Params.Num() < 1) return FYarnValue(false);

		FString NodeGroupName = Params[0].ConvertToString();

		if (!YarnProject)
		{
			return FYarnValue(false);
		}

		const FYarnNode* Node = YarnProject->Program.Nodes.Find(NodeGroupName);
		if (!Node)
		{
			return FYarnValue(false);
		}

		// Check if this node is a node group hub
		if (!Node->HasHeader(TEXT("$Yarn.Internal.NodeGroupHub")))
		{
			return FYarnValue(true);
		}

		// Use GetSaliencyOptionsForNodeGroup to properly evaluate smart variable conditions
		FOnSmartVariableCallFunction CallFunctionHandler;
		CallFunctionHandler.BindLambda([this](const FString& FunctionName, const TArray<FYarnValue>& Parameters) -> FYarnValue {
			return CallFunction(FunctionName, Parameters);
		});

		TArray<FYarnSaliencyCandidate> Candidates;
		FYarnSmartVariableEvaluationVM::GetSaliencyOptionsForNodeGroup(
			NodeGroupName,
			YarnProject->Program,
			VariableStorage,
			CallFunctionHandler,
			Candidates);

		if (Candidates.Num() == 0)
		{
			return FYarnValue(false);
		}

		// Check if any candidate can be selected by the saliency strategy
		if (!ActiveSaliencyStrategy.GetInterface())
		{
			// No strategy means we can't determine - assume content exists
			return FYarnValue(true);
		}

		// Check if any candidate can be selected
		FYarnSaliencyCandidate SelectedCandidate;
		bool bHasContent = IYarnSaliencyStrategy::Execute_QueryBestContent(ActiveSaliencyStrategy.GetObject(), Candidates, SelectedCandidate);
		return FYarnValue(bHasContent);
	}, 1);
}

FYarnLocalizedLine UYarnDialogueRunner::GetLocalizedLine(const FYarnLine& Line)
{
	if (LineProvider)
	{
		return LineProvider->GetLocalizedLine(Line);
	}

	// Fallback: return the line ID as text
	FYarnLocalizedLine LocalizedLine;
	LocalizedLine.RawLine = Line;
	LocalizedLine.Text = FText::FromString(Line.LineID);
	return LocalizedLine;
}

void UYarnDialogueRunner::OnWaitComplete()
{
	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Wait complete, continuing dialogue"));
	}

	if (VirtualMachine.IsActive())
	{
		VirtualMachine.Continue();
	}
}

FYarnLineCancellationToken& UYarnDialogueRunner::GetCurrentCancellationToken()
{
	if (!CancellationTokenSource)
	{
		CancellationTokenSource = NewObject<UYarnCancellationTokenSource>(this);
	}
	return CancellationTokenSource->GetToken();
}

FYarnLineCancellationToken& UYarnDialogueRunner::GetCurrentOptionsCancellationToken()
{
	if (!OptionsCancellationTokenSource)
	{
		OptionsCancellationTokenSource = NewObject<UYarnCancellationTokenSource>(this);
	}
	return OptionsCancellationTokenSource->GetToken();
}

bool UYarnDialogueRunner::IsHurryUpRequested() const
{
	return CancellationTokenSource ? CancellationTokenSource->IsHurryUpRequested() : false;
}

bool UYarnDialogueRunner::IsNextContentRequested() const
{
	return CancellationTokenSource ? CancellationTokenSource->IsNextContentRequested() : false;
}

bool UYarnDialogueRunner::IsOptionHurryUpRequested() const
{
	return OptionsCancellationTokenSource ? OptionsCancellationTokenSource->IsHurryUpRequested() : false;
}

bool UYarnDialogueRunner::IsOptionNextContentRequested() const
{
	return OptionsCancellationTokenSource ? OptionsCancellationTokenSource->IsNextContentRequested() : false;
}

void UYarnDialogueRunner::CreateSaliencyStrategy()
{
	ActiveSaliencyStrategy = UYarnSaliencyStrategyFactory::CreateStrategy(
		SaliencyStrategy,
		VariableStorage,
		this
	);
}

void UYarnDialogueRunner::HandleAddSaliencyCandidate(const FYarnSaliencyCandidate& Candidate)
{
	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Adding saliency candidate '%s' (complexity: %d, failing: %d)"),
			*Candidate.ContentID, Candidate.ComplexityScore, Candidate.FailingConditionCount);
	}
	SaliencyCandidates.Add(Candidate);
}

bool UYarnDialogueRunner::HandleSelectSaliencyCandidate(FYarnSaliencyCandidate& OutSelectedCandidate)
{
	if (!ActiveSaliencyStrategy.GetInterface())
	{
		CreateSaliencyStrategy();
	}

	if (!ActiveSaliencyStrategy.GetInterface())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnDialogueRunner: No saliency strategy available"));
		return false;
	}

	bool bSelected = IYarnSaliencyStrategy::Execute_QueryBestContent(ActiveSaliencyStrategy.GetObject(), SaliencyCandidates, OutSelectedCandidate);

	if (bSelected)
	{
		IYarnSaliencyStrategy::Execute_ContentWasSelected(ActiveSaliencyStrategy.GetObject(), OutSelectedCandidate);

		if (bVerboseLogging)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Selected saliency candidate '%s'"),
				*OutSelectedCandidate.ContentID);
		}
	}
	else
	{
		if (bVerboseLogging)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: No viable saliency candidate found"));
		}
	}

	// Clear candidates for next selection
	ClearSaliencyCandidates();

	return bSelected;
}

bool UYarnDialogueRunner::HandleVMSelectSaliencyCandidate(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate)
{
	if (!ActiveSaliencyStrategy.GetInterface())
	{
		CreateSaliencyStrategy();
	}

	if (!ActiveSaliencyStrategy.GetInterface())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YarnDialogueRunner: No saliency strategy available for VM selection"));
		return false;
	}

	// This is a read-only query. ContentWasSelected will be called
	// separately by the VM via ContentWasSelectedHandler after validation.
	bool bSelected = IYarnSaliencyStrategy::Execute_QueryBestContent(ActiveSaliencyStrategy.GetObject(), Candidates, OutSelectedCandidate);

	if (bSelected)
	{
		if (bVerboseLogging)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: VM saliency strategy selected '%s'"),
				*OutSelectedCandidate.ContentID);
		}
	}
	else
	{
		if (bVerboseLogging)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: VM saliency strategy found no viable candidate"));
		}
	}

	return bSelected;
}

void UYarnDialogueRunner::HandleContentWasSelected(const FYarnSaliencyCandidate& SelectedCandidate)
{
	// Called by the VM after it validates the selection.
	// Notifies the strategy that content was selected (for LeastRecentlyViewed tracking).
	if (ActiveSaliencyStrategy.GetInterface())
	{
		IYarnSaliencyStrategy::Execute_ContentWasSelected(ActiveSaliencyStrategy.GetObject(), SelectedCandidate);

		if (bVerboseLogging)
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Notified strategy that content '%s' was selected"),
				*SelectedCandidate.ContentID);
		}
	}
}

void UYarnDialogueRunner::ClearSaliencyCandidates()
{
	SaliencyCandidates.Empty();
}

void UYarnDialogueRunner::HandlePrepareForLines(const TArray<FString>& LineIDs)
{
	// This handler is called when a new node is started, providing the line IDs
	// that will be needed. This allows pre-loading of localisation and audio assets.
	// Notify presenters that they may want to prepare these lines.

	if (LineIDs.Num() == 0)
	{
		return;
	}

	if (bVerboseLogging)
	{
		UE_LOG(LogYarnSpinner, Log, TEXT("YarnDialogueRunner: Preparing for %d lines in upcoming node"), LineIDs.Num());
	}

	// Notify presenters (if they support it, they can pre-load assets)
	// For now, we just log the IDs. Presenters can override a PrepareForLines method
	// if they need to do actual pre-loading.
	for (UYarnDialoguePresenter* Presenter : DialoguePresenters)
	{
		if (Presenter)
		{
			Presenter->OnPrepareForLines(LineIDs);
		}
	}
}

// ============================================================================
// IYarnSmartVariableEvaluator implementation
// ============================================================================
// Smart variables are compiled expression nodes that compute values on-the-fly.

bool UYarnDialogueRunner::TryGetSmartVariableAsBool(const FString& Name, bool& OutResult)
{
	if (!YarnProject)
	{
		OutResult = false;
		return false;
	}

	// Create a function call handler that uses our registered functions
	FOnSmartVariableCallFunction CallFunctionHandler;
	CallFunctionHandler.BindLambda([this](const FString& FunctionName, const TArray<FYarnValue>& Parameters) -> FYarnValue {
		return CallFunction(FunctionName, Parameters);
	});

	return FYarnSmartVariableEvaluationVM::TryGetSmartVariableAsBool(
		Name,
		YarnProject->Program,
		VariableStorage,
		CallFunctionHandler,
		OutResult);
}

bool UYarnDialogueRunner::TryGetSmartVariableAsFloat(const FString& Name, float& OutResult)
{
	if (!YarnProject)
	{
		OutResult = 0.0f;
		return false;
	}

	// Create a function call handler that uses our registered functions
	FOnSmartVariableCallFunction CallFunctionHandler;
	CallFunctionHandler.BindLambda([this](const FString& FunctionName, const TArray<FYarnValue>& Parameters) -> FYarnValue {
		return CallFunction(FunctionName, Parameters);
	});

	return FYarnSmartVariableEvaluationVM::TryGetSmartVariableAsFloat(
		Name,
		YarnProject->Program,
		VariableStorage,
		CallFunctionHandler,
		OutResult);
}

bool UYarnDialogueRunner::TryGetSmartVariableAsString(const FString& Name, FString& OutResult)
{
	if (!YarnProject)
	{
		OutResult = FString();
		return false;
	}

	// Create a function call handler that uses our registered functions
	FOnSmartVariableCallFunction CallFunctionHandler;
	CallFunctionHandler.BindLambda([this](const FString& FunctionName, const TArray<FYarnValue>& Parameters) -> FYarnValue {
		return CallFunction(FunctionName, Parameters);
	});

	return FYarnSmartVariableEvaluationVM::TryGetSmartVariableAsString(
		Name,
		YarnProject->Program,
		VariableStorage,
		CallFunctionHandler,
		OutResult);
}

bool UYarnDialogueRunner::TryGetSmartVariable(const FString& Name, FYarnValue& OutResult)
{
	if (!YarnProject)
	{
		OutResult = FYarnValue();
		return false;
	}

	// Create a function call handler that uses our registered functions
	FOnSmartVariableCallFunction CallFunctionHandler;
	CallFunctionHandler.BindLambda([this](const FString& FunctionName, const TArray<FYarnValue>& Parameters) -> FYarnValue {
		return CallFunction(FunctionName, Parameters);
	});

	return FYarnSmartVariableEvaluationVM::TryGetSmartVariable(
		Name,
		YarnProject->Program,
		VariableStorage,
		CallFunctionHandler,
		OutResult);
}

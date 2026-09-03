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
#include "YarnSpinnerModule.h"
#include "GameFramework/Actor.h"

UYarnDialogueRunner::UYarnDialogueRunner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UYarnDialogueInstance* UYarnDialogueRunner::GetOrCreateInstance()
{
	if (!Instance)
	{
		Instance = NewObject<UYarnDialogueInstance>(this);
		Instance->Initialize(this);
	}
	return Instance;
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
		VariableStorage->SetSmartVariableEvaluator(this, this);
	}

	GetOrCreateInstance();
	Instance->SetupVirtualMachine();
	Instance->RegisterBuiltInFunctions();
	Instance->ResetForNewProject();

	// Create saliency strategy
	Instance->CreateSaliencyStrategy();

	// Resolve editor-assigned component references into the runtime array
	for (const FComponentReference& PresenterRef : DialoguePresenterReferences)
	{
		UYarnDialoguePresenter* Referenced = Cast<UYarnDialoguePresenter>(PresenterRef.GetComponent(GetOwner()));
		if (Referenced && !DialoguePresenters.Contains(Referenced))
		{
			DialoguePresenters.Add(Referenced);
		}
	}

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
	Instance = nullptr;
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

	if (Instance)
	{
		Instance->SetupVirtualMachine();
	}

	if (LineProvider)
	{
		LineProvider->SetYarnProject(YarnProject);
	}

	// Reset presentation state
	if (Instance)
	{
		Instance->ResetForNewProject();
	}
}

void UYarnDialogueRunner::StartDialogue(const FString& NodeName)
{
	if (Instance)
	{
		Instance->StartDialogue(NodeName);
	}
}

void UYarnDialogueRunner::StartDialogueFromStart()
{
	StartDialogue(StartNode);
}

void UYarnDialogueRunner::StopDialogue()
{
	if (Instance)
	{
		Instance->StopDialogue();
	}
}

bool UYarnDialogueRunner::IsDialogueRunning() const
{
	return Instance ? Instance->IsDialogueRunning() : false;
}

void UYarnDialogueRunner::Continue()
{
	if (Instance)
	{
		Instance->Continue();
	}
}

void UYarnDialogueRunner::RequestHurryUp()
{
	if (Instance)
	{
		Instance->RequestHurryUp();
	}
}

void UYarnDialogueRunner::RequestNextLine()
{
	if (Instance)
	{
		Instance->RequestNextLine();
	}
}

void UYarnDialogueRunner::RequestHurryUpOption()
{
	if (Instance)
	{
		Instance->RequestHurryUpOption();
	}
}

void UYarnDialogueRunner::SelectOption(int32 OptionIndex)
{
	if (Instance)
	{
		Instance->SelectOption(OptionIndex);
	}
}

FString UYarnDialogueRunner::GetCurrentNodeName() const
{
	return Instance ? Instance->GetCurrentNodeName() : FString();
}

void UYarnDialogueRunner::NotifyPresenterLineComplete()
{
	// Back-compat shim. The new flow goes through the FOnYarnLineFinished
	// callback, which lands in HandlePresenterLineFinished. Anything calling
	// us directly (legacy presenters reaching through the runner pointer)
	// is treated as a no-request "I'm done" completion.
	HandlePresenterLineFinished(EYarnLineCompletionRequest::None);
}

void UYarnDialogueRunner::HandlePresenterLineFinished(EYarnLineCompletionRequest Request)
{
	if (Instance)
	{
		Instance->HandlePresenterLineFinished(Request);
	}
}

void UYarnDialogueRunner::HandlePresenterOptionSelected(int32 OptionIndex)
{
	if (Instance)
	{
		Instance->HandlePresenterOptionSelected(OptionIndex);
	}
}

void UYarnDialogueRunner::AddCommandHandler(const FString& CommandName, TFunction<void(const TArray<FString>&)> Handler)
{
	GetOrCreateInstance()->AddCommandHandler(CommandName, Handler);
}

void UYarnDialogueRunner::RemoveCommandHandler(const FString& CommandName)
{
	if (Instance)
	{
		Instance->RemoveCommandHandler(CommandName);
	}
}

void UYarnDialogueRunner::AddBlockingCommandHandler(const FString& CommandName, TFunction<void(const TArray<FString>&)> Handler)
{
	GetOrCreateInstance()->AddBlockingCommandHandler(CommandName, Handler);
}

void UYarnDialogueRunner::CompleteBlockingCommand()
{
	if (Instance)
	{
		Instance->CompleteBlockingCommand();
	}
}

void UYarnDialogueRunner::AddFunction(const FString& FunctionName, TFunction<FYarnValue(const TArray<FYarnValue>&)> Function, int32 ParameterCount)
{
	GetOrCreateInstance()->AddFunction(FunctionName, Function, ParameterCount);
}

void UYarnDialogueRunner::RemoveFunction(const FString& FunctionName)
{
	if (Instance)
	{
		Instance->RemoveFunction(FunctionName);
	}
}

void UYarnDialogueRunner::ReportFunctionError(const FString& Message)
{
	if (Instance)
	{
		Instance->ReportFunctionError(Message);
	}
}

void UYarnDialogueRunner::SetSaliencyStrategy(TScriptInterface<IYarnSaliencyStrategy> InStrategy)
{
	if (Instance)
	{
		Instance->SetSaliencyStrategy(InStrategy);
	}
}

FYarnLineCancellationToken UYarnDialogueRunner::GetCurrentCancellationToken()
{
	return Instance ? Instance->GetCurrentCancellationToken() : FYarnLineCancellationToken();
}

FYarnLineCancellationToken UYarnDialogueRunner::GetCurrentOptionsCancellationToken()
{
	return Instance ? Instance->GetCurrentOptionsCancellationToken() : FYarnLineCancellationToken();
}

bool UYarnDialogueRunner::IsHurryUpRequested() const
{
	return Instance ? Instance->IsHurryUpRequested() : false;
}

bool UYarnDialogueRunner::IsNextContentRequested() const
{
	return Instance ? Instance->IsNextContentRequested() : false;
}

bool UYarnDialogueRunner::IsOptionHurryUpRequested() const
{
	return Instance ? Instance->IsOptionHurryUpRequested() : false;
}

bool UYarnDialogueRunner::IsOptionNextContentRequested() const
{
	return Instance ? Instance->IsOptionNextContentRequested() : false;
}

// ============================================================================
// IYarnSmartVariableEvaluator implementation
// ============================================================================
// Smart variables are compiled expression nodes that compute values on-the-fly.

bool UYarnDialogueRunner::TryGetSmartVariableAsBool(const FString& Name, bool& OutResult)
{
	return Instance ? Instance->TryGetSmartVariableAsBool(Name, OutResult) : false;
}

bool UYarnDialogueRunner::TryGetSmartVariableAsFloat(const FString& Name, float& OutResult)
{
	return Instance ? Instance->TryGetSmartVariableAsFloat(Name, OutResult) : false;
}

bool UYarnDialogueRunner::TryGetSmartVariableAsString(const FString& Name, FString& OutResult)
{
	return Instance ? Instance->TryGetSmartVariableAsString(Name, OutResult) : false;
}

bool UYarnDialogueRunner::TryGetSmartVariable(const FString& Name, FYarnValue& OutResult)
{
	return Instance ? Instance->TryGetSmartVariable(Name, OutResult) : false;
}

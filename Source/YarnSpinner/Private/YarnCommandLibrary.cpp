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

#include "YarnCommandLibrary.h"
#include "YarnDialogueRunner.h"
#include "YarnSpinnerModule.h"

void UYarnCommandLibrary::RegisterCommandHandler(UYarnDialogueRunner* DialogueRunner, const FString& CommandName, FYarnCommandHandlerBP Handler)
{
	if (!DialogueRunner)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("RegisterCommandHandler: DialogueRunner is null"));
		return;
	}

	if (CommandName.IsEmpty())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("RegisterCommandHandler: CommandName is empty"));
		return;
	}

	// wrap the blueprint delegate in a TFunction
	DialogueRunner->AddCommandHandler(CommandName, [Handler](const TArray<FString>& Parameters)
	{
		if (Handler.IsBound())
		{
			Handler.Execute(Parameters);
		}
	});
}

void UYarnCommandLibrary::UnregisterCommandHandler(UYarnDialogueRunner* DialogueRunner, const FString& CommandName)
{
	if (!DialogueRunner)
	{
		return;
	}

	DialogueRunner->RemoveCommandHandler(CommandName);
}

void UYarnCommandLibrary::RegisterFunctionHandler(UYarnDialogueRunner* DialogueRunner, const FString& FunctionName, FYarnFunctionHandlerBP Handler, int32 ParameterCount)
{
	if (!DialogueRunner)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("RegisterFunctionHandler: DialogueRunner is null"));
		return;
	}

	if (FunctionName.IsEmpty())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("RegisterFunctionHandler: FunctionName is empty"));
		return;
	}

	// wrap the blueprint delegate in a TFunction
	DialogueRunner->AddFunction(FunctionName, [Handler](const TArray<FYarnValue>& Parameters) -> FYarnValue
	{
		if (Handler.IsBound())
		{
			return Handler.Execute(Parameters);
		}
		return FYarnValue();
	}, ParameterCount);
}

void UYarnCommandLibrary::UnregisterFunctionHandler(UYarnDialogueRunner* DialogueRunner, const FString& FunctionName)
{
	if (!DialogueRunner)
	{
		return;
	}

	DialogueRunner->RemoveFunction(FunctionName);
}

void UYarnCommandLibrary::RegisterCommandsFromObject(UYarnDialogueRunner* DialogueRunner, UObject* HandlerObject)
{
#if WITH_EDITORONLY_DATA
	if (!DialogueRunner || !HandlerObject)
	{
		return;
	}

	UClass* ObjectClass = HandlerObject->GetClass();
	if (!ObjectClass)
	{
		return;
	}

	// iterate through all functions in the class
	for (TFieldIterator<UFunction> It(ObjectClass); It; ++It)
	{
		UFunction* Function = *It;
		if (!Function)
		{
			continue;
		}

		// check for YarnCommand meta
		FString CommandName;
		if (Function->HasMetaData(TEXT("YarnCommand")))
		{
			CommandName = Function->GetMetaData(TEXT("YarnCommand"));
			if (CommandName.IsEmpty())
			{
				// use the function name if no command name specified
				CommandName = Function->GetName();
			}

			// create a handler that invokes this function
			TWeakObjectPtr<UObject> WeakHandler(HandlerObject);
			FName FuncName = Function->GetFName();

			DialogueRunner->AddCommandHandler(CommandName, [WeakHandler, FuncName](const TArray<FString>& Parameters)
			{
				if (UObject* Handler = WeakHandler.Get())
				{
					if (UFunction* Func = Handler->FindFunction(FuncName))
					{
						// create the parameter struct
						// we expect the function to take a const TArray<FString>&
						struct FParams
						{
							TArray<FString> Parameters;
						};

						FParams Params;
						Params.Parameters = Parameters;
						Handler->ProcessEvent(Func, &Params);
					}
				}
			});

			UE_LOG(LogYarnSpinner, Log, TEXT("Registered command '%s' from %s::%s"),
				*CommandName, *ObjectClass->GetName(), *Function->GetName());
		}

		// check for YarnFunction meta
		FString FunctionName;
		if (Function->HasMetaData(TEXT("YarnFunction")))
		{
			FunctionName = Function->GetMetaData(TEXT("YarnFunction"));
			if (FunctionName.IsEmpty())
			{
				// use the function name if no yarn function name specified
				FunctionName = Function->GetName();
			}

			// count parameters (excluding return value)
			int32 ParamCount = 0;
			for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
			{
				if (!(ParamIt->PropertyFlags & CPF_ReturnParm))
				{
					ParamCount++;
				}
			}

			// create a handler that invokes this function
			TWeakObjectPtr<UObject> WeakHandler(HandlerObject);
			FName FuncName = Function->GetFName();

			DialogueRunner->AddFunction(FunctionName, [WeakHandler, FuncName](const TArray<FYarnValue>& Parameters) -> FYarnValue
			{
				if (UObject* Handler = WeakHandler.Get())
				{
					if (UFunction* Func = Handler->FindFunction(FuncName))
					{
						// create the parameter struct
						struct FParams
						{
							TArray<FYarnValue> Parameters;
							FYarnValue ReturnValue;
						};

						FParams Params;
						Params.Parameters = Parameters;
						Handler->ProcessEvent(Func, &Params);
						return Params.ReturnValue;
					}
				}
				return FYarnValue();
			}, ParamCount);

			UE_LOG(LogYarnSpinner, Log, TEXT("Registered function '%s' from %s::%s"),
				*FunctionName, *ObjectClass->GetName(), *Function->GetName());
		}
	}
#else
	// Metadata-based registration is only available in editor builds
	UE_LOG(LogYarnSpinner, Warning, TEXT("RegisterCommandsFromObject: Metadata-based command registration is only available in editor builds. Use RegisterCommandHandler/RegisterFunctionHandler directly."));
#endif
}

void UYarnCommandLibrary::UnregisterCommandsFromObject(UYarnDialogueRunner* DialogueRunner, UObject* HandlerObject)
{
#if WITH_EDITORONLY_DATA
	if (!DialogueRunner || !HandlerObject)
	{
		return;
	}

	UClass* ObjectClass = HandlerObject->GetClass();
	if (!ObjectClass)
	{
		return;
	}

	// iterate through all functions and unregister
	for (TFieldIterator<UFunction> It(ObjectClass); It; ++It)
	{
		UFunction* Function = *It;
		if (!Function)
		{
			continue;
		}

		if (Function->HasMetaData(TEXT("YarnCommand")))
		{
			FString CommandName = Function->GetMetaData(TEXT("YarnCommand"));
			if (CommandName.IsEmpty())
			{
				CommandName = Function->GetName();
			}
			DialogueRunner->RemoveCommandHandler(CommandName);
		}

		if (Function->HasMetaData(TEXT("YarnFunction")))
		{
			FString FunctionName = Function->GetMetaData(TEXT("YarnFunction"));
			if (FunctionName.IsEmpty())
			{
				FunctionName = Function->GetName();
			}
			DialogueRunner->RemoveFunction(FunctionName);
		}
	}
#endif
}

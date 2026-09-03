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
#include "YarnCommandRegistry.h"
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

void UYarnCommandLibrary::RegisterBlockingCommandHandler(UYarnDialogueRunner* DialogueRunner, const FString& CommandName, FYarnCommandHandlerBP Handler)
{
	if (!DialogueRunner)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("RegisterBlockingCommandHandler: DialogueRunner is null"));
		return;
	}

	if (CommandName.IsEmpty())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("RegisterBlockingCommandHandler: CommandName is empty"));
		return;
	}

	// wrap the blueprint delegate in a TFunction
	DialogueRunner->AddBlockingCommandHandler(CommandName, [Handler](const TArray<FString>& Parameters)
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

#if WITH_EDITORONLY_DATA
void UYarnCommandLibrary::CollectYarnActionsFromClass(const UClass* Class, TArray<FYarnBakedActionEntry>& OutEntries, bool bIncludeInherited)
{
	if (!Class)
	{
		return;
	}

	const EFieldIteratorFlags::SuperClassFlags SuperFlags =
		bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

	for (TFieldIterator<UFunction> It(Class, SuperFlags); It; ++It)
	{
		UFunction* Function = *It;
		if (!Function)
		{
			continue;
		}

		if (Function->HasMetaData(TEXT("YarnCommand")))
		{
			FYarnBakedActionEntry& Entry = OutEntries.AddDefaulted_GetRef();
			Entry.YarnName = Function->GetMetaData(TEXT("YarnCommand"));
			if (Entry.YarnName.IsEmpty())
			{
				// use the function name if no command name specified
				Entry.YarnName = Function->GetName();
			}
			Entry.OwningClass = Function->GetOwnerClass();
			Entry.FunctionName = Function->GetFName();
			Entry.bIsFunction = false;
		}

		if (Function->HasMetaData(TEXT("YarnFunction")))
		{
			FYarnBakedActionEntry& Entry = OutEntries.AddDefaulted_GetRef();
			Entry.YarnName = Function->GetMetaData(TEXT("YarnFunction"));
			if (Entry.YarnName.IsEmpty())
			{
				Entry.YarnName = Function->GetName();
			}
			Entry.OwningClass = Function->GetOwnerClass();
			Entry.FunctionName = Function->GetFName();
			Entry.bIsFunction = true;
		}
	}
}
#endif

namespace
{
	void CollectActionsForClass(const UClass* ObjectClass, TArray<FYarnBakedActionEntry>& OutEntries)
	{
#if WITH_EDITORONLY_DATA
		UYarnCommandLibrary::CollectYarnActionsFromClass(ObjectClass, OutEntries);
#else
		GetDefault<UYarnCommandRegistrySettings>()->GetEntriesForClass(ObjectClass, OutEntries);
#endif
	}
}

void UYarnCommandLibrary::RegisterCommandsFromObject(UYarnDialogueRunner* DialogueRunner, UObject* HandlerObject)
{
	if (!DialogueRunner || !HandlerObject)
	{
		return;
	}

	UClass* ObjectClass = HandlerObject->GetClass();
	if (!ObjectClass)
	{
		return;
	}

	TArray<FYarnBakedActionEntry> Entries;
	CollectActionsForClass(ObjectClass, Entries);

	for (const FYarnBakedActionEntry& Entry : Entries)
	{
		if (!HandlerObject->FindFunction(Entry.FunctionName))
		{
			UE_LOG(LogYarnSpinner, Warning, TEXT("RegisterCommandsFromObject: '%s' has no function '%s' for yarn name '%s' - skipping"),
				*ObjectClass->GetName(), *Entry.FunctionName.ToString(), *Entry.YarnName);
			continue;
		}

		TWeakObjectPtr<UObject> WeakHandler(HandlerObject);
		const FName FuncName = Entry.FunctionName;

		if (!Entry.bIsFunction)
		{
			DialogueRunner->AddCommandHandler(Entry.YarnName, [WeakHandler, FuncName](const TArray<FString>& Parameters)
			{
				if (UObject* Handler = WeakHandler.Get())
				{
					if (UFunction* Func = Handler->FindFunction(FuncName))
					{
						struct FParams
						{
							TArray<FString> Parameters;
						};

						if (Func->ParmsSize != sizeof(FParams))
						{
							UE_LOG(LogYarnSpinner, Error, TEXT("Yarn command handler '%s' has the wrong signature - expected (const TArray<FString>&)"), *FuncName.ToString());
							return;
						}

						FParams Params;
						Params.Parameters = Parameters;
						Handler->ProcessEvent(Func, &Params);
					}
				}
			});

			UE_LOG(LogYarnSpinner, Log, TEXT("Registered command '%s' from %s::%s"),
				*Entry.YarnName, *ObjectClass->GetName(), *Entry.FunctionName.ToString());
		}
		else
		{
			DialogueRunner->AddFunction(Entry.YarnName, [WeakHandler, FuncName](const TArray<FYarnValue>& Parameters) -> FYarnValue
			{
				if (UObject* Handler = WeakHandler.Get())
				{
					if (UFunction* Func = Handler->FindFunction(FuncName))
					{
						struct FParams
						{
							TArray<FYarnValue> Parameters;
							FYarnValue ReturnValue;
						};

						if (Func->ParmsSize != sizeof(FParams))
						{
							UE_LOG(LogYarnSpinner, Error, TEXT("Yarn function handler '%s' has the wrong signature - expected (const TArray<FYarnValue>&) returning FYarnValue"), *FuncName.ToString());
							return FYarnValue();
						}

						FParams Params;
						Params.Parameters = Parameters;
						Handler->ProcessEvent(Func, &Params);
						return Params.ReturnValue;
					}
				}
				return FYarnValue();
			}, -1);

			UE_LOG(LogYarnSpinner, Log, TEXT("Registered function '%s' from %s::%s"),
				*Entry.YarnName, *ObjectClass->GetName(), *Entry.FunctionName.ToString());
		}
	}
}

void UYarnCommandLibrary::UnregisterCommandsFromObject(UYarnDialogueRunner* DialogueRunner, UObject* HandlerObject)
{
	if (!DialogueRunner || !HandlerObject)
	{
		return;
	}

	TArray<FYarnBakedActionEntry> Entries;
	CollectActionsForClass(HandlerObject->GetClass(), Entries);

	for (const FYarnBakedActionEntry& Entry : Entries)
	{
		if (Entry.bIsFunction)
		{
			DialogueRunner->RemoveFunction(Entry.YarnName);
		}
		else
		{
			DialogueRunner->RemoveCommandHandler(Entry.YarnName);
		}
	}
}

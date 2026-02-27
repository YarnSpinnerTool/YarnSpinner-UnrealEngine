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

#include "YarnVariableStorage.h"
#include "YarnProgram.h"
#include "YarnSmartVariables.h"
#include "YarnSpinnerModule.h"

UYarnInMemoryVariableStorage::UYarnInMemoryVariableStorage()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UYarnInMemoryVariableStorage::ValidateVariableName(const FString& VariableName) const
{
	if (!VariableName.StartsWith(TEXT("$")))
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Variable name '%s' is not valid. Variable names must start with '$'. Did you mean '$%s'?"), *VariableName, *VariableName);
		return false;
	}
	return true;
}

void UYarnInMemoryVariableStorage::SetString_Implementation(const FString& VariableName, const FString& Value)
{
	if (!ValidateVariableName(VariableName))
	{
		return;
	}

	FYarnValue YarnValue(Value);
	Variables.Add(VariableName, YarnValue);
	NotifyVariableChanged(VariableName, YarnValue);
}

void UYarnInMemoryVariableStorage::SetNumber_Implementation(const FString& VariableName, float Value)
{
	if (!ValidateVariableName(VariableName))
	{
		return;
	}

	FYarnValue YarnValue(Value);
	Variables.Add(VariableName, YarnValue);
	NotifyVariableChanged(VariableName, YarnValue);
}

void UYarnInMemoryVariableStorage::SetBool_Implementation(const FString& VariableName, bool Value)
{
	if (!ValidateVariableName(VariableName))
	{
		return;
	}

	FYarnValue YarnValue(Value);
	Variables.Add(VariableName, YarnValue);
	NotifyVariableChanged(VariableName, YarnValue);
}

void UYarnInMemoryVariableStorage::SetValue_Implementation(const FString& VariableName, const FYarnValue& Value)
{
	if (!ValidateVariableName(VariableName))
	{
		return;
	}

	Variables.Add(VariableName, Value);
	NotifyVariableChanged(VariableName, Value);
}

bool UYarnInMemoryVariableStorage::TryGetValue_Implementation(const FString& VariableName, FYarnValue& OutValue)
{
	if (!ValidateVariableName(VariableName))
	{
		return false;
	}

	// first check our stored variables
	if (const FYarnValue* StoredValue = Variables.Find(VariableName))
	{
		OutValue = *StoredValue;
		return true;
	}

	// check smart variable evaluators
	if (TryGetSmartValue(VariableName, OutValue))
	{
		return true;
	}

	// if not found, check initial values from the yarn project
	if (YarnProject.IsValid())
	{
		if (YarnProject->TryGetInitialValue(VariableName, OutValue))
		{
			return true;
		}
	}

	return false;
}

bool UYarnInMemoryVariableStorage::Contains_Implementation(const FString& VariableName)
{
	if (!ValidateVariableName(VariableName))
	{
		return false;
	}

	if (Variables.Contains(VariableName))
	{
		return true;
	}

	// check initial values from the yarn project
	if (YarnProject.IsValid())
	{
		FYarnValue TempValue;
		return YarnProject->TryGetInitialValue(VariableName, TempValue);
	}

	return false;
}

void UYarnInMemoryVariableStorage::Clear_Implementation()
{
	Variables.Empty();
}

void UYarnInMemoryVariableStorage::SetYarnProject(UYarnProject* Project)
{
	YarnProject = Project;
}

IYarnSmartVariableEvaluator* UYarnInMemoryVariableStorage::GetSmartVariableEvaluator() const
{
	return CompiledSmartVariableEvaluator;
}

void UYarnInMemoryVariableStorage::SetSmartVariableEvaluator(IYarnSmartVariableEvaluator* Evaluator)
{
	CompiledSmartVariableEvaluator = Evaluator;
}

void UYarnInMemoryVariableStorage::GetAllVariables(
	TMap<FString, float>& OutFloats,
	TMap<FString, FString>& OutStrings,
	TMap<FString, bool>& OutBools
) const
{
	OutFloats.Empty();
	OutStrings.Empty();
	OutBools.Empty();

	for (const TPair<FString, FYarnValue>& Pair : Variables)
	{
		const FString& Name = Pair.Key;
		const FYarnValue& Value = Pair.Value;

		switch (Value.Type)
		{
		case EYarnValueType::Number:
			OutFloats.Add(Name, Value.GetNumberValue());
			break;
		case EYarnValueType::String:
			OutStrings.Add(Name, Value.GetStringValue());
			break;
		case EYarnValueType::Bool:
			OutBools.Add(Name, Value.GetBoolValue());
			break;
		default:
			break;
		}
	}
}

void UYarnInMemoryVariableStorage::SetAllVariables(
	const TMap<FString, float>& Floats,
	const TMap<FString, FString>& Strings,
	const TMap<FString, bool>& Bools,
	bool bClearFirst
)
{
	if (bClearFirst)
	{
		Variables.Empty();
	}

	for (const TPair<FString, float>& Pair : Floats)
	{
		SetNumber_Implementation(Pair.Key, Pair.Value);
	}

	for (const TPair<FString, FString>& Pair : Strings)
	{
		SetString_Implementation(Pair.Key, Pair.Value);
	}

	for (const TPair<FString, bool>& Pair : Bools)
	{
		SetBool_Implementation(Pair.Key, Pair.Value);
	}
}

FString UYarnInMemoryVariableStorage::GetDebugString() const
{
	FString Result;

	for (const TPair<FString, FYarnValue>& Pair : Variables)
	{
		const FString& Name = Pair.Key;
		const FYarnValue& Value = Pair.Value;

		FString TypeName;
		switch (Value.Type)
		{
		case EYarnValueType::Number:
			TypeName = TEXT("Number");
			break;
		case EYarnValueType::String:
			TypeName = TEXT("String");
			break;
		case EYarnValueType::Bool:
			TypeName = TEXT("Bool");
			break;
		default:
			TypeName = TEXT("None");
			break;
		}

		Result += FString::Printf(TEXT("%s = %s (%s)\n"), *Name, *Value.ConvertToString(), *TypeName);
	}

	return Result;
}

void UYarnInMemoryVariableStorage::NotifyVariableChanged(const FString& VariableName, const FYarnValue& NewValue)
{
	// Broadcast general event
	OnVariableChanged.Broadcast(VariableName, NewValue);

	// Notify typed listeners based on value type
	// THREAD SAFETY: Copy listener arrays before iterating to prevent issues
	// if a callback adds or removes listeners during iteration
	switch (NewValue.Type)
	{
	case EYarnValueType::String:
		{
			// Copy the listener array to allow safe modification during iteration
			TArray<FStringListener> ListenersCopy = StringListeners;
			for (const FStringListener& Listener : ListenersCopy)
			{
				if (Listener.VariableName.Equals(VariableName, ESearchCase::IgnoreCase))
				{
					Listener.Callback.ExecuteIfBound(VariableName, NewValue.GetStringValue());
				}
			}
		}
		break;

	case EYarnValueType::Number:
		{
			// Copy the listener array to allow safe modification during iteration
			TArray<FNumberListener> ListenersCopy = NumberListeners;
			for (const FNumberListener& Listener : ListenersCopy)
			{
				if (Listener.VariableName.Equals(VariableName, ESearchCase::IgnoreCase))
				{
					Listener.Callback.ExecuteIfBound(VariableName, NewValue.GetNumberValue());
				}
			}
		}
		break;

	case EYarnValueType::Bool:
		{
			// Copy the listener array to allow safe modification during iteration
			TArray<FBoolListener> ListenersCopy = BoolListeners;
			for (const FBoolListener& Listener : ListenersCopy)
			{
				if (Listener.VariableName.Equals(VariableName, ESearchCase::IgnoreCase))
				{
					Listener.Callback.ExecuteIfBound(VariableName, NewValue.GetBoolValue());
				}
			}
		}
		break;

	default:
		break;
	}
}

FYarnVariableListenerHandle UYarnInMemoryVariableStorage::AddStringChangeListener(const FString& VariableName, FOnYarnStringVariableChanged Callback)
{
	FStringListener Listener;
	Listener.Id = NextListenerId++;
	Listener.VariableName = VariableName;
	Listener.Callback = Callback;
	StringListeners.Add(Listener);
	return FYarnVariableListenerHandle(Listener.Id);
}

FYarnVariableListenerHandle UYarnInMemoryVariableStorage::AddNumberChangeListener(const FString& VariableName, FOnYarnNumberVariableChanged Callback)
{
	FNumberListener Listener;
	Listener.Id = NextListenerId++;
	Listener.VariableName = VariableName;
	Listener.Callback = Callback;
	NumberListeners.Add(Listener);
	return FYarnVariableListenerHandle(Listener.Id);
}

FYarnVariableListenerHandle UYarnInMemoryVariableStorage::AddBoolChangeListener(const FString& VariableName, FOnYarnBoolVariableChanged Callback)
{
	FBoolListener Listener;
	Listener.Id = NextListenerId++;
	Listener.VariableName = VariableName;
	Listener.Callback = Callback;
	BoolListeners.Add(Listener);
	return FYarnVariableListenerHandle(Listener.Id);
}

void UYarnInMemoryVariableStorage::RemoveChangeListener(FYarnVariableListenerHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	// search all listener arrays
	StringListeners.RemoveAll([&Handle](const FStringListener& L) { return L.Id == Handle.Id; });
	NumberListeners.RemoveAll([&Handle](const FNumberListener& L) { return L.Id == Handle.Id; });
	BoolListeners.RemoveAll([&Handle](const FBoolListener& L) { return L.Id == Handle.Id; });
}

void UYarnInMemoryVariableStorage::RegisterSmartVariableEvaluator(TScriptInterface<IYarnGameSmartVariableEvaluator> Evaluator)
{
	if (Evaluator)
	{
		SmartVariableEvaluators.AddUnique(Evaluator);
	}
}

void UYarnInMemoryVariableStorage::UnregisterSmartVariableEvaluator(TScriptInterface<IYarnGameSmartVariableEvaluator> Evaluator)
{
	SmartVariableEvaluators.Remove(Evaluator);
}

bool UYarnInMemoryVariableStorage::TryGetSmartValue(const FString& VariableName, FYarnValue& OutValue)
{
	for (const TScriptInterface<IYarnGameSmartVariableEvaluator>& Evaluator : SmartVariableEvaluators)
	{
		if (Evaluator && IYarnGameSmartVariableEvaluator::Execute_CanProvide(Evaluator.GetObject(), VariableName))
		{
			if (IYarnGameSmartVariableEvaluator::Execute_GetValue(Evaluator.GetObject(), VariableName, OutValue))
			{
				return true;
			}
		}
	}
	return false;
}

void UYarnInMemoryVariableStorage::GetAllVariablesAsMap(TMap<FString, FYarnValue>& OutVariables) const
{
	OutVariables = Variables;
}

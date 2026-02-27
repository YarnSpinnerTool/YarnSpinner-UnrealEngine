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

#include "YarnBlueprintLibrary.h"
#include "YarnProgram.h"
#include "YarnDialogueRunner.h"
#include "YarnVariableStorage.h"
#include "YarnSpinnerCore.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

// ============================================================================
// Project Inspection
// ============================================================================
// These functions let you query information about a yarn project from blueprints,
// useful for building node selection menus or displaying project stats.

TArray<FString> UYarnBlueprintLibrary::GetAllNodeNames(UYarnProject* YarnProject)
{
    TArray<FString> Result;

    if (!YarnProject || YarnProject->Program.Nodes.Num() == 0)
    {
        return Result;
    }

    for (const auto& NodePair : YarnProject->Program.Nodes)
    {
        Result.Add(NodePair.Key);
    }

    Result.Sort();
    return Result;
}

bool UYarnBlueprintLibrary::HasNode(UYarnProject* YarnProject, const FString& NodeName)
{
    if (!YarnProject || YarnProject->Program.Nodes.Num() == 0)
    {
        return false;
    }

    return YarnProject->Program.Nodes.Contains(NodeName);
}

TArray<FString> UYarnBlueprintLibrary::GetAllDeclaredVariableNames(UYarnProject* YarnProject)
{
    TArray<FString> Result;

    if (!YarnProject || YarnProject->Program.Nodes.Num() == 0)
    {
        return Result;
    }

    // These are variables that were declared with <<declare>> in the yarn scripts
    for (const auto& VarPair : YarnProject->Program.InitialValues)
    {
        Result.Add(VarPair.Key);
    }

    Result.Sort();
    return Result;
}

bool UYarnBlueprintLibrary::GetDeclaredVariableDefaultValue(UYarnProject* YarnProject, const FString& VariableName, FYarnValue& OutValue)
{
    if (!YarnProject || YarnProject->Program.Nodes.Num() == 0)
    {
        return false;
    }

    const FYarnValue* Found = YarnProject->Program.InitialValues.Find(VariableName);
    if (Found)
    {
        OutValue = *Found;
        return true;
    }

    return false;
}

int32 UYarnBlueprintLibrary::GetNodeCount(UYarnProject* YarnProject)
{
    if (!YarnProject || YarnProject->Program.Nodes.Num() == 0)
    {
        return 0;
    }

    return YarnProject->Program.Nodes.Num();
}

int32 UYarnBlueprintLibrary::GetLineCount(UYarnProject* YarnProject)
{
    if (!YarnProject)
    {
        return 0;
    }

    return YarnProject->BaseStringTable.Num();
}

TArray<FString> UYarnBlueprintLibrary::GetAllLineIDs(UYarnProject* YarnProject)
{
    TArray<FString> Result;

    if (!YarnProject)
    {
        return Result;
    }

    for (const auto& LinePair : YarnProject->BaseStringTable)
    {
        Result.Add(LinePair.Key);
    }

    Result.Sort();
    return Result;
}

TArray<FString> UYarnBlueprintLibrary::GetNodeTags(UYarnProject* YarnProject, const FString& NodeName)
{
    TArray<FString> Result;

    if (!YarnProject || YarnProject->Program.Nodes.Num() == 0)
    {
        return Result;
    }

    const FYarnNode* Node = YarnProject->Program.Nodes.Find(NodeName);
    if (Node)
    {
        // The yarn compiler stores tags in node headers with the key "tags"
        for (const FYarnHeader& Header : Node->Headers)
        {
            if (Header.Key.Equals(TEXT("tags"), ESearchCase::IgnoreCase))
            {
                TArray<FString> Tags;
                Header.Value.ParseIntoArray(Tags, TEXT(" "));
                Result.Append(Tags);
            }
        }
    }

    return Result;
}

// ============================================================================
// Variable Storage Helpers
// ============================================================================
// These functions wrap the variable storage interface for Blueprint convenience.

TArray<FString> UYarnBlueprintLibrary::GetAllVariableNames(UYarnDialogueRunner* DialogueRunner)
{
    TArray<FString> Result;

    if (!DialogueRunner)
    {
        return Result;
    }

    // The interface only supports get/set for specific variables, so we cast
    // to the concrete type to enumerate all variables.
    UYarnInMemoryVariableStorage* InMemoryStorage = Cast<UYarnInMemoryVariableStorage>(DialogueRunner->VariableStorage.GetObject());
    if (InMemoryStorage)
    {
        TMap<FString, FYarnValue> AllVariables;
        InMemoryStorage->GetAllVariablesAsMap(AllVariables);

        for (const auto& Pair : AllVariables)
        {
            Result.Add(Pair.Key);
        }
    }

    Result.Sort();
    return Result;
}

TMap<FString, FYarnValue> UYarnBlueprintLibrary::GetAllVariables(UYarnDialogueRunner* DialogueRunner)
{
    TMap<FString, FYarnValue> Result;

    if (!DialogueRunner)
    {
        return Result;
    }

    UYarnInMemoryVariableStorage* InMemoryStorage = Cast<UYarnInMemoryVariableStorage>(DialogueRunner->VariableStorage.GetObject());
    if (InMemoryStorage)
    {
        InMemoryStorage->GetAllVariablesAsMap(Result);
    }

    return Result;
}

bool UYarnBlueprintLibrary::HasVariable(UYarnDialogueRunner* DialogueRunner, const FString& VariableName)
{
    if (!DialogueRunner)
    {
        return false;
    }

    TScriptInterface<IYarnVariableStorage> Storage = DialogueRunner->VariableStorage;
    if (!Storage.GetInterface())
    {
        return false;
    }

    FYarnValue Value;
    return IYarnVariableStorage::Execute_TryGetValue(Storage.GetObject(), VariableName, Value);
}

bool UYarnBlueprintLibrary::GetVariableAsString(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, FString& OutValue)
{
    if (!DialogueRunner)
    {
        return false;
    }

    TScriptInterface<IYarnVariableStorage> Storage = DialogueRunner->VariableStorage;
    if (!Storage.GetInterface())
    {
        return false;
    }

    // GetStringValue works for any type; numbers and bools get converted
    FYarnValue Value;
    if (IYarnVariableStorage::Execute_TryGetValue(Storage.GetObject(), VariableName, Value))
    {
        OutValue = Value.GetStringValue();
        return true;
    }

    return false;
}

bool UYarnBlueprintLibrary::GetVariableAsNumber(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, float& OutValue)
{
    if (!DialogueRunner)
    {
        return false;
    }

    TScriptInterface<IYarnVariableStorage> Storage = DialogueRunner->VariableStorage;
    if (!Storage.GetInterface())
    {
        return false;
    }

    FYarnValue Value;
    if (IYarnVariableStorage::Execute_TryGetValue(Storage.GetObject(), VariableName, Value))
    {
        if (Value.Type == EYarnValueType::Number)
        {
            OutValue = Value.GetNumberValue();
            return true;
        }
    }

    return false;
}

bool UYarnBlueprintLibrary::GetVariableAsBool(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, bool& OutValue)
{
    if (!DialogueRunner)
    {
        return false;
    }

    TScriptInterface<IYarnVariableStorage> Storage = DialogueRunner->VariableStorage;
    if (!Storage.GetInterface())
    {
        return false;
    }

    FYarnValue Value;
    if (IYarnVariableStorage::Execute_TryGetValue(Storage.GetObject(), VariableName, Value))
    {
        if (Value.Type == EYarnValueType::Bool)
        {
            OutValue = Value.GetBoolValue();
            return true;
        }
    }

    return false;
}

void UYarnBlueprintLibrary::SetStringVariable(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, const FString& Value)
{
    if (!DialogueRunner)
    {
        return;
    }

    TScriptInterface<IYarnVariableStorage> Storage = DialogueRunner->VariableStorage;
    if (!Storage.GetInterface())
    {
        return;
    }

    IYarnVariableStorage::Execute_SetValue(Storage.GetObject(), VariableName, FYarnValue(Value));
}

void UYarnBlueprintLibrary::SetNumberVariable(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, float Value)
{
    if (!DialogueRunner)
    {
        return;
    }

    TScriptInterface<IYarnVariableStorage> Storage = DialogueRunner->VariableStorage;
    if (!Storage.GetInterface())
    {
        return;
    }

    IYarnVariableStorage::Execute_SetValue(Storage.GetObject(), VariableName, FYarnValue(Value));
}

void UYarnBlueprintLibrary::SetBoolVariable(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, bool Value)
{
    if (!DialogueRunner)
    {
        return;
    }

    TScriptInterface<IYarnVariableStorage> Storage = DialogueRunner->VariableStorage;
    if (!Storage.GetInterface())
    {
        return;
    }

    IYarnVariableStorage::Execute_SetValue(Storage.GetObject(), VariableName, FYarnValue(Value));
}

// ============================================================================
// Persistence
// ============================================================================
// These functions use Unreal's USaveGame system to save and load yarn variables to disk.

bool UYarnBlueprintLibrary::SaveVariablesToSlot(UYarnDialogueRunner* DialogueRunner, const FString& SlotName, int32 UserIndex)
{
    if (!DialogueRunner)
    {
        return false;
    }

    UYarnInMemoryVariableStorage* InMemoryStorage = Cast<UYarnInMemoryVariableStorage>(DialogueRunner->VariableStorage.GetObject());
    if (!InMemoryStorage)
    {
        return false;
    }

    UYarnVariableSaveGame* SaveGame = NewObject<UYarnVariableSaveGame>();
    InMemoryStorage->GetAllVariables(SaveGame->FloatVariables, SaveGame->StringVariables, SaveGame->BoolVariables);

    return UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
}

bool UYarnBlueprintLibrary::LoadVariablesFromSlot(UYarnDialogueRunner* DialogueRunner, const FString& SlotName, int32 UserIndex)
{
    if (!DialogueRunner)
    {
        return false;
    }

    UYarnInMemoryVariableStorage* InMemoryStorage = Cast<UYarnInMemoryVariableStorage>(DialogueRunner->VariableStorage.GetObject());
    if (!InMemoryStorage)
    {
        return false;
    }

    USaveGame* LoadedGame = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
    UYarnVariableSaveGame* SaveGame = Cast<UYarnVariableSaveGame>(LoadedGame);
    if (!SaveGame)
    {
        return false;
    }

    InMemoryStorage->SetAllVariables(SaveGame->FloatVariables, SaveGame->StringVariables, SaveGame->BoolVariables, true);
    return true;
}

bool UYarnBlueprintLibrary::DeleteVariableSlot(const FString& SlotName, int32 UserIndex)
{
    return UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
}

bool UYarnBlueprintLibrary::DoesVariableSlotExist(const FString& SlotName, int32 UserIndex)
{
    return UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
}

// ============================================================================
// Struct Helpers
// ============================================================================
// Blueprint-pure wrappers for common struct queries.

bool UYarnBlueprintLibrary::IsLocalizedLineValid(const FYarnLocalizedLine& Line)
{
    return Line.IsValid();
}

bool UYarnBlueprintLibrary::LocalizedLineHasMetadata(const FYarnLocalizedLine& Line, const FString& Tag)
{
    return Line.HasMetadata(Tag);
}

bool UYarnBlueprintLibrary::HasAvailableOptions(const FYarnOptionSet& OptionSet)
{
    return OptionSet.HasAvailableOptions();
}

int32 UYarnBlueprintLibrary::GetOptionCount(const FYarnOptionSet& OptionSet)
{
    return OptionSet.Options.Num();
}

bool UYarnBlueprintLibrary::IsCommandValid(const FYarnCommand& Command)
{
    return Command.IsValid();
}

bool UYarnBlueprintLibrary::IsYarnValueValid(const FYarnValue& Value)
{
    return Value.IsValid();
}

// ============================================================================
// Value Conversion Helpers
// ============================================================================
// Blueprint-pure functions for creating and converting FYarnValue structs.

FYarnValue UYarnBlueprintLibrary::MakeYarnValueFromString(const FString& Value)
{
    return FYarnValue(Value);
}

FYarnValue UYarnBlueprintLibrary::MakeYarnValueFromNumber(float Value)
{
    return FYarnValue(Value);
}

FYarnValue UYarnBlueprintLibrary::MakeYarnValueFromBool(bool Value)
{
    return FYarnValue(Value);
}

FString UYarnBlueprintLibrary::YarnValueToString(const FYarnValue& Value)
{
    switch (Value.Type)
    {
    case EYarnValueType::String:
        return Value.GetStringValue();
    case EYarnValueType::Number:
        return FString::Printf(TEXT("%.4f"), Value.GetNumberValue());
    case EYarnValueType::Bool:
        return Value.GetBoolValue() ? TEXT("true") : TEXT("false");
    default:
        return TEXT("(unknown)");
    }
}

FString UYarnBlueprintLibrary::GetYarnValueTypeName(const FYarnValue& Value)
{
    switch (Value.Type)
    {
    case EYarnValueType::String:
        return TEXT("String");
    case EYarnValueType::Number:
        return TEXT("Number");
    case EYarnValueType::Bool:
        return TEXT("Bool");
    default:
        return TEXT("Unknown");
    }
}

// ============================================================================
// Dialogue Runner Helpers
// ============================================================================
// Utility functions to find dialogue runners in the world.

UYarnDialogueRunner* UYarnBlueprintLibrary::GetDialogueRunner(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }

    return Actor->FindComponentByClass<UYarnDialogueRunner>();
}

TArray<UYarnDialogueRunner*> UYarnBlueprintLibrary::GetAllDialogueRunners(UObject* WorldContextObject)
{
    TArray<UYarnDialogueRunner*> Result;

    if (!WorldContextObject)
    {
        return Result;
    }

    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return Result;
    }

    // This iterates all actors, so avoid calling it every frame; cache the result if needed
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (UYarnDialogueRunner* Runner = Actor->FindComponentByClass<UYarnDialogueRunner>())
        {
            Result.Add(Runner);
        }
    }

    return Result;
}

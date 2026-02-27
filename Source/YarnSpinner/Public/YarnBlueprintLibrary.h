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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/SaveGame.h"
#include "YarnSpinnerCore.h"
#include "YarnBlueprintLibrary.generated.h"

class UYarnProject;
class UYarnDialogueRunner;

/**
 * Save game object for persisting Yarn Spinner variables to disk.
 *
 * Used internally by UYarnBlueprintLibrary's persistence functions.
 * Variables are split by type for clean serialisation.
 */
UCLASS()
class YARNSPINNER_API UYarnVariableSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TMap<FString, float> FloatVariables;

    UPROPERTY()
    TMap<FString, FString> StringVariables;

    UPROPERTY()
    TMap<FString, bool> BoolVariables;
};

/**
 * Blueprint function library for Yarn Spinner utilities.
 *
 * Provides convenient Blueprint-callable functions for working with
 * Yarn projects, dialogue runners, and variables.
 */
UCLASS()
class YARNSPINNER_API UYarnBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // ========================================================================
    // Project Inspection
    // ========================================================================

    /**
     * Get all node names in a Yarn project.
     *
     * @param YarnProject The project to inspect
     * @return Array of all node names
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Project")
    static TArray<FString> GetAllNodeNames(UYarnProject* YarnProject);

    /**
     * Check if a node exists in a Yarn project.
     *
     * @param YarnProject The project to check
     * @param NodeName The node name to look for
     * @return True if the node exists
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Project")
    static bool HasNode(UYarnProject* YarnProject, const FString& NodeName);

    /**
     * Get all variable names declared in a Yarn project.
     *
     * @param YarnProject The project to inspect
     * @return Array of all variable names (including $ prefix)
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Project")
    static TArray<FString> GetAllDeclaredVariableNames(UYarnProject* YarnProject);

    /**
     * Get the initial value of a declared variable.
     *
     * @param YarnProject The project to check
     * @param VariableName The variable name (with $ prefix)
     * @param OutValue The initial value
     * @return True if the variable was found
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Project")
    static bool GetDeclaredVariableDefaultValue(UYarnProject* YarnProject, const FString& VariableName, FYarnValue& OutValue);

    /**
     * Get the number of nodes in a Yarn project.
     *
     * @param YarnProject The project to count
     * @return Number of nodes
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Project")
    static int32 GetNodeCount(UYarnProject* YarnProject);

    /**
     * Get the number of lines in a Yarn project.
     *
     * @param YarnProject The project to count
     * @return Number of lines
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Project")
    static int32 GetLineCount(UYarnProject* YarnProject);

    /**
     * Get all line IDs in a Yarn project.
     *
     * @param YarnProject The project to inspect
     * @return Array of all line IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Project")
    static TArray<FString> GetAllLineIDs(UYarnProject* YarnProject);

    /**
     * Get the tags for a specific node.
     *
     * @param YarnProject The project containing the node
     * @param NodeName The node to get tags for
     * @return Array of tags for this node
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Project")
    static TArray<FString> GetNodeTags(UYarnProject* YarnProject, const FString& NodeName);

    // ========================================================================
    // Variable Storage Helpers
    // ========================================================================

    /**
     * Get all variable names from a dialogue runner's storage.
     *
     * @param DialogueRunner The runner to inspect
     * @return Array of variable names currently in storage
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static TArray<FString> GetAllVariableNames(UYarnDialogueRunner* DialogueRunner);

    /**
     * Get all variables and their values from a dialogue runner.
     *
     * @param DialogueRunner The runner to inspect
     * @return Map of variable names to values
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static TMap<FString, FYarnValue> GetAllVariables(UYarnDialogueRunner* DialogueRunner);

    /**
     * Check if a variable exists in storage.
     *
     * @param DialogueRunner The runner to check
     * @param VariableName The variable name (with $ prefix)
     * @return True if the variable exists
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static bool HasVariable(UYarnDialogueRunner* DialogueRunner, const FString& VariableName);

    /**
     * Get a variable as a string (works for any type).
     *
     * @param DialogueRunner The runner to query
     * @param VariableName The variable name (with $ prefix)
     * @param OutValue The value as a string
     * @return True if the variable was found
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static bool GetVariableAsString(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, FString& OutValue);

    /**
     * Get a variable as a number.
     *
     * @param DialogueRunner The runner to query
     * @param VariableName The variable name (with $ prefix)
     * @param OutValue The value as a float
     * @return True if the variable was found and is a number
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static bool GetVariableAsNumber(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, float& OutValue);

    /**
     * Get a variable as a boolean.
     *
     * @param DialogueRunner The runner to query
     * @param VariableName The variable name (with $ prefix)
     * @param OutValue The value as a bool
     * @return True if the variable was found and is a bool
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static bool GetVariableAsBool(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, bool& OutValue);

    /**
     * Set a string variable.
     *
     * @param DialogueRunner The runner to modify
     * @param VariableName The variable name (with $ prefix)
     * @param Value The value to set
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static void SetStringVariable(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, const FString& Value);

    /**
     * Set a number variable.
     *
     * @param DialogueRunner The runner to modify
     * @param VariableName The variable name (with $ prefix)
     * @param Value The value to set
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static void SetNumberVariable(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, float Value);

    /**
     * Set a boolean variable.
     *
     * @param DialogueRunner The runner to modify
     * @param VariableName The variable name (with $ prefix)
     * @param Value The value to set
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
    static void SetBoolVariable(UYarnDialogueRunner* DialogueRunner, const FString& VariableName, bool Value);

    // ========================================================================
    // Persistence
    // ========================================================================

    /**
     * Save all Yarn variables to a save slot on disk.
     *
     * Uses Unreal's USaveGame system to persist float, string, and bool
     * variables from the runner's in-memory variable storage.
     *
     * @param DialogueRunner The runner whose variables to save
     * @param SlotName The save slot name (default "YarnVariables")
     * @param UserIndex The user index for the save slot
     * @return True if the save succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Persistence")
    static bool SaveVariablesToSlot(UYarnDialogueRunner* DialogueRunner, const FString& SlotName = TEXT("YarnVariables"), int32 UserIndex = 0);

    /**
     * Load Yarn variables from a save slot on disk.
     *
     * Replaces all current variables in the runner's storage with the
     * saved values. Existing variables are cleared first.
     *
     * @param DialogueRunner The runner whose variables to restore
     * @param SlotName The save slot name (default "YarnVariables")
     * @param UserIndex The user index for the save slot
     * @return True if the load succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Persistence")
    static bool LoadVariablesFromSlot(UYarnDialogueRunner* DialogueRunner, const FString& SlotName = TEXT("YarnVariables"), int32 UserIndex = 0);

    /**
     * Delete a Yarn variable save slot from disk.
     *
     * @param SlotName The save slot name to delete (default "YarnVariables")
     * @param UserIndex The user index for the save slot
     * @return True if the slot was deleted
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Persistence")
    static bool DeleteVariableSlot(const FString& SlotName = TEXT("YarnVariables"), int32 UserIndex = 0);

    /**
     * Check if a Yarn variable save slot exists on disk.
     *
     * @param SlotName The save slot name to check (default "YarnVariables")
     * @param UserIndex The user index for the save slot
     * @return True if the slot exists
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Persistence")
    static bool DoesVariableSlotExist(const FString& SlotName = TEXT("YarnVariables"), int32 UserIndex = 0);

    // ========================================================================
    // Struct Helpers
    // ========================================================================

    /**
     * Check if a localised line is valid (has a line ID).
     *
     * @param Line The line to check
     * @return True if the line has a valid line ID
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Lines")
    static bool IsLocalizedLineValid(const FYarnLocalizedLine& Line);

    /**
     * Check if a localised line has a specific metadata tag.
     *
     * @param Line The line to check
     * @param Tag The metadata tag to look for (e.g., "angry", "whisper")
     * @return True if the line has this tag
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Lines")
    static bool LocalizedLineHasMetadata(const FYarnLocalizedLine& Line, const FString& Tag);

    /**
     * Check if an option set has at least one available (selectable) option.
     *
     * @param OptionSet The option set to check
     * @return True if at least one option has bIsAvailable = true
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Options")
    static bool HasAvailableOptions(const FYarnOptionSet& OptionSet);

    /**
     * Get the number of options in an option set.
     *
     * @param OptionSet The option set to count
     * @return The number of options
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Options")
    static int32 GetOptionCount(const FYarnOptionSet& OptionSet);

    /**
     * Check if a Yarn command is valid (has a command name).
     *
     * @param Command The command to check
     * @return True if the command has a name
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Commands")
    static bool IsCommandValid(const FYarnCommand& Command);

    /**
     * Check if a Yarn value is valid (not uninitialised).
     *
     * @param Value The value to check
     * @return True if the value has been initialised with a type
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Values")
    static bool IsYarnValueValid(const FYarnValue& Value);

    // ========================================================================
    // Value Conversion Helpers
    // ========================================================================

    /**
     * Create a Yarn value from a string.
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Values")
    static FYarnValue MakeYarnValueFromString(const FString& Value);

    /**
     * Create a Yarn value from a number.
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Values")
    static FYarnValue MakeYarnValueFromNumber(float Value);

    /**
     * Create a Yarn value from a boolean.
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Values")
    static FYarnValue MakeYarnValueFromBool(bool Value);

    /**
     * Convert a Yarn value to a display string.
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Values")
    static FString YarnValueToString(const FYarnValue& Value);

    /**
     * Get the type of a Yarn value as a string.
     */
    UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Values")
    static FString GetYarnValueTypeName(const FYarnValue& Value);

    // ========================================================================
    // Dialogue Runner Helpers
    // ========================================================================

    /**
     * Find a dialogue runner on the specified actor.
     *
     * @param Actor The actor to search
     * @return The dialogue runner component, or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
    static UYarnDialogueRunner* GetDialogueRunner(AActor* Actor);

    /**
     * Find all dialogue runners in the current level.
     *
     * @param WorldContextObject Context object for world access
     * @return Array of all dialogue runners in the level
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner", meta = (WorldContext = "WorldContextObject"))
    static TArray<UYarnDialogueRunner*> GetAllDialogueRunners(UObject* WorldContextObject);
};

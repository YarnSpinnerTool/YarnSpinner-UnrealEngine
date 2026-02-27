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

// ============================================================================
// YarnVariableStorage.h
// ============================================================================
//
// Variable storage is where yarn variables ($variableName) are kept. The
// dialogue runner uses this to get and set variables during execution.
//
// Blueprint users:
//   - Use the default in-memory storage (created automatically)
//   - Bind to OnVariableChanged event for notifications
//   - Use AddStringChangeListener/etc for specific variable callbacks
//   - Implement IYarnSmartVariableEvaluator for computed variables
//
// C++ users:
//   - Can implement IYarnVariableStorage for custom storage
//   - Can use UYarnInMemoryVariableStorage's GetAllVariables/SetAllVariables
//     for serialisation
//
// The default in-memory storage does NOT persist across sessions. For save
// games, use GetAllVariables to serialise and SetAllVariables to restore.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "YarnSpinnerCore.h"
#include "YarnProgram.h"
#include "YarnVariableStorage.generated.h"

class UYarnProject;
class UYarnInMemoryVariableStorage;
class IYarnSmartVariableEvaluator;

// ============================================================================
// delegates for variable change notifications
// ============================================================================

/** fired when any variable value changes. broadcast from in-memory storage. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnYarnVariableChanged, const FString&, VariableName, const FYarnValue&, NewValue);

/** delegate for string variable change listeners (used with AddStringChangeListener) */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnYarnStringVariableChanged, const FString&, VariableName, const FString&, NewValue);

/** delegate for number variable change listeners (used with AddNumberChangeListener) */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnYarnNumberVariableChanged, const FString&, VariableName, float, NewValue);

/** delegate for bool variable change listeners (used with AddBoolChangeListener) */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnYarnBoolVariableChanged, const FString&, VariableName, bool, NewValue);

// ============================================================================
// IYarnGameSmartVariableEvaluator
// ============================================================================
//
// Game smart variables are computed on-the-fly rather than stored. For example,
// you might want $player_health to always return the current health value
// from your game systems, rather than having to manually update a yarn variable.
//
// This is different from the compiled expression evaluator in YarnSmartVariables.h;
// this interface is for binding live game state to yarn variables.

/**
 * Interface for game smart variable evaluators.
 *
 * Implement this to provide computed variables that derive their values
 * from game state rather than being stored directly.
 *
 * Example: $player_health could return Character->GetHealth()
 *
 * Register your evaluator with:
 *   VariableStorage->RegisterSmartVariableEvaluator(MyEvaluator)
 *
 * For the compiled expression evaluator, see IYarnSmartVariableEvaluator in
 * YarnSmartVariables.h.
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class UYarnGameSmartVariableEvaluator : public UInterface
{
	GENERATED_BODY()
};

class YARNSPINNER_API IYarnGameSmartVariableEvaluator
{
	GENERATED_BODY()

public:
	/**
	 * get the value of a smart variable.
	 *
	 * called when yarn reads a variable. if this evaluator handles the
	 * variable, compute and return the value.
	 *
	 * @param VariableName the name of the variable being requested.
	 * @param OutValue the computed value to return.
	 * @return true if this evaluator handles the variable.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Smart Variables")
	bool GetValue(const FString& VariableName, FYarnValue& OutValue);

	/**
	 * check if this evaluator can provide a value for the given variable.
	 *
	 * called during variable storage lookup to determine if this evaluator
	 * should be consulted for the variable.
	 *
	 * @param VariableName the variable name to check.
	 * @return true if this evaluator handles the variable.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Smart Variables")
	bool CanProvide(const FString& VariableName);
};

// ============================================================================
// FYarnVariableListenerHandle
// ============================================================================

/**
 * handle returned when registering a variable change listener.
 * store this handle and pass it to RemoveChangeListener when you're done.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnVariableListenerHandle
{
	GENERATED_BODY()

	FYarnVariableListenerHandle() : Id(0) {}
	FYarnVariableListenerHandle(int32 InId) : Id(InId) {}

	/** check if this handle refers to a valid listener */
	bool IsValid() const { return Id > 0; }

	UPROPERTY()
	int32 Id;
};

// ============================================================================
// IYarnVariableStorage
// ============================================================================
//
// The interface that all variable storage implementations must conform to.
// The dialogue runner uses this to get and set variables during execution.
//
// To implement your own storage (e.g., backed by a database or integrated
// with your save system), implement this interface on a UObject.

/**
 * Interface for Yarn Spinner variable storage.
 *
 * Implement this interface to provide custom variable storage, e.g.:
 *   - Persistent storage that survives game sessions
 *   - Cloud-synced storage
 *   - Integration with your game's existing data systems
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UYarnVariableStorage : public UInterface
{
	GENERATED_BODY()
};

class YARNSPINNER_API IYarnVariableStorage
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------------
	// typed setters (convenience methods)
	// ------------------------------------------------------------------------

	/**
	 * set a variable to a string value.
	 * @param VariableName the variable name (must start with $).
	 * @param Value the string value to set.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner|Variables")
	void SetString(const FString& VariableName, const FString& Value);

	/**
	 * set a variable to a number value.
	 * @param VariableName the variable name (must start with $).
	 * @param Value the number value to set.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner|Variables")
	void SetNumber(const FString& VariableName, float Value);

	/**
	 * set a variable to a boolean value.
	 * @param VariableName the variable name (must start with $).
	 * @param Value the boolean value to set.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner|Variables")
	void SetBool(const FString& VariableName, bool Value);

	/**
	 * set a variable from a FYarnValue (generic setter).
	 * @param VariableName the variable name (must start with $).
	 * @param Value the value to set.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner|Variables")
	void SetValue(const FString& VariableName, const FYarnValue& Value);

	// ------------------------------------------------------------------------
	// getters
	// ------------------------------------------------------------------------

	/**
	 * try to get the value of a variable.
	 * @param VariableName the variable name to look up.
	 * @param OutValue the value, if found.
	 * @return true if the variable was found.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner|Variables")
	bool TryGetValue(const FString& VariableName, FYarnValue& OutValue);

	/**
	 * check if a variable exists in storage.
	 * @param VariableName the variable name to check.
	 * @return true if the variable exists.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner|Variables")
	bool Contains(const FString& VariableName);

	// ------------------------------------------------------------------------
	// storage management
	// ------------------------------------------------------------------------

	/**
	 * remove all variables from storage.
	 * useful for starting a new game.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Yarn Spinner|Variables")
	void Clear();

	/**
	 * set the yarn project, allowing access to initial values.
	 * called by the dialogue runner when setting up.
	 * @param Project the yarn project to use.
	 */
	virtual void SetYarnProject(UYarnProject* Project) = 0;

	// ------------------------------------------------------------------------
	// smart variable evaluation
	// ------------------------------------------------------------------------

	/**
	 * Get the smart variable evaluator for this storage.
	 * Used for evaluating compiled smart variable nodes.
	 *
	 * The dialogue runner sets this to itself when it creates/configures storage.
	 *
	 * @return The smart variable evaluator, or nullptr if not set.
	 */
	virtual IYarnSmartVariableEvaluator* GetSmartVariableEvaluator() const { return nullptr; }

	/**
	 * Set the smart variable evaluator for this storage.
	 * Typically called by the dialogue runner with itself.
	 *
	 * @param Evaluator The smart variable evaluator to use.
	 */
	virtual void SetSmartVariableEvaluator(IYarnSmartVariableEvaluator* Evaluator) {}
};

// ============================================================================
// UYarnInMemoryVariableStorage
// ============================================================================
//
// The default variable storage implementation. Stores variables in memory
// (a TMap). Variables are lost when the game exits.
//
// To persist variables across sessions:
//   1. Call GetAllVariables() to get maps of all variables
//   2. Save those maps to your save game
//   3. On load, call SetAllVariables() to restore them

/**
 * In-memory variable storage component.
 *
 * Stores variables in memory as a TMap. Variables are lost when the game
 * exits unless you serialise them yourself.
 *
 * Features:
 *   - OnVariableChanged event for monitoring all changes
 *   - Typed change listeners for specific variables
 *   - Smart variable support for computed values
 *   - Serialisation helpers for save games
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable)
class YARNSPINNER_API UYarnInMemoryVariableStorage : public UActorComponent, public IYarnVariableStorage
{
	GENERATED_BODY()

public:
	UYarnInMemoryVariableStorage();

	// ------------------------------------------------------------------------
	// IYarnVariableStorage implementation
	// ------------------------------------------------------------------------

	virtual void SetString_Implementation(const FString& VariableName, const FString& Value) override;
	virtual void SetNumber_Implementation(const FString& VariableName, float Value) override;
	virtual void SetBool_Implementation(const FString& VariableName, bool Value) override;
	virtual void SetValue_Implementation(const FString& VariableName, const FYarnValue& Value) override;
	virtual bool TryGetValue_Implementation(const FString& VariableName, FYarnValue& OutValue) override;
	virtual bool Contains_Implementation(const FString& VariableName) override;
	virtual void Clear_Implementation() override;
	virtual void SetYarnProject(UYarnProject* Project) override;
	virtual IYarnSmartVariableEvaluator* GetSmartVariableEvaluator() const override;
	virtual void SetSmartVariableEvaluator(IYarnSmartVariableEvaluator* Evaluator) override;

	// ------------------------------------------------------------------------
	// change notification
	// ------------------------------------------------------------------------

	/**
	 * event fired when any variable changes.
	 * bind to this in blueprints to monitor variable changes.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Variables")
	FOnYarnVariableChanged OnVariableChanged;

	// ------------------------------------------------------------------------
	// typed variable change listeners
	// ------------------------------------------------------------------------
	// these let you listen for changes to specific variables, with the value
	// already converted to the expected type. useful in blueprints.

	/**
	 * add a listener for changes to a specific string variable.
	 * the callback receives the variable name and new string value.
	 * @param VariableName the variable name to listen for (must start with $).
	 * @param Callback the callback to invoke when the variable changes.
	 * @return a handle to use for removing the listener.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables|Listeners")
	FYarnVariableListenerHandle AddStringChangeListener(const FString& VariableName, FOnYarnStringVariableChanged Callback);

	/**
	 * add a listener for changes to a specific number variable.
	 * the callback receives the variable name and new number value.
	 * @param VariableName the variable name to listen for (must start with $).
	 * @param Callback the callback to invoke when the variable changes.
	 * @return a handle to use for removing the listener.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables|Listeners")
	FYarnVariableListenerHandle AddNumberChangeListener(const FString& VariableName, FOnYarnNumberVariableChanged Callback);

	/**
	 * add a listener for changes to a specific bool variable.
	 * the callback receives the variable name and new bool value.
	 * @param VariableName the variable name to listen for (must start with $).
	 * @param Callback the callback to invoke when the variable changes.
	 * @return a handle to use for removing the listener.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables|Listeners")
	FYarnVariableListenerHandle AddBoolChangeListener(const FString& VariableName, FOnYarnBoolVariableChanged Callback);

	/**
	 * remove a previously registered change listener.
	 * @param Handle the handle returned from Add*ChangeListener.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables|Listeners")
	void RemoveChangeListener(FYarnVariableListenerHandle Handle);

	// ------------------------------------------------------------------------
	// smart variables
	// ------------------------------------------------------------------------
	// smart variables are computed on-the-fly rather than stored. see
	// IYarnSmartVariableEvaluator above.

	/**
	 * register a game smart variable evaluator.
	 * the evaluator will be consulted for variables it handles.
	 * @param Evaluator the evaluator to register.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Smart Variables")
	void RegisterSmartVariableEvaluator(TScriptInterface<IYarnGameSmartVariableEvaluator> Evaluator);

	/**
	 * unregister a game smart variable evaluator.
	 * @param Evaluator the evaluator to unregister.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Smart Variables")
	void UnregisterSmartVariableEvaluator(TScriptInterface<IYarnGameSmartVariableEvaluator> Evaluator);

	// ------------------------------------------------------------------------
	// serialisation helpers
	// ------------------------------------------------------------------------
	// use these for save games. call GetAllVariables to save, SetAllVariables
	// to restore.

	/**
	 * get all variables as separate maps for serialisation.
	 * splits by type for cleaner save game structures.
	 * @param OutFloats output map of float variables.
	 * @param OutStrings output map of string variables.
	 * @param OutBools output map of boolean variables.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
	void GetAllVariables(
		TMap<FString, float>& OutFloats,
		TMap<FString, FString>& OutStrings,
		TMap<FString, bool>& OutBools
	) const;

	/**
	 * set all variables from maps (for deserialisation).
	 * use this to restore variables from a save game.
	 * @param Floats map of float variables.
	 * @param Strings map of string variables.
	 * @param Bools map of boolean variables.
	 * @param bClearFirst if true, clears existing variables first.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
	void SetAllVariables(
		const TMap<FString, float>& Floats,
		const TMap<FString, FString>& Strings,
		const TMap<FString, bool>& Bools,
		bool bClearFirst = true
	);

	/**
	 * get a debug string listing all current variables.
	 * useful for debugging in the editor or output log.
	 * @return a formatted string of all variables and their values.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
	FString GetDebugString() const;

	/**
	 * get all variables as a single map of FYarnValue.
	 * useful for iteration and generic variable access.
	 * @param OutVariables output map of all variables.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Variables")
	void GetAllVariablesAsMap(TMap<FString, FYarnValue>& OutVariables) const;

protected:
	// ------------------------------------------------------------------------
	// internal storage
	// ------------------------------------------------------------------------

	/** the variables stored in memory (variable name -> value) */
	UPROPERTY()
	TMap<FString, FYarnValue> Variables;

	/** weak reference to the yarn project (for initial values) */
	UPROPERTY()
	TWeakObjectPtr<UYarnProject> YarnProject;

	/**
	 * validate that a variable name is correctly formatted.
	 * yarn variables must start with $ (e.g., $player_name).
	 * @param VariableName the variable name to check.
	 * @return true if the name is valid.
	 */
	bool ValidateVariableName(const FString& VariableName) const;

	/**
	 * called when a variable value changes.
	 * fires OnVariableChanged and typed listeners.
	 * override in subclasses to add custom notification behaviour.
	 * @param VariableName the name of the variable that changed.
	 * @param NewValue the new value.
	 */
	virtual void NotifyVariableChanged(const FString& VariableName, const FYarnValue& NewValue);

private:
	// ------------------------------------------------------------------------
	// listener management
	// ------------------------------------------------------------------------

	/** counter for generating unique listener ids */
	int32 NextListenerId = 1;

	/** internal struct for tracking string variable listeners */
	struct FStringListener
	{
		int32 Id;
		FString VariableName;
		FOnYarnStringVariableChanged Callback;
	};
	TArray<FStringListener> StringListeners;

	/** internal struct for tracking number variable listeners */
	struct FNumberListener
	{
		int32 Id;
		FString VariableName;
		FOnYarnNumberVariableChanged Callback;
	};
	TArray<FNumberListener> NumberListeners;

	/** internal struct for tracking bool variable listeners */
	struct FBoolListener
	{
		int32 Id;
		FString VariableName;
		FOnYarnBoolVariableChanged Callback;
	};
	TArray<FBoolListener> BoolListeners;

	// ------------------------------------------------------------------------
	// smart variable support
	// ------------------------------------------------------------------------

	/** registered game smart variable evaluators */
	UPROPERTY()
	TArray<TScriptInterface<IYarnGameSmartVariableEvaluator>> SmartVariableEvaluators;

	/** The smart variable evaluator for compiled expression nodes.
	 * Typically set by the dialogue runner to itself.
	 */
	IYarnSmartVariableEvaluator* CompiledSmartVariableEvaluator = nullptr;

	/**
	 * try to get value from game smart variable evaluators.
	 * checks each registered evaluator in order.
	 */
	bool TryGetSmartValue(const FString& VariableName, FYarnValue& OutValue);
};

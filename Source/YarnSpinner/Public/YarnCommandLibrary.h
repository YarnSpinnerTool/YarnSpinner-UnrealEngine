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
#include "UObject/Object.h"
#include "YarnSpinnerCore.h"
#include "YarnCommandLibrary.generated.h"

class UYarnDialogueRunner;

/**
 * A registered Yarn command handler.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnCommandRegistration
{
	GENERATED_BODY()

	/** The command name (as used in Yarn scripts) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FString CommandName;

	/** The object that handles this command */
	UPROPERTY()
	TWeakObjectPtr<UObject> HandlerObject;

	/** The function name to call on the handler object */
	UPROPERTY()
	FName FunctionName;

	/** Whether this command blocks dialogue until complete */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	bool bIsBlocking = true;

	FYarnCommandRegistration() = default;
	FYarnCommandRegistration(const FString& InCommandName, UObject* InHandler, FName InFunction)
		: CommandName(InCommandName)
		, HandlerObject(InHandler)
		, FunctionName(InFunction)
	{
	}
};

/**
 * Blueprint-callable delegate for command handlers.
 */
DECLARE_DYNAMIC_DELEGATE_OneParam(FYarnCommandHandlerBP, const TArray<FString>&, Parameters);

/**
 * Blueprint-callable delegate for function handlers.
 */
DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(FYarnValue, FYarnFunctionHandlerBP, const TArray<FYarnValue>&, Parameters);

/**
 * Library for registering Yarn commands and functions.
 *
 * This class provides static methods for registering command and function
 * handlers with a Yarn Dialogue Runner. Commands can be registered from
 * Blueprint or C++.
 */
UCLASS()
class YARNSPINNER_API UYarnCommandLibrary : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Register a command handler using a Blueprint delegate.
	 *
	 * @param DialogueRunner The dialogue runner to register with.
	 * @param CommandName The name of the command to handle (without the <<>>).
	 * @param Handler The delegate to call when the command is received.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Commands")
	static void RegisterCommandHandler(UYarnDialogueRunner* DialogueRunner, const FString& CommandName, FYarnCommandHandlerBP Handler);

	/**
	 * Unregister a command handler.
	 *
	 * @param DialogueRunner The dialogue runner to unregister from.
	 * @param CommandName The name of the command to unregister.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Commands")
	static void UnregisterCommandHandler(UYarnDialogueRunner* DialogueRunner, const FString& CommandName);

	/**
	 * Register a function handler using a Blueprint delegate.
	 *
	 * @param DialogueRunner The dialogue runner to register with.
	 * @param FunctionName The name of the function to handle.
	 * @param Handler The delegate to call when the function is invoked.
	 * @param ParameterCount The expected number of parameters.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Functions")
	static void RegisterFunctionHandler(UYarnDialogueRunner* DialogueRunner, const FString& FunctionName, FYarnFunctionHandlerBP Handler, int32 ParameterCount);

	/**
	 * Unregister a function handler.
	 *
	 * @param DialogueRunner The dialogue runner to unregister from.
	 * @param FunctionName The name of the function to unregister.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Functions")
	static void UnregisterFunctionHandler(UYarnDialogueRunner* DialogueRunner, const FString& FunctionName);

	/**
	 * Register all command handlers from an object that has UFUNCTION methods
	 * marked with the YarnCommand meta specifier.
	 *
	 * Example usage:
	 *   UFUNCTION(BlueprintCallable, meta = (YarnCommand = "my_command"))
	 *   void MyCommandHandler(const TArray<FString>& Parameters);
	 *
	 * @param DialogueRunner The dialogue runner to register with.
	 * @param HandlerObject The object containing command handler methods.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Commands")
	static void RegisterCommandsFromObject(UYarnDialogueRunner* DialogueRunner, UObject* HandlerObject);

	/**
	 * Unregister all commands from an object.
	 *
	 * @param DialogueRunner The dialogue runner to unregister from.
	 * @param HandlerObject The object whose commands should be unregistered.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Commands")
	static void UnregisterCommandsFromObject(UYarnDialogueRunner* DialogueRunner, UObject* HandlerObject);
};

/**
 * Macro to mark a UFUNCTION as a Yarn command handler.
 * Usage: UFUNCTION(meta = (YarnCommand = "command_name"))
 */
#define YARN_COMMAND(CommandName) meta = (YarnCommand = #CommandName)

/**
 * Macro to mark a UFUNCTION as a Yarn function handler.
 * Usage: UFUNCTION(meta = (YarnFunction = "function_name"))
 */
#define YARN_FUNCTION(FunctionName) meta = (YarnFunction = #FunctionName)

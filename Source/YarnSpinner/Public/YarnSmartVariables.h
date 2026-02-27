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
// YarnSmartVariables.h
// ============================================================================
//
// Smart variables are computed on-the-fly from compiled expression nodes rather
// than being stored directly. This file provides the evaluation VM and interface
// for implementing smart variable support.
//
// Smart variables are used by the saliency system to evaluate conditions from
// node headers. The VM evaluates compiled expressions that reference other
// variables and call functions to compute the final value.
//
// The key method is:
//   FYarnSmartVariableEvaluationVM::TryGetSmartVariable<T>(...)
//
// This evaluates a smart variable node from the program and returns the result.

#include "CoreMinimal.h"
#include "YarnSpinnerCore.h"
#include "YarnProgram.h"
#include "YarnSaliency.h"
#include "YarnVariableStorage.h"

class UYarnProject;

// ============================================================================
// IYarnSmartVariableEvaluator
// ============================================================================
//
// Objects implementing this interface can evaluate smart variables from
// compiled expression nodes in the program.

/**
 * Interface for evaluating smart variables from compiled expression nodes.
 *
 * Smart variables are variables whose values are computed by evaluating a
 * compiled expression node in the program, rather than being stored directly.
 * This is used by the saliency system to evaluate conditions.
 *
 * This is a pure C++ interface (not exposed to Blueprints) for internal
 * use by the dialogue runner and saliency system. For Blueprint-callable
 * smart variables that bind game state, see IYarnGameSmartVariableEvaluator
 * in YarnVariableStorage.h.
 */
class YARNSPINNER_API IYarnSmartVariableEvaluator
{
public:
	virtual ~IYarnSmartVariableEvaluator() = default;

	/**
	 * Evaluate the value of a smart variable.
	 *
	 * This method evaluates a smart variable named by the given name and
	 * returns the result as the specified type.
	 *
	 * @param Name The name of the smart variable to evaluate.
	 * @param OutResult On return, contains the evaluated value.
	 * @return true if the smart variable was evaluated successfully, false otherwise.
	 */
	virtual bool TryGetSmartVariableAsBool(const FString& Name, bool& OutResult) = 0;

	/**
	 * Evaluate the value of a smart variable as a float.
	 *
	 * @param Name The name of the smart variable to evaluate.
	 * @param OutResult On return, contains the evaluated value.
	 * @return true if the smart variable was evaluated successfully, false otherwise.
	 */
	virtual bool TryGetSmartVariableAsFloat(const FString& Name, float& OutResult) = 0;

	/**
	 * Evaluate the value of a smart variable as a string.
	 *
	 * @param Name The name of the smart variable to evaluate.
	 * @param OutResult On return, contains the evaluated value.
	 * @return true if the smart variable was evaluated successfully, false otherwise.
	 */
	virtual bool TryGetSmartVariableAsString(const FString& Name, FString& OutResult) = 0;

	/**
	 * Evaluate the value of a smart variable as an FYarnValue.
	 *
	 * @param Name The name of the smart variable to evaluate.
	 * @param OutResult On return, contains the evaluated value.
	 * @return true if the smart variable was evaluated successfully, false otherwise.
	 */
	virtual bool TryGetSmartVariable(const FString& Name, FYarnValue& OutResult) = 0;
};

// ============================================================================
// FYarnSmartVariableEvaluationVM
// ============================================================================
//
// A lightweight, static virtual machine designed for evaluating smart variables.
// This implements a subset of the instruction operations, specifically designed
// for evaluating smart variable implementation nodes.
//
/**
 * Delegate for calling functions during smart variable evaluation.
 * @param FunctionName The name of the function to call.
 * @param Parameters The parameters to pass.
 * @return The return value.
 */
DECLARE_DELEGATE_RetVal_TwoParams(FYarnValue, FOnSmartVariableCallFunction, const FString&, const TArray<FYarnValue>&);

/**
 * A lightweight, static virtual machine for evaluating smart variables.
 *
 * This class implements a subset of the Yarn Spinner instruction operations,
 * and is designed for evaluating smart variable implementation nodes.
 *
 * Supported instructions:
 * - PushString, PushFloat, PushBool: Push literals onto the stack
 * - Pop: Remove value from stack
 * - CallFunc: Call a function
 * - PushVariable: Push a variable value
 * - JumpIfFalse: Conditional jump
 * - Stop: End evaluation
 *
 */
class YARNSPINNER_API FYarnSmartVariableEvaluationVM
{
public:
	/**
	 * Evaluate a smart variable and return its value as the specified type.
	 *
	 * @param Name The name of the smart variable (also the node name to evaluate).
	 * @param Program The compiled program containing the smart variable node.
	 * @param VariableStorage The variable storage for fetching variable values.
	 * @param CallFunctionHandler Handler for calling functions during evaluation.
	 * @param OutResult On return, contains the computed value.
	 * @return true if the variable could be fetched and converted to the target type.
	 */
	static bool TryGetSmartVariableAsBool(
		const FString& Name,
		const FYarnProgram& Program,
		TScriptInterface<IYarnVariableStorage> VariableStorage,
		FOnSmartVariableCallFunction CallFunctionHandler,
		bool& OutResult);

	static bool TryGetSmartVariableAsFloat(
		const FString& Name,
		const FYarnProgram& Program,
		TScriptInterface<IYarnVariableStorage> VariableStorage,
		FOnSmartVariableCallFunction CallFunctionHandler,
		float& OutResult);

	static bool TryGetSmartVariableAsString(
		const FString& Name,
		const FYarnProgram& Program,
		TScriptInterface<IYarnVariableStorage> VariableStorage,
		FOnSmartVariableCallFunction CallFunctionHandler,
		FString& OutResult);

	/**
	 * Evaluate a smart variable and return its raw FYarnValue.
	 *
	 * @param Name The name of the smart variable (also the node name to evaluate).
	 * @param Program The compiled program containing the smart variable node.
	 * @param VariableStorage The variable storage for fetching variable values.
	 * @param CallFunctionHandler Handler for calling functions during evaluation.
	 * @param OutResult On return, contains the computed value.
	 * @return true if the variable was evaluated successfully.
	 */
	static bool TryGetSmartVariable(
		const FString& Name,
		const FYarnProgram& Program,
		TScriptInterface<IYarnVariableStorage> VariableStorage,
		FOnSmartVariableCallFunction CallFunctionHandler,
		FYarnValue& OutResult);

	/**
	 * Retrieve saliency options for a node group.
	 *
	 * This method iterates through all nodes in the program to find those
	 * that belong to the specified node group. It calculates the complexity
	 * score and counts the number of conditions that pass or fail for each
	 * node, then creates a saliency option for each qualifying node.
	 *
	 * @param NodeGroupName The name of the node group to search for.
	 * @param Program The compiled program.
	 * @param VariableStorage The variable storage for fetching variable values.
	 * @param CallFunctionHandler Handler for calling functions during evaluation.
	 * @param OutOptions On return, contains the saliency options for the node group.
	 * @return true if the node group was found, false otherwise.
	 */
	static bool GetSaliencyOptionsForNodeGroup(
		const FString& NodeGroupName,
		const FYarnProgram& Program,
		TScriptInterface<IYarnVariableStorage> VariableStorage,
		FOnSmartVariableCallFunction CallFunctionHandler,
		TArray<FYarnSaliencyCandidate>& OutOptions);

private:
	/**
	 * Internal method to evaluate a smart variable node.
	 *
	 * @param Name The name of the smart variable node.
	 * @param Program The compiled program.
	 * @param VariableStorage The variable storage.
	 * @param CallFunctionHandler The function call handler.
	 * @param OutResult On return, contains the computed value.
	 * @return true if evaluation succeeded.
	 */
	static bool EvaluateSmartVariableNode(
		const FString& Name,
		const FYarnProgram& Program,
		TScriptInterface<IYarnVariableStorage> VariableStorage,
		FOnSmartVariableCallFunction CallFunctionHandler,
		FYarnValue& OutResult);

	/**
	 * Evaluate a single instruction.
	 *
	 * @param Instruction The instruction to evaluate.
	 * @param Program The compiled program.
	 * @param VariableStorage The variable storage.
	 * @param CallFunctionHandler The function call handler.
	 * @param Stack The value stack.
	 * @param ProgramCounter The current program counter (may be modified).
	 * @return true to continue evaluation, false to stop.
	 */
	static bool EvaluateInstruction(
		const FYarnInstruction& Instruction,
		const FYarnProgram& Program,
		TScriptInterface<IYarnVariableStorage> VariableStorage,
		FOnSmartVariableCallFunction CallFunctionHandler,
		TArray<FYarnValue>& Stack,
		int32& ProgramCounter);

	/**
	 * Call a function and push the result onto the stack.
	 *
	 * @param Instruction The CallFunc instruction.
	 * @param CallFunctionHandler The function call handler.
	 * @param Stack The value stack.
	 */
	static void CallFunction(
		const FYarnInstruction& Instruction,
		FOnSmartVariableCallFunction CallFunctionHandler,
		TArray<FYarnValue>& Stack);
};

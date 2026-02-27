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

#include "YarnSmartVariables.h"
#include "YarnSpinnerModule.h"

// ============================================================================
// FYarnSmartVariableEvaluationVM Implementation
// ============================================================================

bool FYarnSmartVariableEvaluationVM::TryGetSmartVariableAsBool(
	const FString& Name,
	const FYarnProgram& Program,
	TScriptInterface<IYarnVariableStorage> VariableStorage,
	FOnSmartVariableCallFunction CallFunctionHandler,
	bool& OutResult)
{
	FYarnValue Value;
	if (EvaluateSmartVariableNode(Name, Program, VariableStorage, CallFunctionHandler, Value))
	{
		OutResult = Value.ConvertToBool();
		return true;
	}
	OutResult = false;
	return false;
}

bool FYarnSmartVariableEvaluationVM::TryGetSmartVariableAsFloat(
	const FString& Name,
	const FYarnProgram& Program,
	TScriptInterface<IYarnVariableStorage> VariableStorage,
	FOnSmartVariableCallFunction CallFunctionHandler,
	float& OutResult)
{
	FYarnValue Value;
	if (EvaluateSmartVariableNode(Name, Program, VariableStorage, CallFunctionHandler, Value))
	{
		OutResult = Value.ConvertToNumber();
		return true;
	}
	OutResult = 0.0f;
	return false;
}

bool FYarnSmartVariableEvaluationVM::TryGetSmartVariableAsString(
	const FString& Name,
	const FYarnProgram& Program,
	TScriptInterface<IYarnVariableStorage> VariableStorage,
	FOnSmartVariableCallFunction CallFunctionHandler,
	FString& OutResult)
{
	FYarnValue Value;
	if (EvaluateSmartVariableNode(Name, Program, VariableStorage, CallFunctionHandler, Value))
	{
		OutResult = Value.ConvertToString();
		return true;
	}
	OutResult = FString();
	return false;
}

bool FYarnSmartVariableEvaluationVM::TryGetSmartVariable(
	const FString& Name,
	const FYarnProgram& Program,
	TScriptInterface<IYarnVariableStorage> VariableStorage,
	FOnSmartVariableCallFunction CallFunctionHandler,
	FYarnValue& OutResult)
{
	return EvaluateSmartVariableNode(Name, Program, VariableStorage, CallFunctionHandler, OutResult);
}

bool FYarnSmartVariableEvaluationVM::GetSaliencyOptionsForNodeGroup(
	const FString& NodeGroupName,
	const FYarnProgram& Program,
	TScriptInterface<IYarnVariableStorage> VariableStorage,
	FOnSmartVariableCallFunction CallFunctionHandler,
	TArray<FYarnSaliencyCandidate>& OutOptions)
{
	// Check if the specified node group exists in the program
	if (!Program.HasNode(NodeGroupName))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Error getting available content for node group '%s': not a valid node group name"),
			*NodeGroupName);
		return false;
	}

	OutOptions.Empty();

	// Iterate through all nodes in the program
	for (const TPair<FString, FYarnNode>& NodePair : Program.Nodes)
	{
		const FYarnNode& Node = NodePair.Value;

		// Check if the current node belongs to the specified node group
		FString NodeGroup = Node.GetHeaderValue(TEXT("$Yarn.Internal.NodeGroup"));

		// Skip nodes that don't belong to this node group
		if (NodeGroup != NodeGroupName)
		{
			continue;
		}

		int32 PassingCount = 0;
		int32 FailingCount = 0;

		// Retrieve the saliency condition variables for the current node
		TArray<FString> Conditions = Node.GetContentSaliencyConditionVariables();

		// Iterate through each condition and evaluate its result
		for (const FString& Condition : Conditions)
		{
			bool bConditionResult = false;

			if (TryGetSmartVariableAsBool(Condition, Program, VariableStorage, CallFunctionHandler, bConditionResult))
			{
				// Successfully evaluated the condition's smart variable
				if (bConditionResult)
				{
					PassingCount++;
				}
				else
				{
					FailingCount++;
				}
			}
			else
			{
				// Smart variable doesn't exist or failed to evaluate - treat as error
				UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Failed to evaluate saliency condition smart variable '%s': variable not found in program"),
					*Condition);
				FailingCount++;
			}
		}

		// Create a new saliency option for the node
		FYarnSaliencyCandidate Candidate;
		Candidate.ContentID = Node.Name;
		Candidate.ContentType = EYarnSaliencyContentType::Node;
		Candidate.ComplexityScore = Node.GetContentSaliencyConditionComplexityScore();
		Candidate.PassingConditionCount = PassingCount;
		Candidate.FailingConditionCount = FailingCount;
		Candidate.bConditionPassed = (FailingCount == 0); // All conditions must pass

		OutOptions.Add(Candidate);
	}

	return true;
}

bool FYarnSmartVariableEvaluationVM::EvaluateSmartVariableNode(
	const FString& Name,
	const FYarnProgram& Program,
	TScriptInterface<IYarnVariableStorage> VariableStorage,
	FOnSmartVariableCallFunction CallFunctionHandler,
	FYarnValue& OutResult)
{
	// Validate inputs
	if (!VariableStorage.GetInterface())
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: VariableStorage cannot be null"));
		return false;
	}

	if (Name.IsEmpty())
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Variable name cannot be null or empty"));
		return false;
	}

	// Try to find the node with this name
	const FYarnNode* SmartVariableNode = Program.GetNode(Name);
	if (!SmartVariableNode)
	{
		// No node with this name exists - return indicating failure
		return false;
	}

	// Create the value stack
	TArray<FYarnValue> Stack;

	// Initialize program counter
	int32 ProgramCounter = 0;

	// Execute instructions until we hit a Stop or run out of instructions
	while (ProgramCounter < SmartVariableNode->Instructions.Num())
	{
		const FYarnInstruction& Instruction = SmartVariableNode->Instructions[ProgramCounter];

		bool bContinue = EvaluateInstruction(
			Instruction,
			Program,
			VariableStorage,
			CallFunctionHandler,
			Stack,
			ProgramCounter);

		if (!bContinue)
		{
			break;
		}
	}

	// Validate stack state
	if (Stack.Num() < 1)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Error evaluating smart variable '%s': stack did not contain a value after evaluation"),
			*Name);
		return false;
	}

	// Get the result from the stack
	OutResult = Stack.Pop();

	// Check for dangling values
	if (Stack.Num() > 0)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Error evaluating smart variable '%s': stack had %d dangling value(s)"),
			*Name, Stack.Num());
		return false;
	}

	return true;
}

bool FYarnSmartVariableEvaluationVM::EvaluateInstruction(
	const FYarnInstruction& Instruction,
	const FYarnProgram& Program,
	TScriptInterface<IYarnVariableStorage> VariableStorage,
	FOnSmartVariableCallFunction CallFunctionHandler,
	TArray<FYarnValue>& Stack,
	int32& ProgramCounter)
{
	switch (Instruction.Type)
	{
	case EYarnInstructionType::PushString:
		Stack.Add(FYarnValue(Instruction.StringOperand));
		break;

	case EYarnInstructionType::PushFloat:
		Stack.Add(FYarnValue(Instruction.FloatOperand));
		break;

	case EYarnInstructionType::PushBool:
		Stack.Add(FYarnValue(Instruction.BoolOperand));
		break;

	case EYarnInstructionType::Pop:
		if (Stack.Num() > 0)
		{
			Stack.Pop();
		}
		else
		{
			UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Stack underflow on Pop instruction"));
		}
		break;

	case EYarnInstructionType::CallFunction:
		CallFunction(Instruction, CallFunctionHandler, Stack);
		break;

	case EYarnInstructionType::PushVariable:
		{
			FString VariableName = Instruction.StringOperand;
			FYarnValue VariableValue;

			if (VariableStorage.GetInterface())
			{
				if (IYarnVariableStorage::Execute_TryGetValue(VariableStorage.GetObject(), VariableName, VariableValue))
				{
					Stack.Add(VariableValue);
				}
				else
				{
					// Also check initial values
					if (Program.TryGetInitialValue(VariableName, VariableValue))
					{
						Stack.Add(VariableValue);
					}
					else
					{
						UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Failed to fetch any value for '%s' when evaluating a smart variable"),
							*VariableName);
						return false;
					}
				}
			}
			else
			{
				// Try initial values from program
				if (Program.TryGetInitialValue(VariableName, VariableValue))
				{
					Stack.Add(VariableValue);
				}
				else
				{
					UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Failed to fetch any value for '%s' when evaluating a smart variable"),
						*VariableName);
					return false;
				}
			}
		}
		break;

	case EYarnInstructionType::JumpIfFalse:
		{
			// Peek without popping - the value stays on the stack
			if (Stack.Num() > 0)
			{
				bool bConditionValue = Stack.Last().ConvertToBool();
				if (!bConditionValue)
				{
					// Set the program counter directly to destination
					ProgramCounter = Instruction.IntOperand;
					return true; // Continue without incrementing PC
				}
				// Fall through to increment PC normally
			}
			else
			{
				UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Stack underflow on JumpIfFalse instruction"));
			}
		}
		// Fall through to increment PC
		ProgramCounter++;
		return true;

	case EYarnInstructionType::Stop:
		// Return false to indicate we should stop evaluating
		return false;

	default:
		UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Invalid opcode %d when evaluating a smart variable"),
			static_cast<int32>(Instruction.Type));
		return false;
	}

	// Increment program counter for most instructions
	ProgramCounter++;

	// Return true to continue evaluation
	return true;
}

void FYarnSmartVariableEvaluationVM::CallFunction(
	const FYarnInstruction& Instruction,
	FOnSmartVariableCallFunction CallFunctionHandler,
	TArray<FYarnValue>& Stack)
{
	FString FunctionName = Instruction.StringOperand;

	// Pop the parameter count from the stack
	if (Stack.Num() == 0)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Stack underflow when getting parameter count for function '%s'"),
			*FunctionName);
		Stack.Add(FYarnValue()); // Push null result
		return;
	}

	int32 ParamCount = FMath::RoundToInt(Stack.Pop().ConvertToNumber());

	// Pop parameters in reverse order
	TArray<FYarnValue> Parameters;
	Parameters.SetNum(ParamCount);

	for (int32 i = ParamCount - 1; i >= 0; i--)
	{
		if (Stack.Num() == 0)
		{
			UE_LOG(LogYarnSpinner, Error, TEXT("Smart Variable VM: Stack underflow when getting parameters for function '%s'"),
				*FunctionName);
			Parameters[i] = FYarnValue();
		}
		else
		{
			Parameters[i] = Stack.Pop();
		}
	}

	// Call the function via the handler
	if (CallFunctionHandler.IsBound())
	{
		FYarnValue Result = CallFunctionHandler.Execute(FunctionName, Parameters);
		// Only push a result if the function returned a value (not void)
		if (Result.Type != EYarnValueType::None)
		{
			Stack.Add(Result);
		}
	}
	else
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Smart Variable VM: Function '%s' called but no handler bound"),
			*FunctionName);
		Stack.Add(FYarnValue()); // Push null result
	}
}

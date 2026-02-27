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

#include "YarnVirtualMachine.h"
#include "YarnSmartVariables.h"
#include "YarnSpinnerModule.h"

FYarnVirtualMachine::FYarnVirtualMachine()
{
}

void FYarnVirtualMachine::SetProgram(const FYarnProgram& InProgram)
{
	Program = InProgram;

	// Only stop if we're actually running, to avoid spurious DialogueCompleteHandler calls
	if (ExecutionState != EYarnExecutionState::Stopped)
	{
		Stop();
	}
}

bool FYarnVirtualMachine::SetNode(const FString& NodeName)
{
	return SetNode(NodeName, true);
}

bool FYarnVirtualMachine::SetNode(const FString& NodeName, bool bClearState)
{
	const FYarnNode* Node = Program.GetNode(NodeName);
	if (!Node)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Cannot set node '%s' - node not found"), *NodeName);
		return false;
	}

	UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Running node '%s' (clearState=%s)"), *NodeName, bClearState ? TEXT("true") : TEXT("false"));

	// Clear state (stack, options) on non-detour jumps to prevent stack pollution
	if (bClearState)
	{
		ResetState();
	}

	CurrentNode = Node;
	CurrentNodeName = NodeName;
	InstructionPointer = 0;

	// Notify that the node has started
	if (NodeStartHandler.IsBound())
	{
		NodeStartHandler.Execute(NodeName);
	}

	// Notify of upcoming lines for pre-loading
	if (PrepareForLinesHandler.IsBound())
	{
		TArray<FString> LineIDs = Program.GetLineIDsForNode(NodeName);
		PrepareForLinesHandler.Execute(LineIDs);
	}

	return true;
}

void FYarnVirtualMachine::SetExecutionState(EYarnExecutionState NewState)
{
	ExecutionState = NewState;

	// Auto-reset state when transitioning to Stopped for a clean next run
	if (NewState == EYarnExecutionState::Stopped)
	{
		ResetState();
	}
}

void FYarnVirtualMachine::ResetState()
{
	Stack.Empty();
	CurrentOptions.Empty();
	ReturnStack.Empty();
	InstructionPointer = 0;
	CurrentNodeName.Empty();
}

void FYarnVirtualMachine::ReturnFromNode(const FYarnNode* Node)
{
	if (!Node)
	{
		return;
	}

	// Notify that the node has completed
	if (NodeCompleteHandler.IsBound())
	{
		NodeCompleteHandler.Execute(Node->Name);
	}

	// Update the tracking variable for this node (supports visited()/visited_count())
	FString TrackingVariable = Node->GetTrackingVariableName();
	if (!TrackingVariable.IsEmpty() && VariableStorage.GetInterface())
	{
		FYarnValue CurrentValue;

		if (IYarnVariableStorage::Execute_TryGetValue(VariableStorage.GetObject(), TrackingVariable, CurrentValue))
		{
			float NewCount = CurrentValue.ConvertToNumber() + 1.0f;
			IYarnVariableStorage::Execute_SetValue(VariableStorage.GetObject(), TrackingVariable, FYarnValue(NewCount));
			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Updated tracking variable '%s' = %f"), *TrackingVariable, NewCount);
		}
		else
		{
			UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Failed to get the tracking variable for node %s"), *Node->Name);
		}
	}
}

bool FYarnVirtualMachine::CheckCanContinue() const
{
	if (!CurrentNode)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Cannot continue - no node has been selected"));
		return false;
	}

	if (ExecutionState == EYarnExecutionState::WaitingOnOptionSelection)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Cannot continue - waiting for option selection. Call SetSelectedOption first."));
		return false;
	}

	if (!OptionsHandler.IsBound())
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Cannot continue - OptionsHandler has not been set"));
		return false;
	}

	if (!CallFunctionHandler.IsBound())
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Cannot continue - CallFunctionHandler has not been set"));
		return false;
	}

	return true;
}

bool FYarnVirtualMachine::Continue()
{
	UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Continue() called, state=%d"), static_cast<int32>(ExecutionState));

	// Re-entrancy guard
	if (bIsContinuing)
	{
		return true;
	}

	if (!CheckCanContinue())
	{
		return false;
	}

	bIsContinuing = true;

	// If Continue() is called from within a content handler callback,
	// set state to Running and return to avoid recursion.
	// Execution will resume when the original Continue() call unwinds.
	if (ExecutionState == EYarnExecutionState::DeliveringContent)
	{
		SetExecutionState(EYarnExecutionState::Running);
		bIsContinuing = false;
		return true;
	}

	UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Continuing node '%s' with %d instructions, IP=%d"),
		*CurrentNodeName, CurrentNode->Instructions.Num(), InstructionPointer);

	SetExecutionState(EYarnExecutionState::Running);

	// Execute instructions until the VM yields or stops
	while (CurrentNode && ExecutionState == EYarnExecutionState::Running)
	{
		if (InstructionPointer >= CurrentNode->Instructions.Num())
		{
			UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: IP %d out of bounds (have %d instructions) - this shouldn't happen"),
				InstructionPointer, CurrentNode->Instructions.Num());
			SetExecutionState(EYarnExecutionState::Error);
			bIsContinuing = false;
			return false;
		}

		const FYarnInstruction& Instruction = CurrentNode->Instructions[InstructionPointer];
		UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] Executing instruction type %d"), InstructionPointer, static_cast<int32>(Instruction.Type));

		RunInstruction(Instruction);

		// Always increment IP after each instruction.
		// Jump instructions set IP to (destination - 1) anticipating this increment.
		InstructionPointer++;

		// Check for end of node
		if (InstructionPointer >= CurrentNode->Instructions.Num())
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Reached end of node '%s' at IP=%d"), *CurrentNodeName, InstructionPointer);

			ReturnFromNode(CurrentNode);

			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Dialogue complete"));
			SetExecutionState(EYarnExecutionState::Stopped);
			if (DialogueCompleteHandler.IsBound())
			{
				DialogueCompleteHandler.Execute();
			}
			break;
		}

		// Loop exits on next iteration if ExecutionState != Running
	}

	bIsContinuing = false;
	UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Continue() finished, state=%d"), static_cast<int32>(ExecutionState));
	return true;
}

void FYarnVirtualMachine::Stop()
{
	SetExecutionState(EYarnExecutionState::Stopped);
	CurrentNode = nullptr;
	CurrentNodeName.Empty();

	if (DialogueCompleteHandler.IsBound())
	{
		DialogueCompleteHandler.Execute();
	}
}

void FYarnVirtualMachine::SetSelectedOption(int32 OptionIndex)
{
	if (ExecutionState != EYarnExecutionState::WaitingOnOptionSelection)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Yarn VM: SetSelectedOption called but VM is not waiting for selection"));
		return;
	}

	if (OptionIndex == YarnNoOptionSelected)
	{
		// No option selected - the compiled code uses JumpIfFalse to handle this
		Push(FYarnValue(false));
		UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: No option selected, pushed false"));
	}
	else
	{
		if (OptionIndex < 0 || OptionIndex >= CurrentOptions.Num())
		{
			UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Invalid option index %d (have %d options)"), OptionIndex, CurrentOptions.Num());
			return;
		}

		// Push destination and true flag; the compiled code uses PeekAndJump to read the destination
		int32 Destination = CurrentOptions[OptionIndex].OptionID;
		Push(FYarnValue(static_cast<float>(Destination)));
		Push(FYarnValue(true));
		UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Option %d selected, pushed destination %d and true"), OptionIndex, Destination);
	}

	CurrentOptions.Empty();
	SetExecutionState(EYarnExecutionState::WaitingForContinue);
}

void FYarnVirtualMachine::SignalContentComplete()
{
	if (ExecutionState != EYarnExecutionState::DeliveringContent)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Yarn VM: SignalContentComplete called when not delivering content (state=%d)"),
			static_cast<int32>(ExecutionState));
		return;
	}

	if (!bIsContinuing)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Yarn VM: SignalContentComplete called when not within Continue()"));
		return;
	}

	// Set to Running so execution resumes immediately within the current Continue() call
	SetExecutionState(EYarnExecutionState::Running);
}


void FYarnVirtualMachine::Push(const FYarnValue& Value)
{
	Stack.Add(Value);
}

FYarnValue FYarnVirtualMachine::Pop()
{
	if (Stack.Num() == 0)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Stack underflow on pop - halting execution"));
		ExecutionState = EYarnExecutionState::Error;
		return FYarnValue();
	}
	return Stack.Pop();
}

const FYarnValue& FYarnVirtualMachine::Peek() const
{
	// Use thread-local storage for the empty value to avoid shared state issues
	thread_local FYarnValue Empty;
	if (Stack.Num() == 0)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Stack underflow on peek"));
		Empty = FYarnValue();  // Reset to ensure clean state
		return Empty;
	}
	return Stack.Last();
}

TArray<FString> FYarnVirtualMachine::PopStrings(int32 Count)
{
	TArray<FString> Result;
	Result.SetNum(Count);

	// pop in reverse order
	for (int32 i = Count - 1; i >= 0; i--)
	{
		Result[i] = Pop().ConvertToString();
	}

	return Result;
}

bool FYarnVirtualMachine::RunInstruction(const FYarnInstruction& Instruction)
{
	switch (Instruction.Type)
	{
	case EYarnInstructionType::JumpTo:
		{
			int32 Destination = Instruction.IntOperand;
			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] JumpTo: destination=%d"), InstructionPointer, Destination);

			if (Destination < 0 || Destination >= CurrentNode->Instructions.Num())
			{
				UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: JumpTo destination %d out of bounds (0-%d) - halting"),
					Destination, CurrentNode->Instructions.Num() - 1);
				ExecutionState = EYarnExecutionState::Error;
				return false;
			}

			InstructionPointer = Destination - 1; // -1 because the loop increments after
		}
		return true;

	case EYarnInstructionType::PeekAndJump:
		{
			const FYarnValue& Value = Peek();
			int32 Destination = FMath::RoundToInt(Value.ConvertToNumber());

			if (Destination < 0 || Destination >= CurrentNode->Instructions.Num())
			{
				UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: PeekAndJump destination %d out of bounds (0-%d) - halting"),
					Destination, CurrentNode->Instructions.Num() - 1);
				ExecutionState = EYarnExecutionState::Error;
				return false;
			}

			InstructionPointer = Destination - 1;
		}
		return true;

	case EYarnInstructionType::RunLine:
		{
			FYarnLine Line;
			Line.LineID = Instruction.StringOperand;
			Line.Substitutions = PopStrings(Instruction.IntOperand);

			SetExecutionState(EYarnExecutionState::DeliveringContent);
			if (LineHandler.IsBound())
			{
				LineHandler.Execute(Line);
			}

			// If the handler didn't call SignalContentComplete(), wait for async completion
			if (ExecutionState == EYarnExecutionState::DeliveringContent)
			{
				SetExecutionState(EYarnExecutionState::WaitingForContinue);
				return false;
			}
		}
		return true;

	case EYarnInstructionType::RunCommand:
		{
			FString CommandText = Instruction.StringOperand;
			TArray<FString> Substitutions = PopStrings(Instruction.IntOperand);

			CommandText = ExpandSubstitutions(CommandText, Substitutions);

			FYarnCommand Command(CommandText);

			SetExecutionState(EYarnExecutionState::DeliveringContent);
			if (CommandHandler.IsBound())
			{
				CommandHandler.Execute(Command);
			}

			// If the handler didn't call SignalContentComplete(), wait for async completion
			if (ExecutionState == EYarnExecutionState::DeliveringContent)
			{
				SetExecutionState(EYarnExecutionState::WaitingForContinue);
				return false;
			}
		}
		return true;

	case EYarnInstructionType::AddOption:
		{
			FYarnOption Option;
			Option.Line.RawLine.LineID = Instruction.StringOperand;
			Option.Line.RawLine.Substitutions = PopStrings(Instruction.IntOperand2);

			if (Instruction.BoolOperand) // has condition
			{
				Option.bIsAvailable = Pop().ConvertToBool();
				UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] AddOption: '%s' (conditional, available=%s, dest=%d)"),
					InstructionPointer, *Instruction.StringOperand,
					Option.bIsAvailable ? TEXT("true") : TEXT("false"),
					Instruction.IntOperand);
			}
			else
			{
				Option.bIsAvailable = true;
				UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] AddOption: '%s' (unconditional, dest=%d)"),
					InstructionPointer, *Instruction.StringOperand, Instruction.IntOperand);
			}

			Option.OptionID = Instruction.IntOperand; // store destination in OptionID
			CurrentOptions.Add(Option);
		}
		return true;

	case EYarnInstructionType::ShowOptions:
		{
			if (CurrentOptions.Num() == 0)
			{
				UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: ShowOptions with no options - stopping dialogue"));
				SetExecutionState(EYarnExecutionState::Stopped);
				if (DialogueCompleteHandler.IsBound())
				{
					DialogueCompleteHandler.Execute();
				}
				return false;
			}

			FYarnOptionSet OptionSet;
			OptionSet.Options = CurrentOptions;

			SetExecutionState(EYarnExecutionState::WaitingOnOptionSelection);

			if (OptionsHandler.IsBound())
			{
				OptionsHandler.Execute(OptionSet);
			}

			// If the handler called SetSelectedOption() synchronously, resume immediately
			if (ExecutionState == EYarnExecutionState::WaitingForContinue)
			{
				SetExecutionState(EYarnExecutionState::Running);
				return true;
			}

		}
		return false;

	case EYarnInstructionType::PushString:
		Push(FYarnValue(Instruction.StringOperand));
		return true;

	case EYarnInstructionType::PushFloat:
		Push(FYarnValue(Instruction.FloatOperand));
		return true;

	case EYarnInstructionType::PushBool:
		UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] PushBool: %s"), InstructionPointer, Instruction.BoolOperand ? TEXT("true") : TEXT("false"));
		Push(FYarnValue(Instruction.BoolOperand));
		return true;

	case EYarnInstructionType::JumpIfFalse:
		{
			// Peek (not pop) - the value stays on the stack
			const FYarnValue& Value = Peek();
			bool bConditionValue = Value.ConvertToBool();
			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] JumpIfFalse: condition=%s, jumping=%s (to %d)"),
				InstructionPointer,
				bConditionValue ? TEXT("true") : TEXT("false"),
				bConditionValue ? TEXT("NO") : TEXT("YES"),
				Instruction.IntOperand);
			if (!bConditionValue)
			{
				int32 Destination = Instruction.IntOperand;
				if (Destination < 0 || Destination >= CurrentNode->Instructions.Num())
				{
					UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: JumpIfFalse destination %d out of bounds (0-%d) - halting"),
						Destination, CurrentNode->Instructions.Num() - 1);
					ExecutionState = EYarnExecutionState::Error;
					return false;
				}
				InstructionPointer = Destination - 1;
			}
		}
		return true;

	case EYarnInstructionType::Pop:
		Pop();
		return true;

	case EYarnInstructionType::CallFunction:
		{
			FString FunctionName = Instruction.StringOperand;

			// The compiler pushes the parameter count before the CallFunc instruction
			int32 ParamCount = FMath::RoundToInt(Pop().ConvertToNumber());
			TArray<FYarnValue> Parameters;
			Parameters.SetNum(ParamCount);
			for (int32 i = ParamCount - 1; i >= 0; i--)
			{
				Parameters[i] = Pop();
			}

			// Build parameter string for logging
			FString ParamsStr;
			for (int32 i = 0; i < Parameters.Num(); i++)
			{
				if (i > 0) ParamsStr += TEXT(", ");
				switch (Parameters[i].Type)
				{
				case EYarnValueType::Bool:
					ParamsStr += Parameters[i].GetBoolValue() ? TEXT("true") : TEXT("false");
					break;
				case EYarnValueType::Number:
					ParamsStr += FString::Printf(TEXT("%f"), Parameters[i].GetNumberValue());
					break;
				case EYarnValueType::String:
					ParamsStr += FString::Printf(TEXT("\"%s\""), *Parameters[i].GetStringValue());
					break;
				default:
					ParamsStr += TEXT("<null>");
					break;
				}
			}

			if (CallFunctionHandler.IsBound())
			{
				FYarnValue Result = CallFunctionHandler.Execute(FunctionName, Parameters);

				// Only push the return value for non-void functions. Void functions
				// return FYarnValue() with Type == None, and pushing that would corrupt
				// the stack since the compiler doesn't emit a Pop for void calls.
				bool bFunctionReturnsValue = (Result.Type != EYarnValueType::None);

				if (bFunctionReturnsValue)
				{
					FString ResultStr;
					switch (Result.Type)
					{
					case EYarnValueType::Bool:
						ResultStr = Result.GetBoolValue() ? TEXT("true") : TEXT("false");
						break;
					case EYarnValueType::Number:
						ResultStr = FString::Printf(TEXT("%f"), Result.GetNumberValue());
						break;
					case EYarnValueType::String:
						ResultStr = FString::Printf(TEXT("\"%s\""), *Result.GetStringValue());
						break;
					default:
						ResultStr = TEXT("<unknown>");
						break;
					}

					UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] CallFunction: %s(%s) => %s"),
						InstructionPointer, *FunctionName, *ParamsStr, *ResultStr);

					Push(Result);
				}
				else
				{
					UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] CallFunction: %s(%s) => <void>"),
						InstructionPointer, *FunctionName, *ParamsStr);
				}
			}
			else
			{
				UE_LOG(LogYarnSpinner, Warning, TEXT("Yarn VM: Function '%s' called but no handler bound"), *FunctionName);
			}
		}
		return true;

	case EYarnInstructionType::PushVariable:
		{
			FString VariableName = Instruction.StringOperand;
			FYarnValue Value;
			bool bFoundInStorage = false;
			bool bFoundInProgram = false;

			if (VariableStorage.GetInterface())
			{
				bFoundInStorage = IYarnVariableStorage::Execute_TryGetValue(VariableStorage.GetObject(), VariableName, Value);
			}

			if (!bFoundInStorage)
			{
				bFoundInProgram = Program.TryGetInitialValue(VariableName, Value);
			}

			if (!bFoundInStorage && !bFoundInProgram)
			{
				UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Variable '%s' not found in storage or initial values - halting"),
					*VariableName);
				SetExecutionState(EYarnExecutionState::Error);
				return false;
			}

			// Log variable access
			FString ValueStr;
			switch (Value.Type)
			{
			case EYarnValueType::Bool:
				ValueStr = Value.GetBoolValue() ? TEXT("true") : TEXT("false");
				break;
			case EYarnValueType::Number:
				ValueStr = FString::Printf(TEXT("%f"), Value.GetNumberValue());
				break;
			case EYarnValueType::String:
				ValueStr = FString::Printf(TEXT("\"%s\""), *Value.GetStringValue());
				break;
			default:
				ValueStr = TEXT("<undefined>");
				break;
			}

			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] PushVariable: '%s' = %s (source: %s)"),
				InstructionPointer,
				*VariableName,
				*ValueStr,
				bFoundInStorage ? TEXT("VariableStorage") : TEXT("InitialValues"));

			Push(Value);
		}
		return true;

	case EYarnInstructionType::StoreVariable:
		{
			FString VariableName = Instruction.StringOperand;
			FYarnValue Value = Peek(); // peek, don't pop
			FString ValueStr;
			switch (Value.Type)
			{
			case EYarnValueType::Bool:
				ValueStr = Value.GetBoolValue() ? TEXT("true") : TEXT("false");
				break;
			case EYarnValueType::Number:
				ValueStr = FString::Printf(TEXT("%f"), Value.GetNumberValue());
				break;
			case EYarnValueType::String:
				ValueStr = FString::Printf(TEXT("\"%s\""), *Value.GetStringValue());
				break;
			default:
				ValueStr = TEXT("<undefined>");
				break;
			}

			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] StoreVariable: '%s' = %s"), InstructionPointer, *VariableName, *ValueStr);

			if (VariableStorage.GetInterface())
			{
				IYarnVariableStorage::Execute_SetValue(VariableStorage.GetObject(), VariableName, Value);
			}
			else
			{
				UE_LOG(LogYarnSpinner, Warning, TEXT("Yarn VM: Cannot store variable '%s' - no variable storage set"), *VariableName);
			}
		}
		return true;

	case EYarnInstructionType::Stop:
		{
			ReturnFromNode(CurrentNode);

			// Unwind the entire call stack so all deferred nodes get their tracking variables updated
			while (ReturnStack.Num() > 0)
			{
				TPair<FString, int32> ReturnAddress = ReturnStack.Pop();
				const FYarnNode* ReturnNode = Program.GetNode(ReturnAddress.Key);
				if (ReturnNode)
				{
					ReturnFromNode(ReturnNode);
				}
			}

			// Fire handler before setting Stopped, since Stopped triggers ResetState()
			if (DialogueCompleteHandler.IsBound())
			{
				DialogueCompleteHandler.Execute();
			}
			SetExecutionState(EYarnExecutionState::Stopped);
		}
		return false;

	case EYarnInstructionType::RunNode:
		{
			FString NodeName = Instruction.StringOperand;

			ReturnFromNode(CurrentNode);

			// Unwind the entire call stack - this is a hard jump that abandons pending returns
			while (ReturnStack.Num() > 0)
			{
				TPair<FString, int32> ReturnAddress = ReturnStack.Pop();
				const FYarnNode* ReturnNode = Program.GetNode(ReturnAddress.Key);
				if (ReturnNode)
				{
					ReturnFromNode(ReturnNode);
				}
			}

			if (!SetNode(NodeName, true))
			{
				SetExecutionState(EYarnExecutionState::Error);
				return false;
			}
			InstructionPointer = -1; // will be incremented to 0
		}
		return true;

	case EYarnInstructionType::PeekAndRunNode:
		{
			FYarnValue Value = Peek();
			FString NodeName = Value.ConvertToString();

			ReturnFromNode(CurrentNode);

			// Unwind the entire call stack
			while (ReturnStack.Num() > 0)
			{
				TPair<FString, int32> ReturnAddress = ReturnStack.Pop();
				const FYarnNode* ReturnNode = Program.GetNode(ReturnAddress.Key);
				if (ReturnNode)
				{
					ReturnFromNode(ReturnNode);
				}
			}

			if (!SetNode(NodeName, true))
			{
				SetExecutionState(EYarnExecutionState::Error);
				return false;
			}
			InstructionPointer = -1;
		}
		return true;

	case EYarnInstructionType::DetourToNode:
		{
			FString NodeName = Instruction.StringOperand;
			if (ReturnStack.Num() >= MaxCallStackDepth)
			{
				UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: DetourToNode exceeded maximum call stack depth (%d) - possible infinite recursion. Halting."),
					MaxCallStackDepth);
				ExecutionState = EYarnExecutionState::Error;
				return false;
			}

			// Don't call ReturnFromNode - the source node isn't "complete" until Return
			ReturnStack.Push(TPair<FString, int32>(CurrentNodeName, InstructionPointer + 1));

			// Don't clear state - detour preserves the stack
			if (!SetNode(NodeName, false))
			{
				SetExecutionState(EYarnExecutionState::Error);
				return false;
			}
			InstructionPointer = -1;
		}
		return true;

	case EYarnInstructionType::PeekAndDetourToNode:
		{
			FYarnValue Value = Peek();
			FString NodeName = Value.ConvertToString();

			if (ReturnStack.Num() >= MaxCallStackDepth)
			{
				UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: PeekAndDetourToNode exceeded maximum call stack depth (%d) - possible infinite recursion. Halting."),
					MaxCallStackDepth);
				ExecutionState = EYarnExecutionState::Error;
				return false;
			}

			ReturnStack.Push(TPair<FString, int32>(CurrentNodeName, InstructionPointer + 1));

			if (!SetNode(NodeName, false))
			{
				SetExecutionState(EYarnExecutionState::Error);
				return false;
			}
			InstructionPointer = -1;
		}
		return true;

	case EYarnInstructionType::Return:
		{
			if (ReturnStack.Num() == 0)
			{
				// No return address - treat as Stop (end of dialogue)
				// This happens when a node is reached via <<jump>> instead of <<detour>>
				UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: Return with no return address - treating as Stop"));

				ReturnFromNode(CurrentNode);

				// Fire handler before setting Stopped, since Stopped triggers ResetState()
				if (DialogueCompleteHandler.IsBound())
				{
					DialogueCompleteHandler.Execute();
				}
				SetExecutionState(EYarnExecutionState::Stopped);
				return false;
			}

			ReturnFromNode(CurrentNode);

			TPair<FString, int32> ReturnAddress = ReturnStack.Pop();
			if (!SetNode(ReturnAddress.Key, false))
			{
				SetExecutionState(EYarnExecutionState::Error);
				return false;
			}
			InstructionPointer = ReturnAddress.Value - 1;
		}
		return true;

	case EYarnInstructionType::AddSaliencyCandidate:
		{
			bool bCondition = Pop().ConvertToBool();

			FYarnSaliencyCandidate Candidate;
			Candidate.ContentID = Instruction.StringOperand;
			Candidate.bConditionPassed = bCondition;
			Candidate.ComplexityScore = Instruction.IntOperand2;
			Candidate.Destination = Instruction.IntOperand;
			Candidate.ContentType = EYarnSaliencyContentType::Line;

			Candidate.PassingConditionCount = bCondition ? 1 : 0;
			Candidate.FailingConditionCount = bCondition ? 0 : 1;

			SaliencyCandidates.Add(Candidate);

			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] AddSaliencyCandidate: '%s' (when: %s, complexity=%d, dest=%d)"),
				InstructionPointer, *Candidate.ContentID,
				bCondition ? TEXT("PASSED") : TEXT("FAILED"),
				Candidate.ComplexityScore, Candidate.Destination);
		}
		return true;

	case EYarnInstructionType::AddSaliencyCandidateFromNode:
		{
			FString NodeName = Instruction.StringOperand;
			const FYarnNode* CandidateNode = Program.GetNode(NodeName);

			FYarnSaliencyCandidate Candidate;
			Candidate.ContentID = NodeName;
			Candidate.ContentType = EYarnSaliencyContentType::Node;
			Candidate.Destination = Instruction.IntOperand;

			if (CandidateNode)
			{
				// Evaluate smart variable conditions from node headers
				TArray<FString> ConditionVariables = CandidateNode->GetContentSaliencyConditionVariables();

				if (ConditionVariables.Num() > 0 && !VariableStorage.GetInterface())
				{
					UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Failed to add saliency candidate from node '%s': VariableStorage is not set"), *NodeName);
					// Treat as failed - node won't be selectable
					Candidate.bConditionPassed = false;
					Candidate.FailingConditionCount = ConditionVariables.Num();
					Candidate.ComplexityScore = CandidateNode->GetContentSaliencyConditionComplexityScore();
					SaliencyCandidates.Add(Candidate);
					return true;
				}

				int32 PassedCount = 0;
				int32 FailedCount = 0;

				for (const FString& VariableName : ConditionVariables)
				{
					bool bConditionPassed = false;

					// Try to evaluate as a smart variable (compiled expression node) first
					if (FYarnSmartVariableEvaluationVM::TryGetSmartVariableAsBool(
						VariableName,
						Program,
						VariableStorage,
						CallFunctionHandler,
						bConditionPassed))
					{
						// Evaluated as smart variable
					}
					else
					{
						// Fall back to regular variable storage
						FYarnValue ConditionValue;
						if (VariableStorage.GetInterface() &&
							IYarnVariableStorage::Execute_TryGetValue(VariableStorage.GetObject(), VariableName, ConditionValue))
						{
							bConditionPassed = ConditionValue.ConvertToBool();
						}
						else
						{
							UE_LOG(LogYarnSpinner, Warning, TEXT("Yarn VM: Failed to evaluate saliency condition '%s' for node '%s'"),
								*VariableName, *NodeName);
							bConditionPassed = false;
						}
					}

					if (bConditionPassed)
					{
						PassedCount++;
					}
					else
					{
						FailedCount++;
					}
				}

				Candidate.PassingConditionCount = PassedCount;
				Candidate.FailingConditionCount = FailedCount;
				Candidate.bConditionPassed = (FailedCount == 0);
				Candidate.ComplexityScore = CandidateNode->GetContentSaliencyConditionComplexityScore();

				UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] AddSaliencyCandidateFromNode: '%s' (passed=%d, failed=%d, complexity=%d, dest=%d)"),
					InstructionPointer, *NodeName, PassedCount, FailedCount,
					Candidate.ComplexityScore, Candidate.Destination);
			}
			else
			{
				Candidate.bConditionPassed = false;
				Candidate.FailingConditionCount = 1;
				UE_LOG(LogYarnSpinner, Warning, TEXT("Yarn VM: [%d] AddSaliencyCandidateFromNode: Node '%s' not found"),
					InstructionPointer, *NodeName);
			}

			SaliencyCandidates.Add(Candidate);
		}
		return true;

	case EYarnInstructionType::SelectSaliencyCandidate:
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM: [%d] SelectSaliencyCandidate: evaluating %d candidates"),
				InstructionPointer, SaliencyCandidates.Num());

			for (const FYarnSaliencyCandidate& Candidate : SaliencyCandidates)
			{
				UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM:   - Candidate: '%s' (passed=%s, complexity=%d)"),
					*Candidate.ContentID,
					Candidate.bConditionPassed ? TEXT("true") : TEXT("false"),
					Candidate.ComplexityScore);
			}

			// Keep a copy for validation - strategy receives all candidates (not just passed ones)
			TArray<FYarnSaliencyCandidate> OriginalCandidates = SaliencyCandidates;
			FYarnSaliencyCandidate Selected;
			bool bSelected = false;

			if (SelectSaliencyCandidateHandler.IsBound())
			{
				bSelected = SelectSaliencyCandidateHandler.Execute(SaliencyCandidates, Selected);
			}
			else
			{
				// Fallback: highest complexity among passed candidates, random among ties
				TArray<FYarnSaliencyCandidate> PassedCandidates;
				for (const FYarnSaliencyCandidate& Candidate : SaliencyCandidates)
				{
					if (Candidate.bConditionPassed)
					{
						PassedCandidates.Add(Candidate);
					}
				}

				if (PassedCandidates.Num() > 0)
				{
					int32 BestScore = PassedCandidates[0].ComplexityScore;
					TArray<int32> BestIndices;
					BestIndices.Add(0);

					for (int32 i = 1; i < PassedCandidates.Num(); i++)
					{
						if (PassedCandidates[i].ComplexityScore > BestScore)
						{
							BestScore = PassedCandidates[i].ComplexityScore;
							BestIndices.Empty();
							BestIndices.Add(i);
						}
						else if (PassedCandidates[i].ComplexityScore == BestScore)
						{
							BestIndices.Add(i);
						}
					}

					int32 SelectedIndex = BestIndices[FMath::RandRange(0, BestIndices.Num() - 1)];
					Selected = PassedCandidates[SelectedIndex];
					bSelected = true;

					UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM:   => Fallback selection: '%s' (from %d candidates with best score %d)"),
						*Selected.ContentID, BestIndices.Num(), BestScore);
				}
			}

			SaliencyCandidates.Empty();

			if (bSelected)
			{
				// Validate that the selected candidate was in the original list
				bool bSelectedWasValid = false;
				for (const FYarnSaliencyCandidate& Candidate : OriginalCandidates)
				{
					if (Candidate.ContentID == Selected.ContentID)
					{
						bSelectedWasValid = true;
						break;
					}
				}

				if (!bSelectedWasValid)
				{
					UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Saliency strategy returned candidate '%s' that was not in the candidate list"),
						*Selected.ContentID);
					SetExecutionState(EYarnExecutionState::Error);
					return false;
				}

				UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM:   => Selected: '%s' (dest=%d)"),
					*Selected.ContentID, Selected.Destination);

				// Notify strategy so it can track view history
				if (ContentWasSelectedHandler.IsBound())
				{
					ContentWasSelectedHandler.Execute(Selected);
				}

				Push(FYarnValue(static_cast<float>(Selected.Destination)));
				Push(FYarnValue(true));
			}
			else
			{
				UE_LOG(LogYarnSpinner, Log, TEXT("Yarn VM:   => No selection, pushing false"));
				Push(FYarnValue(false));
			}
		}
		return true;

	default:
		UE_LOG(LogYarnSpinner, Error, TEXT("Yarn VM: Unknown instruction type %d - halting execution"), static_cast<int32>(Instruction.Type));
		ExecutionState = EYarnExecutionState::Error;
		return false;  // Return false to halt VM on unknown instruction
	}
}

FString FYarnVirtualMachine::ExpandSubstitutions(const FString& TemplateString, const TArray<FString>& Substitutions)
{
	// Process in reverse index order so {10} isn't partially matched when replacing {1}
	FString Result = TemplateString;
	for (int32 i = Substitutions.Num() - 1; i >= 0; i--)
	{
		FString Marker = FString::Printf(TEXT("{%d}"), i);
		Result = Result.Replace(*Marker, *Substitutions[i]);
	}
	return Result;
}

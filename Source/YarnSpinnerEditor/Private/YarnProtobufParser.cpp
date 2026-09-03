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

#include "YarnProtobufParser.h"
#include "YarnProgram.h"
#include "YarnSpinnerModule.h"

FYarnProtobufParser::FYarnProtobufParser(const TArray<uint8>& InData)
	: Data(InData)
	, Position(0)
{
}

uint64 FYarnProtobufParser::ReadVarint()
{
	uint64 Result = 0;
	int32 Shift = 0;

	while (Position < Data.Num())
	{
		uint8 Byte = Data[Position++];
		Result |= static_cast<uint64>(Byte & 0x7F) << Shift;
		if ((Byte & 0x80) == 0)
		{
			break;
		}
		Shift += 7;
		// Protobuf spec limits varints to 10 bytes (70 bits) max
		if (Shift > 63)
		{
			break;
		}
	}

	return Result;
}

uint32 FYarnProtobufParser::ReadFixed32()
{
	if (Position + 4 > Data.Num())
	{
		return 0;
	}

	uint32 Result = Data[Position] |
		(static_cast<uint32>(Data[Position + 1]) << 8) |
		(static_cast<uint32>(Data[Position + 2]) << 16) |
		(static_cast<uint32>(Data[Position + 3]) << 24);
	Position += 4;
	return Result;
}

FString FYarnProtobufParser::ReadString()
{
	uint64 Length = ReadVarint();

	// Bounds check - ensure we have enough data and position is valid
	if (Position >= Data.Num() || Length > static_cast<uint64>(Data.Num() - Position))
	{
		Position = Data.Num(); // Move to end to stop parsing
		return FString();
	}

	if (Length == 0)
	{
		return FString();
	}

	FString Result;
	// Convert UTF-8 to FString
	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(&Data[Position]), Length);
	Result = FString(Converter.Length(), Converter.Get());
	Position += Length;
	return Result;
}

TArray<uint8> FYarnProtobufParser::ReadBytes()
{
	uint64 Length = ReadVarint();
	TArray<uint8> Result;

	// Bounds check - ensure we have enough data and position is valid
	if (Position >= Data.Num() || Length > static_cast<uint64>(Data.Num() - Position))
	{
		Position = Data.Num(); // Move to end to stop parsing
		return Result;
	}

	if (Length == 0)
	{
		return Result;
	}

	Result.Append(&Data[Position], Length);
	Position += Length;
	return Result;
}

void FYarnProtobufParser::SkipField(int32 WireType)
{
	switch (WireType)
	{
	case 0: // varint
		ReadVarint();
		break;
	case 1: // 64-bit
		if (Position + 8 <= Data.Num())
		{
			Position += 8;
		}
		else
		{
			Position = Data.Num();
		}
		break;
	case 2: // length-delimited
		{
			uint64 Length = ReadVarint();
			if (Position < Data.Num() && Length <= static_cast<uint64>(Data.Num() - Position))
			{
				Position += Length;
			}
			else
			{
				Position = Data.Num();
			}
		}
		break;
	case 5: // 32-bit
		if (Position + 4 <= Data.Num())
		{
			Position += 4;
		}
		else
		{
			Position = Data.Num();
		}
		break;
	default:
		break;
	}
}

bool FYarnProtobufParser::ParseProgram(FYarnProgram& OutProgram, FString& OutError)
{
	while (Position < Data.Num())
	{
		uint64 Tag = ReadVarint();
		int32 FieldNumber = Tag >> 3;
		int32 WireType = Tag & 0x7;

		switch (FieldNumber)
		{
		case 1: // name
			OutProgram.Name = ReadString();
			break;

		case 2: // nodes (map<string, Node>)
			{
				// Map entry is a submessage
				TArray<uint8> EntryData = ReadBytes();
				FYarnProtobufParser EntryParser(EntryData);

				FString NodeName;
				FYarnNode Node;

				while (EntryParser.Position < EntryData.Num())
				{
					uint64 EntryTag = EntryParser.ReadVarint();
					int32 EntryField = EntryTag >> 3;
					int32 EntryWire = EntryTag & 0x7;

					if (EntryField == 1) // key (string)
					{
						NodeName = EntryParser.ReadString();
					}
					else if (EntryField == 2) // value (Node)
					{
						TArray<uint8> NodeData = EntryParser.ReadBytes();
						FYarnProtobufParser NodeParser(NodeData);
						NodeParser.ParseNode(Node);
					}
					else
					{
						EntryParser.SkipField(EntryWire);
					}
				}

				if (!NodeName.IsEmpty())
				{
					Node.Name = NodeName;

					if (const FYarnNode* Existing = OutProgram.Nodes.Find(NodeName))
					{
						if (Existing->Name != NodeName)
						{
							UE_LOG(LogYarnSpinner, Error, TEXT("yarn project importer: node names '%s' and '%s' differ only by case - this Unreal Engine map keys nodes case-insensitively, so one will silently overwrite the other. Rename one of them."),
								*Existing->Name, *NodeName);
						}
					}

					OutProgram.Nodes.Add(NodeName, Node);
				}
			}
			break;

		case 3: // initial_values (map<string, Operand>)
			{
				TArray<uint8> EntryData = ReadBytes();
				FYarnProtobufParser EntryParser(EntryData);

				FString VariableName;
				FYarnValue Value;

				while (EntryParser.Position < EntryData.Num())
				{
					uint64 EntryTag = EntryParser.ReadVarint();
					int32 EntryField = EntryTag >> 3;
					int32 EntryWire = EntryTag & 0x7;

					if (EntryField == 1)
					{
						VariableName = EntryParser.ReadString();
					}
					else if (EntryField == 2)
					{
						TArray<uint8> OperandData = EntryParser.ReadBytes();
						FYarnProtobufParser OperandParser(OperandData);
						OperandParser.ParseOperand(Value);
					}
					else
					{
						EntryParser.SkipField(EntryWire);
					}
				}

				if (!VariableName.IsEmpty())
				{
					const FString* ExistingKey = nullptr;
					for (const auto& ExistingPair : OutProgram.InitialValues)
					{
						if (ExistingPair.Key.Equals(VariableName, ESearchCase::IgnoreCase))
						{
							ExistingKey = &ExistingPair.Key;
							break;
						}
					}

					if (ExistingKey && *ExistingKey != VariableName)
					{
						UE_LOG(LogYarnSpinner, Error, TEXT("yarn project importer: initial values '%s' and '%s' differ only by case - this Unreal Engine map keys variables case-insensitively, so one will silently overwrite the other. Rename one of them."),
							**ExistingKey, *VariableName);
					}

					OutProgram.InitialValues.Add(VariableName, Value);
				}
			}
			break;

		case 4: // language_version
			OutProgram.LanguageVersion = static_cast<int32>(ReadVarint());
			break;

		default:
			SkipField(WireType);
			break;
		}
	}

	return true;
}

bool FYarnProtobufParser::ParseNode(FYarnNode& OutNode)
{
	while (Position < Data.Num())
	{
		uint64 Tag = ReadVarint();
		int32 FieldNumber = Tag >> 3;
		int32 WireType = Tag & 0x7;

		switch (FieldNumber)
		{
		case 1: // name
			OutNode.Name = ReadString();
			break;

		case 6: // headers
			{
				TArray<uint8> HeaderData = ReadBytes();
				FYarnProtobufParser HeaderParser(HeaderData);

				FYarnHeader Header;
				while (HeaderParser.Position < HeaderData.Num())
				{
					uint64 HeaderTag = HeaderParser.ReadVarint();
					int32 HeaderField = HeaderTag >> 3;

					if (HeaderField == 1)
					{
						Header.Key = HeaderParser.ReadString();
					}
					else if (HeaderField == 2)
					{
						Header.Value = HeaderParser.ReadString();
					}
					else
					{
						HeaderParser.SkipField(HeaderTag & 0x7);
					}
				}

				OutNode.Headers.Add(Header);
			}
			break;

		case 7: // instructions
			{
				TArray<uint8> InstructionData = ReadBytes();
				FYarnProtobufParser InstructionParser(InstructionData);

				FYarnInstruction Instruction;
				InstructionParser.ParseInstruction(Instruction);
				// Always keep the instruction, even if its type is unrecognised (Invalid).
				// Jump destinations are instruction indices, so dropping an entry would
				// shift every later destination in the node. The VM halts if an Invalid
				// instruction is executed, matching the C# runtime's behaviour.
				if (Instruction.Type == EYarnInstructionType::Invalid)
				{
					UE_LOG(LogYarnSpinner, Warning, TEXT("Node '%s': instruction %d has an unrecognised type and will halt the VM if executed"), *OutNode.Name, OutNode.Instructions.Num());
				}
				OutNode.Instructions.Add(Instruction);
			}
			break;

		default:
			SkipField(WireType);
			break;
		}
	}

	return true;
}

bool FYarnProtobufParser::ParseInstruction(FYarnInstruction& OutInstruction)
{
	// The instruction message uses oneof, so we need to parse the submessage
	while (Position < Data.Num())
	{
		uint64 Tag = ReadVarint();
		int32 FieldNumber = Tag >> 3;
		int32 WireType = Tag & 0x7;

		// Each field number corresponds to an instruction type
		switch (FieldNumber)
		{
		case 1: // jumpTo
			{
				OutInstruction.Type = EYarnInstructionType::JumpTo;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.IntOperand = static_cast<int32>(SubParser.ReadVarint());
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 2: // peekAndJump
			OutInstruction.Type = EYarnInstructionType::PeekAndJump;
			ReadBytes(); // Empty message
			break;

		case 3: // runLine
			{
				OutInstruction.Type = EYarnInstructionType::RunLine;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					int32 SubField = SubTag >> 3;
					if (SubField == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else if (SubField == 2)
					{
						OutInstruction.IntOperand = static_cast<int32>(SubParser.ReadVarint());
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 4: // runCommand
			{
				OutInstruction.Type = EYarnInstructionType::RunCommand;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					int32 SubField = SubTag >> 3;
					if (SubField == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else if (SubField == 2)
					{
						OutInstruction.IntOperand = static_cast<int32>(SubParser.ReadVarint());
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 5: // addOption
			{
				OutInstruction.Type = EYarnInstructionType::AddOption;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					int32 SubField = SubTag >> 3;
					if (SubField == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else if (SubField == 2)
					{
						OutInstruction.IntOperand = static_cast<int32>(SubParser.ReadVarint());
					}
					else if (SubField == 3)
					{
						OutInstruction.IntOperand2 = static_cast<int32>(SubParser.ReadVarint());
					}
					else if (SubField == 4)
					{
						OutInstruction.BoolOperand = SubParser.ReadVarint() != 0;
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 6: // showOptions
			OutInstruction.Type = EYarnInstructionType::ShowOptions;
			ReadBytes();
			break;

		case 7: // pushString
			{
				OutInstruction.Type = EYarnInstructionType::PushString;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 8: // pushFloat
			{
				OutInstruction.Type = EYarnInstructionType::PushFloat;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						// Float is wire type 5 (fixed32)
						uint32 Bits = SubParser.ReadFixed32();
						float FloatValue;
						FMemory::Memcpy(&FloatValue, &Bits, sizeof(FloatValue));
						OutInstruction.FloatOperand = FloatValue;
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 9: // pushBool
			{
				OutInstruction.Type = EYarnInstructionType::PushBool;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.BoolOperand = SubParser.ReadVarint() != 0;
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 10: // jumpIfFalse
			{
				OutInstruction.Type = EYarnInstructionType::JumpIfFalse;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.IntOperand = static_cast<int32>(SubParser.ReadVarint());
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 11: // pop
			OutInstruction.Type = EYarnInstructionType::Pop;
			ReadBytes();
			break;

		case 12: // callFunc
			{
				OutInstruction.Type = EYarnInstructionType::CallFunction;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 13: // pushVariable
			{
				OutInstruction.Type = EYarnInstructionType::PushVariable;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 14: // storeVariable
			{
				OutInstruction.Type = EYarnInstructionType::StoreVariable;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 15: // stop
			OutInstruction.Type = EYarnInstructionType::Stop;
			ReadBytes();
			break;

		case 16: // runNode
			{
				OutInstruction.Type = EYarnInstructionType::RunNode;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 17: // peekAndRunNode
			OutInstruction.Type = EYarnInstructionType::PeekAndRunNode;
			ReadBytes();
			break;

		case 18: // detourToNode
			{
				OutInstruction.Type = EYarnInstructionType::DetourToNode;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					if ((SubTag >> 3) == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 19: // peekAndDetourToNode
			OutInstruction.Type = EYarnInstructionType::PeekAndDetourToNode;
			ReadBytes();
			break;

		case 20: // return
			OutInstruction.Type = EYarnInstructionType::Return;
			ReadBytes();
			break;

		case 21: // addSaliencyCandidate
			{
				OutInstruction.Type = EYarnInstructionType::AddSaliencyCandidate;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					int32 SubField = SubTag >> 3;
					if (SubField == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else if (SubField == 2)
					{
						OutInstruction.IntOperand2 = static_cast<int32>(SubParser.ReadVarint());
					}
					else if (SubField == 3)
					{
						OutInstruction.IntOperand = static_cast<int32>(SubParser.ReadVarint());
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 22: // addSaliencyCandidateFromNode
			{
				OutInstruction.Type = EYarnInstructionType::AddSaliencyCandidateFromNode;
				TArray<uint8> SubData = ReadBytes();
				FYarnProtobufParser SubParser(SubData);
				while (SubParser.Position < SubData.Num())
				{
					uint64 SubTag = SubParser.ReadVarint();
					int32 SubField = SubTag >> 3;
					if (SubField == 1)
					{
						OutInstruction.StringOperand = SubParser.ReadString();
					}
					else if (SubField == 2)
					{
						OutInstruction.IntOperand = static_cast<int32>(SubParser.ReadVarint());
					}
					else
					{
						SubParser.SkipField(SubTag & 0x7);
					}
				}
			}
			break;

		case 23: // selectSaliencyCandidate
			OutInstruction.Type = EYarnInstructionType::SelectSaliencyCandidate;
			ReadBytes();
			break;

		default:
			// Unknown field: skip by wire type, exactly as generated protobuf code
			// does. Fields beyond 23 don't exist in the current schema.
			SkipField(WireType);
			break;
		}
	}

	return true;
}

bool FYarnProtobufParser::ParseOperand(FYarnValue& OutValue)
{
	while (Position < Data.Num())
	{
		uint64 Tag = ReadVarint();
		int32 FieldNumber = Tag >> 3;
		int32 WireType = Tag & 0x7;

		switch (FieldNumber)
		{
		case 1: // string_value
			OutValue = FYarnValue(ReadString());
			break;

		case 2: // bool_value
			OutValue = FYarnValue(ReadVarint() != 0);
			break;

		case 3: // float_value
			{
				uint32 Bits = ReadFixed32();
				float FloatValue;
				FMemory::Memcpy(&FloatValue, &Bits, sizeof(FloatValue));
				OutValue = FYarnValue(FloatValue);
			}
			break;

		default:
			SkipField(WireType);
			break;
		}
	}

	return true;
}


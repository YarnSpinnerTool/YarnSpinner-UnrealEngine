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

#include "YarnProgram.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

// FYarnInstruction factory methods

FYarnInstruction FYarnInstruction::JumpTo(int32 Destination)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::JumpTo;
	Inst.IntOperand = Destination;
	return Inst;
}

FYarnInstruction FYarnInstruction::PeekAndJump()
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::PeekAndJump;
	return Inst;
}

FYarnInstruction FYarnInstruction::RunLine(const FString& LineID, int32 SubstitutionCount)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::RunLine;
	Inst.StringOperand = LineID;
	Inst.IntOperand = SubstitutionCount;
	return Inst;
}

FYarnInstruction FYarnInstruction::RunCommand(const FString& CommandText, int32 SubstitutionCount)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::RunCommand;
	Inst.StringOperand = CommandText;
	Inst.IntOperand = SubstitutionCount;
	return Inst;
}

FYarnInstruction FYarnInstruction::AddOption(const FString& LineID, int32 Destination, int32 SubstitutionCount, bool bHasCondition)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::AddOption;
	Inst.StringOperand = LineID;
	Inst.IntOperand = Destination;
	Inst.IntOperand2 = SubstitutionCount;
	Inst.BoolOperand = bHasCondition;
	return Inst;
}

FYarnInstruction FYarnInstruction::ShowOptions()
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::ShowOptions;
	return Inst;
}

FYarnInstruction FYarnInstruction::PushString(const FString& Value)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::PushString;
	Inst.StringOperand = Value;
	return Inst;
}

FYarnInstruction FYarnInstruction::PushFloat(float Value)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::PushFloat;
	Inst.FloatOperand = Value;
	return Inst;
}

FYarnInstruction FYarnInstruction::PushBool(bool bValue)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::PushBool;
	Inst.BoolOperand = bValue;
	return Inst;
}

FYarnInstruction FYarnInstruction::JumpIfFalse(int32 Destination)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::JumpIfFalse;
	Inst.IntOperand = Destination;
	return Inst;
}

FYarnInstruction FYarnInstruction::Pop()
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::Pop;
	return Inst;
}

FYarnInstruction FYarnInstruction::CallFunction(const FString& FunctionName)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::CallFunction;
	Inst.StringOperand = FunctionName;
	return Inst;
}

FYarnInstruction FYarnInstruction::PushVariable(const FString& VariableName)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::PushVariable;
	Inst.StringOperand = VariableName;
	return Inst;
}

FYarnInstruction FYarnInstruction::StoreVariable(const FString& VariableName)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::StoreVariable;
	Inst.StringOperand = VariableName;
	return Inst;
}

FYarnInstruction FYarnInstruction::Stop()
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::Stop;
	return Inst;
}

FYarnInstruction FYarnInstruction::RunNode(const FString& NodeName)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::RunNode;
	Inst.StringOperand = NodeName;
	return Inst;
}

FYarnInstruction FYarnInstruction::PeekAndRunNode()
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::PeekAndRunNode;
	return Inst;
}

FYarnInstruction FYarnInstruction::DetourToNode(const FString& NodeName)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::DetourToNode;
	Inst.StringOperand = NodeName;
	return Inst;
}

FYarnInstruction FYarnInstruction::PeekAndDetourToNode()
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::PeekAndDetourToNode;
	return Inst;
}

FYarnInstruction FYarnInstruction::Return()
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::Return;
	return Inst;
}

FYarnInstruction FYarnInstruction::AddSaliencyCandidate(const FString& ContentID, int32 ComplexityScore, int32 Destination)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::AddSaliencyCandidate;
	Inst.StringOperand = ContentID;
	Inst.IntOperand = Destination;
	Inst.IntOperand2 = ComplexityScore;
	return Inst;
}

FYarnInstruction FYarnInstruction::AddSaliencyCandidateFromNode(const FString& NodeName, int32 Destination)
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::AddSaliencyCandidateFromNode;
	Inst.StringOperand = NodeName;
	Inst.IntOperand = Destination;
	return Inst;
}

FYarnInstruction FYarnInstruction::SelectSaliencyCandidate()
{
	FYarnInstruction Inst;
	Inst.Type = EYarnInstructionType::SelectSaliencyCandidate;
	return Inst;
}

// FYarnNode implementation

FString FYarnNode::GetHeaderValue(const FString& Key) const
{
	for (const FYarnHeader& Header : Headers)
	{
		if (Header.Key == Key)
		{
			return Header.Value;
		}
	}
	return FString();
}

bool FYarnNode::HasHeader(const FString& Key) const
{
	for (const FYarnHeader& Header : Headers)
	{
		if (Header.Key == Key)
		{
			return true;
		}
	}
	return false;
}

FString FYarnNode::GetTrackingVariableName() const
{
	// The tracking variable name is stored in the "$Yarn.Internal.TrackingVariable" header
	// and is the full variable name (e.g., "$Yarn.Internal.Visiting.NodeName")
	return GetHeaderValue(TEXT("$Yarn.Internal.TrackingVariable"));
}

TArray<FString> FYarnNode::GetContentSaliencyConditionVariables() const
{
	TArray<FString> Variables;

	// The "$Yarn.Internal.ContentSaliencyVariables" header contains a semicolon-delimited
	// list of smart variable names
	FString VariablesHeader = GetHeaderValue(TEXT("$Yarn.Internal.ContentSaliencyVariables"));
	if (!VariablesHeader.IsEmpty())
	{
		VariablesHeader.ParseIntoArray(Variables, TEXT(";"), true);
	}

	return Variables;
}

int32 FYarnNode::GetContentSaliencyConditionComplexityScore() const
{
	// The complexity score is stored in the "$Yarn.Internal.ContentSaliencyComplexity" header
	FString ComplexityHeader = GetHeaderValue(TEXT("$Yarn.Internal.ContentSaliencyComplexity"));
	if (!ComplexityHeader.IsEmpty())
	{
		return FCString::Atoi(*ComplexityHeader);
	}

	// Return -1 when no complexity header is set
	return -1;
}

TArray<FString> FYarnNode::GetLineIDs() const
{
	TArray<FString> LineIDs;

	// Scan instructions for RunLine and AddOption which reference line IDs
	for (const FYarnInstruction& Instruction : Instructions)
	{
		if (Instruction.Type == EYarnInstructionType::RunLine ||
			Instruction.Type == EYarnInstructionType::AddOption)
		{
			if (!Instruction.StringOperand.IsEmpty())
			{
				LineIDs.AddUnique(Instruction.StringOperand);
			}
		}
	}

	return LineIDs;
}

// FYarnProgram implementation

const FYarnNode* FYarnProgram::GetNode(const FString& NodeName) const
{
	return Nodes.Find(NodeName);
}

bool FYarnProgram::HasNode(const FString& NodeName) const
{
	return Nodes.Contains(NodeName);
}

bool FYarnProgram::TryGetInitialValue(const FString& VariableName, FYarnValue& OutValue) const
{
	const FYarnValue* Value = InitialValues.Find(VariableName);
	if (Value)
	{
		OutValue = *Value;
		return true;
	}
	return false;
}

TArray<FString> FYarnProgram::GetLineIDsForNode(const FString& NodeName) const
{
	const FYarnNode* Node = GetNode(NodeName);
	if (Node)
	{
		return Node->GetLineIDs();
	}
	return TArray<FString>();
}

// UYarnProject implementation

FString UYarnProject::GetBaseText(const FString& LineID) const
{
	const FString* Text = BaseStringTable.Find(LineID);
	return Text ? *Text : FString();
}

FString UYarnProject::GetLocalizedText(const FString& LineID, const FString& CultureCode) const
{
	// Try exact culture match first
	if (const FYarnLocalization* Localization = Localizations.Find(CultureCode))
	{
		if (const FString* Text = Localization->Strings.Find(LineID))
		{
			return *Text;
		}
	}

	// Try base culture (e.g., "en" from "en-US")
	int32 DashIndex;
	if (CultureCode.FindChar(TEXT('-'), DashIndex))
	{
		FString BaseCulture = CultureCode.Left(DashIndex);
		if (const FYarnLocalization* Localization = Localizations.Find(BaseCulture))
		{
			if (const FString* Text = Localization->Strings.Find(LineID))
			{
				return *Text;
			}
		}
	}

	// Fall back to base text
	return GetBaseText(LineID);
}

bool UYarnProject::HasLocalization(const FString& CultureCode) const
{
	return Localizations.Contains(CultureCode);
}

TArray<FString> UYarnProject::GetAvailableCultures() const
{
	TArray<FString> Cultures;
	Localizations.GetKeys(Cultures);
	return Cultures;
}

bool UYarnProject::HasNode(const FString& NodeName) const
{
	return Program.HasNode(NodeName);
}

bool UYarnProject::TryGetInitialValue(const FString& VariableName, FYarnValue& OutValue) const
{
	return Program.TryGetInitialValue(VariableName, OutValue);
}

void UYarnProject::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

#if WITH_EDITORONLY_DATA
void UYarnProject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

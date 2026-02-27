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

#include "YarnSpinnerCore.h"
#include "YarnSpinnerModule.h"

// Default return values for type-mismatched accessors
static const FString EmptyString = TEXT("");
static const float ZeroFloat = 0.0f;

// FYarnValue implementation

FYarnValue::FYarnValue()
	: Type(EYarnValueType::None)
	, StringValue()
	, NumberValue(0.0f)
	, BoolValue(false)
{
}

FYarnValue::FYarnValue(const FString& InString)
	: Type(EYarnValueType::String)
	, StringValue(InString)
	, NumberValue(0.0f)
	, BoolValue(false)
{
}

FYarnValue::FYarnValue(float InNumber)
	: Type(EYarnValueType::Number)
	, StringValue()
	, NumberValue(InNumber)
	, BoolValue(false)
{
}

FYarnValue::FYarnValue(bool bInBool)
	: Type(EYarnValueType::Bool)
	, StringValue()
	, NumberValue(0.0f)
	, BoolValue(bInBool)
{
}

const FString& FYarnValue::GetStringValue() const
{
	if (Type == EYarnValueType::String)
	{
		return StringValue;
	}
	return EmptyString;
}

float FYarnValue::GetNumberValue() const
{
	if (Type == EYarnValueType::Number)
	{
		return NumberValue;
	}
	return ZeroFloat;
}

bool FYarnValue::GetBoolValue() const
{
	if (Type == EYarnValueType::Bool)
	{
		return BoolValue;
	}
	return false;
}

FString FYarnValue::ConvertToString() const
{
	switch (Type)
	{
	case EYarnValueType::String:
		return StringValue;

	case EYarnValueType::Number:
		// If value has no fractional part, format as integer
		if (FMath::Frac(NumberValue) == 0.0f)
		{
			return FString::Printf(TEXT("%d"), FMath::RoundToInt(NumberValue));
		}
		return FString::SanitizeFloat(NumberValue);

	case EYarnValueType::Bool:
		return BoolValue ? TEXT("True") : TEXT("False");

	default:
		return TEXT("");
	}
}

float FYarnValue::ConvertToNumber() const
{
	switch (Type)
	{
	case EYarnValueType::String:
		return FCString::Atof(*StringValue);

	case EYarnValueType::Number:
		return NumberValue;

	case EYarnValueType::Bool:
		return BoolValue ? 1.0f : 0.0f;

	default:
		return 0.0f;
	}
}

bool FYarnValue::ConvertToBool() const
{
	switch (Type)
	{
	case EYarnValueType::String:
		// Only "True" and "False" (case-insensitive) are valid boolean strings.
		if (StringValue.Equals(TEXT("True"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (StringValue.Equals(TEXT("False"), ESearchCase::IgnoreCase))
		{
			return false;
		}
		// Non-boolean string: log a warning and return false
		UE_LOG(LogYarnSpinner, Warning, TEXT("FYarnValue::ConvertToBool: String '%s' is not a valid boolean value"), *StringValue);
		return false;

	case EYarnValueType::Number:
		// Returns true for any non-zero value
		return NumberValue != 0.0f;

	case EYarnValueType::Bool:
		return BoolValue;

	default:
		return false;
	}
}

bool FYarnValue::operator==(const FYarnValue& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}

	switch (Type)
	{
	case EYarnValueType::String:
		return StringValue == Other.StringValue;

	case EYarnValueType::Number:
		return NumberValue == Other.NumberValue;

	case EYarnValueType::Bool:
		return BoolValue == Other.BoolValue;

	default:
		return true; // both are None
	}
}

// FYarnOptionSet implementation

const FYarnOption* FYarnOptionSet::GetOptionByID(int32 OptionID) const
{
	for (const FYarnOption& Option : Options)
	{
		if (Option.OptionID == OptionID)
		{
			return &Option;
		}
	}
	return nullptr;
}

bool FYarnOptionSet::HasAvailableOptions() const
{
	for (const FYarnOption& Option : Options)
	{
		if (Option.bIsAvailable)
		{
			return true;
		}
	}
	return false;
}

// FYarnCommand implementation

FYarnCommand::FYarnCommand(const FString& InCommandText)
	: CommandText(InCommandText)
{
	ParseCommandText(InCommandText, CommandName, Parameters);
}

void FYarnCommand::ParseCommandText(const FString& InCommandText, FString& OutName, TArray<FString>& OutParameters)
{
	OutName.Empty();
	OutParameters.Empty();

	if (InCommandText.IsEmpty())
	{
		return;
	}

	// Parse the command text into words, respecting quoted strings and escape sequences
	// Supports: \" (escaped quote), \\ (escaped backslash), \n (newline), \t (tab)
	TArray<FString> Words;
	bool bInQuotes = false;
	bool bEscapeNext = false;
	FString CurrentWord;

	for (int32 i = 0; i < InCommandText.Len(); i++)
	{
		TCHAR Char = InCommandText[i];

		// Handle escape sequences
		if (bEscapeNext)
		{
			bEscapeNext = false;
			switch (Char)
			{
			case TEXT('"'):
				CurrentWord.AppendChar(TEXT('"'));
				break;
			case TEXT('\\'):
				CurrentWord.AppendChar(TEXT('\\'));
				break;
			case TEXT('n'):
				CurrentWord.AppendChar(TEXT('\n'));
				break;
			case TEXT('t'):
				CurrentWord.AppendChar(TEXT('\t'));
				break;
			case TEXT('r'):
				CurrentWord.AppendChar(TEXT('\r'));
				break;
			default:
				// Unknown escape - keep both characters
				CurrentWord.AppendChar(TEXT('\\'));
				CurrentWord.AppendChar(Char);
				break;
			}
			continue;
		}

		// Check for escape character
		if (Char == TEXT('\\') && bInQuotes)
		{
			bEscapeNext = true;
			continue;
		}

		if (Char == TEXT('"'))
		{
			bInQuotes = !bInQuotes;
		}
		else if (Char == TEXT(' ') && !bInQuotes)
		{
			if (!CurrentWord.IsEmpty())
			{
				Words.Add(CurrentWord);
				CurrentWord.Empty();
			}
		}
		else
		{
			CurrentWord.AppendChar(Char);
		}
	}

	// Handle trailing escape (shouldn't happen in well-formed input)
	if (bEscapeNext)
	{
		CurrentWord.AppendChar(TEXT('\\'));
	}

	if (!CurrentWord.IsEmpty())
	{
		Words.Add(CurrentWord);
	}

	if (Words.Num() > 0)
	{
		OutName = Words[0];

		for (int32 i = 1; i < Words.Num(); i++)
		{
			OutParameters.Add(Words[i]);
		}
	}
}

// FYarnLocalizedLine implementation

FYarnLocalizedLine FYarnLocalizedLine::InvalidLine()
{
	// Return a line with empty LineID (IsValid() will return false)
	FYarnLocalizedLine Line;
	Line.RawLine.LineID = FString();
	Line.Text = FText::GetEmpty();
	Line.TextWithoutCharacterName = FText::GetEmpty();
	Line.CharacterName = FString();
	return Line;
}

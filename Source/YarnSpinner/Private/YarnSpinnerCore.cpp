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
#include <charconv>
#include <cmath>

// Default return values for type-mismatched accessors
static const FString EmptyString = TEXT("");
static const float ZeroFloat = 0.0f;

static FString FormatYarnNumber(float Value)
{
	if (FMath::IsNaN(Value))
	{
		return TEXT("NaN");
	}

	if (!FMath::IsFinite(Value))
	{
		return Value > 0.0f ? TEXT("Infinity") : TEXT("-Infinity");
	}

	if (Value == 0.0f)
	{
		return std::signbit(Value) ? TEXT("-0") : TEXT("0");
	}

	char Buffer[64];
	const std::to_chars_result ConvResult = std::to_chars(Buffer, Buffer + UE_ARRAY_COUNT(Buffer), Value, std::chars_format::scientific);
	const int32 Length = static_cast<int32>(ConvResult.ptr - Buffer);

	int32 Index = 0;
	bool bNegative = false;
	if (Buffer[Index] == '-')
	{
		bNegative = true;
		Index++;
	}

	FString Digits;
	Digits.AppendChar(Buffer[Index]);
	Index++;
	if (Index < Length && Buffer[Index] == '.')
	{
		Index++;
		while (Index < Length && Buffer[Index] != 'e')
		{
			Digits.AppendChar(Buffer[Index]);
			Index++;
		}
	}

	Index++;
	const bool bExponentNegative = (Buffer[Index] == '-');
	if (Buffer[Index] == '+' || Buffer[Index] == '-')
	{
		Index++;
	}

	int32 Exponent = 0;
	while (Index < Length)
	{
		Exponent = Exponent * 10 + (Buffer[Index] - '0');
		Index++;
	}
	if (bExponentNegative)
	{
		Exponent = -Exponent;
	}

	const int32 NumDigits = Digits.Len();
	const bool bUsePlain = (Exponent >= -4 && Exponent < 9);

	FString Result;
	if (bUsePlain)
	{
		if (Exponent >= NumDigits - 1)
		{
			Result = Digits;
			for (int32 i = NumDigits - 1; i < Exponent; i++)
			{
				Result.AppendChar(TEXT('0'));
			}
		}
		else if (Exponent >= 0)
		{
			Result = Digits.Left(Exponent + 1) + TEXT(".") + Digits.Mid(Exponent + 1);
		}
		else
		{
			Result = TEXT("0.");
			for (int32 i = 0; i < -Exponent - 1; i++)
			{
				Result.AppendChar(TEXT('0'));
			}
			Result += Digits;
		}
	}
	else
	{
		Result = Digits.Left(1);
		if (NumDigits > 1)
		{
			Result += TEXT(".") + Digits.Mid(1);
		}
		Result += TEXT("E");
		Result += Exponent >= 0 ? TEXT("+") : TEXT("-");
		FString ExponentDigits = FString::Printf(TEXT("%d"), FMath::Abs(Exponent));
		while (ExponentDigits.Len() < 2)
		{
			ExponentDigits = TEXT("0") + ExponentDigits;
		}
		Result += ExponentDigits;
	}

	return bNegative ? TEXT("-") + Result : Result;
}

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
		return FormatYarnNumber(NumberValue);

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
		return StringValue.Equals(Other.StringValue, ESearchCase::CaseSensitive);

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

	TArray<FString> Words;
	FString CurrentWord;

	const int32 Len = InCommandText.Len();
	int32 i = 0;
	while (i < Len)
	{
		TCHAR Char = InCommandText[i];

		if (FChar::IsWhitespace(Char))
		{
			if (!CurrentWord.IsEmpty())
			{
				Words.Add(CurrentWord);
				CurrentWord.Empty();
			}
			i++;
			continue;
		}

		if (Char == TEXT('"'))
		{
			i++;
			bool bTerminated = false;
			while (i < Len)
			{
				TCHAR QuotedChar = InCommandText[i];
				if (QuotedChar == TEXT('\\'))
				{
					const TCHAR Next = (i + 1 < Len) ? InCommandText[i + 1] : TEXT('\0');
					if (Next == TEXT('\\') || Next == TEXT('"'))
					{
						CurrentWord.AppendChar(Next);
						i += 2;
					}
					else
					{
						CurrentWord.AppendChar(QuotedChar);
						i++;
					}
				}
				else if (QuotedChar == TEXT('"'))
				{
					bTerminated = true;
					i++;
					break;
				}
				else
				{
					CurrentWord.AppendChar(QuotedChar);
					i++;
				}
			}

			if (!bTerminated)
			{
				Words.Add(CurrentWord);
				CurrentWord.Empty();
				break;
			}

			Words.Add(CurrentWord);
			CurrentWord.Empty();
		}
		else
		{
			CurrentWord.AppendChar(Char);
			i++;
		}
	}

	if (!CurrentWord.IsEmpty())
	{
		Words.Add(CurrentWord);
	}

	if (Words.Num() > 0)
	{
		OutName = Words[0];

		for (int32 w = 1; w < Words.Num(); w++)
		{
			OutParameters.Add(Words[w]);
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

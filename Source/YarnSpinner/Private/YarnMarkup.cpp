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

#include "YarnMarkup.h"
#include "YarnSpinnerModule.h"
#include "Internationalization/Regex.h"

// ============================================================================
// FYarnMarkupStyle implementation
// ============================================================================

void FYarnMarkupStyle::GenerateRichTextTags(FString& OutStartTag, FString& OutEndTag) const
{
	OutStartTag.Empty();
	OutEndTag.Empty();

	if (bUseColor)
	{
		FString HexColor = UYarnMarkupLibrary::ColorToHexString(Color);
		OutStartTag += FString::Printf(TEXT("<color=%s>"), *HexColor);
		OutEndTag = TEXT("</color>") + OutEndTag;
	}

	if (bBold)
	{
		OutStartTag += TEXT("<b>");
		OutEndTag = TEXT("</b>") + OutEndTag;
	}

	if (bItalic)
	{
		OutStartTag += TEXT("<i>");
		OutEndTag = TEXT("</i>") + OutEndTag;
	}

	if (bUnderline)
	{
		OutStartTag += TEXT("<u>");
		OutEndTag = TEXT("</u>") + OutEndTag;
	}

	if (bStrikethrough)
	{
		OutStartTag += TEXT("<s>");
		OutEndTag = TEXT("</s>") + OutEndTag;
	}
}

// ============================================================================
// UYarnRichTextPalette implementation
// ============================================================================

const FYarnMarkupStyle* UYarnRichTextPalette::FindBasicStyle(const FString& MarkerName) const
{
	for (const FYarnMarkupStyle& Style : BasicStyles)
	{
		if (Style.MarkerName.Equals(MarkerName, ESearchCase::IgnoreCase))
		{
			return &Style;
		}
	}
	return nullptr;
}

const FYarnRichTextMarker* UYarnRichTextPalette::FindCustomMarker(const FString& MarkerName) const
{
	for (const FYarnRichTextMarker& Marker : CustomMarkers)
	{
		if (Marker.MarkerName.Equals(MarkerName, ESearchCase::IgnoreCase))
		{
			return &Marker;
		}
	}
	return nullptr;
}

// ============================================================================
// UYarnPaletteMarkupProcessor implementation
// ============================================================================

UYarnPaletteMarkupProcessor::UYarnPaletteMarkupProcessor()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FYarnMarkupReplacementResult UYarnPaletteMarkupProcessor::ProcessMarkup_Implementation(
	const FYarnMarkupAttribute& Attribute,
	FString& Text,
	TArray<FYarnMarkupAttribute>& ChildAttributes,
	const FString& LocaleCode)
{
	FYarnMarkupReplacementResult Result;

	if (!Palette)
	{
		Result.Diagnostics.Add(TEXT("No palette assigned to PaletteMarkupProcessor"));
		return Result;
	}

	// Check basic styles first
	if (const FYarnMarkupStyle* Style = Palette->FindBasicStyle(Attribute.Name))
	{
		FString StartTag, EndTag;
		Style->GenerateRichTextTags(StartTag, EndTag);

		int32 OriginalLength = Text.Len();
		Text = StartTag + Text + EndTag;
		Result.InvisibleCharactersAdded = Text.Len() - OriginalLength;

		return Result;
	}

	// Check custom markers
	if (const FYarnRichTextMarker* Marker = Palette->FindCustomMarker(Attribute.Name))
	{
		int32 OriginalLength = Text.Len();

		Text = Marker->StartTag + Text + Marker->EndTag;

		Result.InvisibleCharactersAdded = Text.Len() - OriginalLength;

		// Adjust child attributes if visible characters were added at the start
		if (Marker->VisibleCharactersAtStart > 0)
		{
			for (FYarnMarkupAttribute& Child : ChildAttributes)
			{
				Child.Position += Marker->VisibleCharactersAtStart;
			}
		}

		return Result;
	}

	// Unknown marker - pass through without modification
	UE_LOG(LogYarnSpinner, Warning, TEXT("Unknown markup marker: %s"), *Attribute.Name);
	return Result;
}

// ============================================================================
// UYarnStyleMarkupProcessor implementation
// ============================================================================

UYarnStyleMarkupProcessor::UYarnStyleMarkupProcessor()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FYarnMarkupReplacementResult UYarnStyleMarkupProcessor::ProcessMarkup_Implementation(
	const FYarnMarkupAttribute& Attribute,
	FString& Text,
	TArray<FYarnMarkupAttribute>& ChildAttributes,
	const FString& LocaleCode)
{
	FYarnMarkupReplacementResult Result;

	// Only process "style" markers
	if (!Attribute.Name.Equals(TEXT("style"), ESearchCase::IgnoreCase))
	{
		return Result;
	}

	// Get the style name from the attribute properties
	FString StyleName = Attribute.GetProperty(TEXT("style"));
	if (StyleName.IsEmpty())
	{
		// Try using the first property value as the style name
		for (const auto& Pair : Attribute.Properties)
		{
			StyleName = Pair.Value.ToString();
			break;
		}
	}

	if (StyleName.IsEmpty())
	{
		Result.Diagnostics.Add(TEXT("Style marker missing style name property"));
		return Result;
	}

	int32 OriginalLength = Text.Len();

	// Wrap in rich text style tags
	FString StartTag = FString::Printf(TEXT("<style=\"%s\">"), *StyleName);
	FString EndTag = TEXT("</style>");

	Text = StartTag + Text + EndTag;

	Result.InvisibleCharactersAdded = Text.Len() - OriginalLength;

	return Result;
}

// ============================================================================
// CLDR Plural Rules
// Auto-generated from Unicode CLDR, version 42.0
// Covers 130+ languages with correct cardinal and ordinal plural rules
// ============================================================================

namespace YarnPlurals
{
	enum class EPluralCase : uint8
	{
		Zero,
		One,
		Two,
		Few,
		Many,
		Other
	};

	// ---- Helper functions for plural rule evaluation ----

	static double AbsoluteValue(double Number)
	{
		return FMath::Abs(Number);
	}

	static int32 IntegerValue(double Number)
	{
		return static_cast<int32>(FMath::TruncToDouble(Number));
	}

	static int32 FractionalValue(double Number)
	{
		FString Text = FString::SanitizeFloat(Number, 0);
		int32 DotIdx = Text.Find(TEXT("."));
		if (DotIdx < 0)
		{
			return 0;
		}
		FString FracStr = Text.Mid(DotIdx + 1);
		if (FracStr.IsEmpty())
		{
			return 0;
		}
		return FCString::Atoi(*FracStr);
	}

	static int32 VisibleFractionalDigits(double Number, bool bTrailingZeroes)
	{
		FString Text = FString::SanitizeFloat(Number, 0);
		int32 DotIdx = Text.Find(TEXT("."));
		if (DotIdx < 0)
		{
			return 0;
		}
		FString FracStr = Text.Mid(DotIdx + 1);
		if (!bTrailingZeroes)
		{
			FracStr.TrimEndInline();
			// Trim trailing zeroes
			while (FracStr.Len() > 0 && FracStr[FracStr.Len() - 1] == TEXT('0'))
			{
				FracStr = FracStr.Left(FracStr.Len() - 1);
			}
		}
		return FracStr.Len();
	}

	// ---- Cardinal plural case functions (0-38) ----
	// Each function implements the CLDR plural rules for a group of languages

	// Bambara, Tibetan, Dzongkha, Indonesian, Japanese, Korean, Lao, Malay,
	// Burmese, Thai, Vietnamese, Chinese, etc. (always Other)
	static EPluralCase GetCardinalPluralCase_0(double Number)
	{
		return EPluralCase::Other;
	}

	// Amharic, Assamese, Bangla, Dogri, Persian, Gujarati, Hindi, Kannada, etc.
	static EPluralCase GetCardinalPluralCase_1(double Number)
	{
		double n = AbsoluteValue(Number);
		int32 i = IntegerValue(Number);
		if ((i == 0) || (n == 1)) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Fula, Armenian, Kabyle
	static EPluralCase GetCardinalPluralCase_2(double Number)
	{
		int32 i = IntegerValue(Number);
		if ((i == 0) || (i == 1)) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Asturian, German, English, Estonian, Finnish, Galician, Dutch, Swedish, etc.
	static EPluralCase GetCardinalPluralCase_3(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((i == 1) && (v == 0)) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Sinhala
	static EPluralCase GetCardinalPluralCase_4(double Number)
	{
		double n = AbsoluteValue(Number);
		int32 i = IntegerValue(Number);
		int32 f = FractionalValue(Number);
		if (((n == 0) || (n == 1)) || ((i == 0) && (f == 1))) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Akan, Bhojpuri, Lingala, Malagasy, Northern Sotho, Punjabi, Tigrinya, Walloon
	static EPluralCase GetCardinalPluralCase_5(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n >= 0 && n <= 1)) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Central Atlas Tamazight
	static EPluralCase GetCardinalPluralCase_6(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n >= 0 && n <= 1) || (n >= 11 && n <= 99)) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Afrikaans, Bulgarian, Greek, Hungarian, Turkish, and many others (n == 1 -> One)
	static EPluralCase GetCardinalPluralCase_7(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 1) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Danish
	static EPluralCase GetCardinalPluralCase_8(double Number)
	{
		double n = AbsoluteValue(Number);
		int32 i = IntegerValue(Number);
		int32 t = FractionalValue(Number);
		if ((n == 1) || (!(t == 0) && ((i == 0) || (i == 1)))) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Icelandic
	static EPluralCase GetCardinalPluralCase_9(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 t = FractionalValue(Number);
		if (((t == 0) && ((i % 10) == 1) && !((i % 100) == 11)) || (((t % 10) == 1) && !((t % 100) == 11)))
			return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Macedonian
	static EPluralCase GetCardinalPluralCase_10(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		int32 f = FractionalValue(Number);
		if (((v == 0) && ((i % 10) == 1) && !((i % 100) == 11)) || (((f % 10) == 1) && !((f % 100) == 11)))
			return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Cebuano, Filipino, Tagalog
	static EPluralCase GetCardinalPluralCase_11(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		int32 f = FractionalValue(Number);
		if (((v == 0) && ((i == 1) || (i == 2) || (i == 3))) ||
			((v == 0) && !(((i % 10) == 4) || ((i % 10) == 6) || ((i % 10) == 9))) ||
			(!(v == 0) && !(((f % 10) == 4) || ((f % 10) == 6) || ((f % 10) == 9))))
			return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Latvian, Prussian
	static EPluralCase GetCardinalPluralCase_12(double Number)
	{
		double n = AbsoluteValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		int32 f = FractionalValue(Number);
		if (((static_cast<int64>(n) % 10) == 0) ||
			((static_cast<int64>(n) % 100) >= 11 && (static_cast<int64>(n) % 100) <= 19) ||
			((v == 2) && ((f % 100) >= 11 && (f % 100) <= 19)))
			return EPluralCase::Zero;
		if (((static_cast<int64>(n) % 10) == 1 && !((static_cast<int64>(n) % 100) == 11)) ||
			((v == 2) && ((f % 10) == 1) && !((f % 100) == 11)) ||
			(!(v == 2) && ((f % 10) == 1)))
			return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Langi
	static EPluralCase GetCardinalPluralCase_13(double Number)
	{
		double n = AbsoluteValue(Number);
		int32 i = IntegerValue(Number);
		if (n == 0) return EPluralCase::Zero;
		if (((i == 0) || (i == 1)) && !(n == 0)) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Colognian
	static EPluralCase GetCardinalPluralCase_14(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 0) return EPluralCase::Zero;
		if (n == 1) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Hebrew
	static EPluralCase GetCardinalPluralCase_15(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((i == 1) && (v == 0)) return EPluralCase::One;
		if ((i == 2) && (v == 0)) return EPluralCase::Two;
		if ((i == 0) && !(v == 0)) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Inuktitut, Nama, Santali, Northern Sami, etc.
	static EPluralCase GetCardinalPluralCase_16(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 1) return EPluralCase::One;
		if (n == 2) return EPluralCase::Two;
		return EPluralCase::Other;
	}

	// Tachelhit
	static EPluralCase GetCardinalPluralCase_17(double Number)
	{
		double n = AbsoluteValue(Number);
		int32 i = IntegerValue(Number);
		if ((i == 0) || (n == 1)) return EPluralCase::One;
		if (n >= 2 && n <= 10) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Romanian
	static EPluralCase GetCardinalPluralCase_18(double Number)
	{
		double n = AbsoluteValue(Number);
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((i == 1) && (v == 0)) return EPluralCase::One;
		if (!(v == 0) || (n == 0) || (!(n == 1) && ((static_cast<int64>(n) % 100) >= 1 && (static_cast<int64>(n) % 100) <= 19)))
			return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Bosnian, Croatian, Serbo-Croatian, Serbian
	static EPluralCase GetCardinalPluralCase_19(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		int32 f = FractionalValue(Number);
		if (((v == 0) && ((i % 10) == 1) && !((i % 100) == 11)) || (((f % 10) == 1) && !((f % 100) == 11)))
			return EPluralCase::One;
		if (((v == 0) && ((i % 10) >= 2 && (i % 10) <= 4) && !((i % 100) >= 12 && (i % 100) <= 14)) ||
			(((f % 10) >= 2 && (f % 10) <= 4) && !((f % 100) >= 12 && (f % 100) <= 14)))
			return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// French
	static EPluralCase GetCardinalPluralCase_20(double Number)
	{
		int32 i = IntegerValue(Number);
		if ((i == 0) || (i == 1)) return EPluralCase::One;
		return EPluralCase::Many;
	}

	// Portuguese
	static EPluralCase GetCardinalPluralCase_21(double Number)
	{
		int32 i = IntegerValue(Number);
		if (i >= 0 && i <= 1) return EPluralCase::One;
		return EPluralCase::Many;
	}

	// Catalan, Italian, European Portuguese (pt_PT), Venetian
	static EPluralCase GetCardinalPluralCase_22(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((i == 1) && (v == 0)) return EPluralCase::One;
		return EPluralCase::Many;
	}

	// Spanish
	static EPluralCase GetCardinalPluralCase_23(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 1) return EPluralCase::One;
		return EPluralCase::Many;
	}

	// Scottish Gaelic
	static EPluralCase GetCardinalPluralCase_24(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 1) || (n == 11)) return EPluralCase::One;
		if ((n == 2) || (n == 12)) return EPluralCase::Two;
		if ((n >= 3 && n <= 10) || (n >= 13 && n <= 19)) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Slovenian
	static EPluralCase GetCardinalPluralCase_25(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((v == 0) && ((i % 100) == 1)) return EPluralCase::One;
		if ((v == 0) && ((i % 100) == 2)) return EPluralCase::Two;
		if (((v == 0) && ((i % 100) >= 3 && (i % 100) <= 4)) || !(v == 0)) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Lower Sorbian, Upper Sorbian
	static EPluralCase GetCardinalPluralCase_26(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		int32 f = FractionalValue(Number);
		if (((v == 0) && ((i % 100) == 1)) || ((f % 100) == 1)) return EPluralCase::One;
		if (((v == 0) && ((i % 100) == 2)) || ((f % 100) == 2)) return EPluralCase::Two;
		if (((v == 0) && ((i % 100) >= 3 && (i % 100) <= 4)) || ((f % 100) >= 3 && (f % 100) <= 4))
			return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Czech, Slovak
	static EPluralCase GetCardinalPluralCase_27(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((i == 1) && (v == 0)) return EPluralCase::One;
		if ((i >= 2 && i <= 4) && (v == 0)) return EPluralCase::Few;
		if (!(v == 0)) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Polish
	static EPluralCase GetCardinalPluralCase_28(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((i == 1) && (v == 0)) return EPluralCase::One;
		if ((v == 0) && ((i % 10) >= 2 && (i % 10) <= 4) && !((i % 100) >= 12 && (i % 100) <= 14))
			return EPluralCase::Few;
		if (((v == 0) && !(i == 1) && ((i % 10) >= 0 && (i % 10) <= 1)) ||
			((v == 0) && ((i % 10) >= 5 && (i % 10) <= 9)) ||
			((v == 0) && ((i % 100) >= 12 && (i % 100) <= 14)))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Belarusian
	static EPluralCase GetCardinalPluralCase_29(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (((ni % 10) == 1) && !((ni % 100) == 11)) return EPluralCase::One;
		if (((ni % 10) >= 2 && (ni % 10) <= 4) && !((ni % 100) >= 12 && (ni % 100) <= 14))
			return EPluralCase::Few;
		if (((ni % 10) == 0) || ((ni % 10) >= 5 && (ni % 10) <= 9) || ((ni % 100) >= 11 && (ni % 100) <= 14))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Lithuanian
	static EPluralCase GetCardinalPluralCase_30(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		int32 f = FractionalValue(Number);
		if (((ni % 10) == 1) && !((ni % 100) >= 11 && (ni % 100) <= 19)) return EPluralCase::One;
		if (((ni % 10) >= 2 && (ni % 10) <= 9) && !((ni % 100) >= 11 && (ni % 100) <= 19))
			return EPluralCase::Few;
		if (!(f == 0)) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Russian, Ukrainian
	static EPluralCase GetCardinalPluralCase_31(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((v == 0) && ((i % 10) == 1) && !((i % 100) == 11)) return EPluralCase::One;
		if ((v == 0) && ((i % 10) >= 2 && (i % 10) <= 4) && !((i % 100) >= 12 && (i % 100) <= 14))
			return EPluralCase::Few;
		if (((v == 0) && ((i % 10) == 0)) ||
			((v == 0) && ((i % 10) >= 5 && (i % 10) <= 9)) ||
			((v == 0) && ((i % 100) >= 11 && (i % 100) <= 14)))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Breton
	static EPluralCase GetCardinalPluralCase_32(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (((ni % 10) == 1) && !(((ni % 100) == 11) || ((ni % 100) == 71) || ((ni % 100) == 91)))
			return EPluralCase::One;
		if (((ni % 10) == 2) && !(((ni % 100) == 12) || ((ni % 100) == 72) || ((ni % 100) == 92)))
			return EPluralCase::Two;
		if ((((ni % 10) >= 3 && (ni % 10) <= 4) || ((ni % 10) == 9)) &&
			!(((ni % 100) >= 10 && (ni % 100) <= 19) || ((ni % 100) >= 70 && (ni % 100) <= 79) || ((ni % 100) >= 90 && (ni % 100) <= 99)))
			return EPluralCase::Few;
		if (!(n == 0) && ((ni % 1000000) == 0)) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Maltese
	static EPluralCase GetCardinalPluralCase_33(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (n == 1) return EPluralCase::One;
		if (n == 2) return EPluralCase::Two;
		if ((n == 0) || ((ni % 100) >= 3 && (ni % 100) <= 10)) return EPluralCase::Few;
		if ((ni % 100) >= 11 && (ni % 100) <= 19) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Irish
	static EPluralCase GetCardinalPluralCase_34(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 1) return EPluralCase::One;
		if (n == 2) return EPluralCase::Two;
		if (n >= 3 && n <= 6) return EPluralCase::Few;
		if (n >= 7 && n <= 10) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Manx
	static EPluralCase GetCardinalPluralCase_35(double Number)
	{
		int32 i = IntegerValue(Number);
		int32 v = VisibleFractionalDigits(Number, true);
		if ((v == 0) && ((i % 10) == 1)) return EPluralCase::One;
		if ((v == 0) && ((i % 10) == 2)) return EPluralCase::Two;
		if ((v == 0) && (((i % 100) == 0) || ((i % 100) == 20) || ((i % 100) == 40) || ((i % 100) == 60) || ((i % 100) == 80)))
			return EPluralCase::Few;
		if (!(v == 0)) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Cornish
	static EPluralCase GetCardinalPluralCase_36(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (n == 0) return EPluralCase::Zero;
		if (n == 1) return EPluralCase::One;
		if (((ni % 100) == 2) || ((ni % 100) == 22) || ((ni % 100) == 42) || ((ni % 100) == 62) || ((ni % 100) == 82) ||
			(((ni % 1000) == 0) && (((ni % 100000) >= 1000 && (ni % 100000) <= 20000) || ((ni % 100000) == 40000) || ((ni % 100000) == 60000) || ((ni % 100000) == 80000))) ||
			(!(n == 0) && ((ni % 1000000) == 100000)))
			return EPluralCase::Two;
		if (((ni % 100) == 3) || ((ni % 100) == 23) || ((ni % 100) == 43) || ((ni % 100) == 63) || ((ni % 100) == 83))
			return EPluralCase::Few;
		if (!(n == 1) && (((ni % 100) == 1) || ((ni % 100) == 21) || ((ni % 100) == 41) || ((ni % 100) == 61) || ((ni % 100) == 81)))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Arabic, Najdi Arabic
	static EPluralCase GetCardinalPluralCase_37(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (n == 0) return EPluralCase::Zero;
		if (n == 1) return EPluralCase::One;
		if (n == 2) return EPluralCase::Two;
		if ((ni % 100) >= 3 && (ni % 100) <= 10) return EPluralCase::Few;
		if ((ni % 100) >= 11 && (ni % 100) <= 99) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Welsh
	static EPluralCase GetCardinalPluralCase_38(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 0) return EPluralCase::Zero;
		if (n == 1) return EPluralCase::One;
		if (n == 2) return EPluralCase::Two;
		if (n == 3) return EPluralCase::Few;
		if (n == 6) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// ---- Ordinal plural case functions (39-62) ----

	// Most languages: always Other
	static EPluralCase GetOrdinalPluralCase_39(double Number)
	{
		return EPluralCase::Other;
	}

	// Swedish
	static EPluralCase GetOrdinalPluralCase_40(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if ((((ni % 10) == 1) || ((ni % 10) == 2)) && !(((ni % 100) == 11) || ((ni % 100) == 12)))
			return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Filipino, French, Irish, Armenian, Lao, Malay, Romanian, Tagalog, Vietnamese
	static EPluralCase GetOrdinalPluralCase_41(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 1) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Hungarian
	static EPluralCase GetOrdinalPluralCase_42(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 1) || (n == 5)) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Nepali
	static EPluralCase GetOrdinalPluralCase_43(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n >= 1 && n <= 4) return EPluralCase::One;
		return EPluralCase::Other;
	}

	// Belarusian
	static EPluralCase GetOrdinalPluralCase_44(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if ((((ni % 10) == 2) || ((ni % 10) == 3)) && !(((ni % 100) == 12) || ((ni % 100) == 13)))
			return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Ukrainian
	static EPluralCase GetOrdinalPluralCase_45(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (((ni % 10) == 3) && !((ni % 100) == 13)) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Turkmen
	static EPluralCase GetOrdinalPluralCase_46(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if ((((ni % 10) == 6) || ((ni % 10) == 9)) || (n == 10)) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Kazakh
	static EPluralCase GetOrdinalPluralCase_47(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (((ni % 10) == 6) || ((ni % 10) == 9) || (((ni % 10) == 0) && !(n == 0)))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Italian, Sardinian, Sicilian, Venetian
	static EPluralCase GetOrdinalPluralCase_48(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 11) || (n == 8) || (n == 80) || (n == 800)) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Ligurian
	static EPluralCase GetOrdinalPluralCase_49(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 11) || (n == 8) || (n >= 80 && n <= 89) || (n >= 800 && n <= 899))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Georgian
	static EPluralCase GetOrdinalPluralCase_50(double Number)
	{
		int32 i = IntegerValue(Number);
		if (i == 1) return EPluralCase::One;
		if ((i == 0) || (((i % 100) >= 2 && (i % 100) <= 20) || ((i % 100) == 40) || ((i % 100) == 60) || ((i % 100) == 80)))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Albanian
	static EPluralCase GetOrdinalPluralCase_51(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (n == 1) return EPluralCase::One;
		if (((ni % 10) == 4) && !((ni % 100) == 14)) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Cornish
	static EPluralCase GetOrdinalPluralCase_52(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if ((n >= 1 && n <= 4) ||
			(((ni % 100) >= 1 && (ni % 100) <= 4) || ((ni % 100) >= 21 && (ni % 100) <= 24) ||
			 ((ni % 100) >= 41 && (ni % 100) <= 44) || ((ni % 100) >= 61 && (ni % 100) <= 64) ||
			 ((ni % 100) >= 81 && (ni % 100) <= 84)))
			return EPluralCase::One;
		if ((n == 5) || ((ni % 100) == 5)) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// English
	static EPluralCase GetOrdinalPluralCase_53(double Number)
	{
		double n = AbsoluteValue(Number);
		int64 ni = static_cast<int64>(n);
		if (((ni % 10) == 1) && !((ni % 100) == 11)) return EPluralCase::One;
		if (((ni % 10) == 2) && !((ni % 100) == 12)) return EPluralCase::Two;
		if (((ni % 10) == 3) && !((ni % 100) == 13)) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Marathi
	static EPluralCase GetOrdinalPluralCase_54(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 1) return EPluralCase::One;
		if ((n == 2) || (n == 3)) return EPluralCase::Two;
		if (n == 4) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Scottish Gaelic
	static EPluralCase GetOrdinalPluralCase_55(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 1) || (n == 11)) return EPluralCase::One;
		if ((n == 2) || (n == 12)) return EPluralCase::Two;
		if ((n == 3) || (n == 13)) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Catalan
	static EPluralCase GetOrdinalPluralCase_56(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 1) || (n == 3)) return EPluralCase::One;
		if (n == 2) return EPluralCase::Two;
		if (n == 4) return EPluralCase::Few;
		return EPluralCase::Other;
	}

	// Macedonian
	static EPluralCase GetOrdinalPluralCase_57(double Number)
	{
		int32 i = IntegerValue(Number);
		if (((i % 10) == 1) && !((i % 100) == 11)) return EPluralCase::One;
		if (((i % 10) == 2) && !((i % 100) == 12)) return EPluralCase::Two;
		if ((((i % 10) == 7) || ((i % 10) == 8)) && !(((i % 100) == 17) || ((i % 100) == 18)))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Azerbaijani
	static EPluralCase GetOrdinalPluralCase_58(double Number)
	{
		int32 i = IntegerValue(Number);
		if ((((i % 10) == 1) || ((i % 10) == 2) || ((i % 10) == 5) || ((i % 10) == 7) || ((i % 10) == 8)) ||
			(((i % 100) == 20) || ((i % 100) == 50) || ((i % 100) == 70) || ((i % 100) == 80)))
			return EPluralCase::One;
		if ((((i % 10) == 3) || ((i % 10) == 4)) ||
			(((i % 1000) == 100) || ((i % 1000) == 200) || ((i % 1000) == 300) || ((i % 1000) == 400) ||
			 ((i % 1000) == 500) || ((i % 1000) == 600) || ((i % 1000) == 700) || ((i % 1000) == 800) ||
			 ((i % 1000) == 900)))
			return EPluralCase::Few;
		if ((i == 0) || ((i % 10) == 6) || (((i % 100) == 40) || ((i % 100) == 60) || ((i % 100) == 90)))
			return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Gujarati, Hindi
	static EPluralCase GetOrdinalPluralCase_59(double Number)
	{
		double n = AbsoluteValue(Number);
		if (n == 1) return EPluralCase::One;
		if ((n == 2) || (n == 3)) return EPluralCase::Two;
		if (n == 4) return EPluralCase::Few;
		if (n == 6) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Assamese, Bangla
	static EPluralCase GetOrdinalPluralCase_60(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 1) || (n == 5) || (n == 7) || (n == 8) || (n == 9) || (n == 10))
			return EPluralCase::One;
		if ((n == 2) || (n == 3)) return EPluralCase::Two;
		if (n == 4) return EPluralCase::Few;
		if (n == 6) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Odia
	static EPluralCase GetOrdinalPluralCase_61(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 1) || (n == 5) || (n >= 7 && n <= 9)) return EPluralCase::One;
		if ((n == 2) || (n == 3)) return EPluralCase::Two;
		if (n == 4) return EPluralCase::Few;
		if (n == 6) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// Welsh
	static EPluralCase GetOrdinalPluralCase_62(double Number)
	{
		double n = AbsoluteValue(Number);
		if ((n == 0) || (n == 7) || (n == 8) || (n == 9)) return EPluralCase::Zero;
		if (n == 1) return EPluralCase::One;
		if (n == 2) return EPluralCase::Two;
		if ((n == 3) || (n == 4)) return EPluralCase::Few;
		if ((n == 5) || (n == 6)) return EPluralCase::Many;
		return EPluralCase::Other;
	}

	// ---- Dispatch tables ----

	EPluralCase GetCardinalPluralCase(const FString& LanguageCode, double Value)
	{
		// No-plural languages (always Other)
		if (LanguageCode == TEXT("bm") || LanguageCode == TEXT("bo") || LanguageCode == TEXT("dz") ||
			LanguageCode == TEXT("hnj") || LanguageCode == TEXT("id") || LanguageCode == TEXT("ig") ||
			LanguageCode == TEXT("ii") || LanguageCode == TEXT("in") || LanguageCode == TEXT("ja") ||
			LanguageCode == TEXT("jbo") || LanguageCode == TEXT("jv") || LanguageCode == TEXT("jw") ||
			LanguageCode == TEXT("kde") || LanguageCode == TEXT("kea") || LanguageCode == TEXT("km") ||
			LanguageCode == TEXT("ko") || LanguageCode == TEXT("lkt") || LanguageCode == TEXT("lo") ||
			LanguageCode == TEXT("ms") || LanguageCode == TEXT("my") || LanguageCode == TEXT("nqo") ||
			LanguageCode == TEXT("osa") || LanguageCode == TEXT("root") || LanguageCode == TEXT("sah") ||
			LanguageCode == TEXT("ses") || LanguageCode == TEXT("sg") || LanguageCode == TEXT("su") ||
			LanguageCode == TEXT("th") || LanguageCode == TEXT("to") || LanguageCode == TEXT("tpi") ||
			LanguageCode == TEXT("vi") || LanguageCode == TEXT("wo") || LanguageCode == TEXT("yo") ||
			LanguageCode == TEXT("yue") || LanguageCode == TEXT("zh"))
			return GetCardinalPluralCase_0(Value);

		if (LanguageCode == TEXT("am") || LanguageCode == TEXT("as") || LanguageCode == TEXT("bn") ||
			LanguageCode == TEXT("doi") || LanguageCode == TEXT("fa") || LanguageCode == TEXT("gu") ||
			LanguageCode == TEXT("hi") || LanguageCode == TEXT("kn") || LanguageCode == TEXT("pcm") ||
			LanguageCode == TEXT("zu"))
			return GetCardinalPluralCase_1(Value);

		if (LanguageCode == TEXT("ff") || LanguageCode == TEXT("hy") || LanguageCode == TEXT("kab"))
			return GetCardinalPluralCase_2(Value);

		if (LanguageCode == TEXT("ast") || LanguageCode == TEXT("de") || LanguageCode == TEXT("en") ||
			LanguageCode == TEXT("et") || LanguageCode == TEXT("fi") || LanguageCode == TEXT("fy") ||
			LanguageCode == TEXT("gl") || LanguageCode == TEXT("ia") || LanguageCode == TEXT("io") ||
			LanguageCode == TEXT("ji") || LanguageCode == TEXT("lij") || LanguageCode == TEXT("nl") ||
			LanguageCode == TEXT("sc") || LanguageCode == TEXT("scn") || LanguageCode == TEXT("sv") ||
			LanguageCode == TEXT("sw") || LanguageCode == TEXT("ur") || LanguageCode == TEXT("yi"))
			return GetCardinalPluralCase_3(Value);

		if (LanguageCode == TEXT("si")) return GetCardinalPluralCase_4(Value);

		if (LanguageCode == TEXT("ak") || LanguageCode == TEXT("bho") || LanguageCode == TEXT("guw") ||
			LanguageCode == TEXT("ln") || LanguageCode == TEXT("mg") || LanguageCode == TEXT("nso") ||
			LanguageCode == TEXT("pa") || LanguageCode == TEXT("ti") || LanguageCode == TEXT("wa"))
			return GetCardinalPluralCase_5(Value);

		if (LanguageCode == TEXT("tzm")) return GetCardinalPluralCase_6(Value);

		if (LanguageCode == TEXT("af") || LanguageCode == TEXT("an") || LanguageCode == TEXT("asa") ||
			LanguageCode == TEXT("az") || LanguageCode == TEXT("bal") || LanguageCode == TEXT("bem") ||
			LanguageCode == TEXT("bez") || LanguageCode == TEXT("bg") || LanguageCode == TEXT("brx") ||
			LanguageCode == TEXT("ce") || LanguageCode == TEXT("cgg") || LanguageCode == TEXT("chr") ||
			LanguageCode == TEXT("ckb") || LanguageCode == TEXT("dv") || LanguageCode == TEXT("ee") ||
			LanguageCode == TEXT("el") || LanguageCode == TEXT("eo") || LanguageCode == TEXT("eu") ||
			LanguageCode == TEXT("fo") || LanguageCode == TEXT("fur") || LanguageCode == TEXT("gsw") ||
			LanguageCode == TEXT("ha") || LanguageCode == TEXT("haw") || LanguageCode == TEXT("hu") ||
			LanguageCode == TEXT("jgo") || LanguageCode == TEXT("jmc") || LanguageCode == TEXT("ka") ||
			LanguageCode == TEXT("kaj") || LanguageCode == TEXT("kcg") || LanguageCode == TEXT("kk") ||
			LanguageCode == TEXT("kkj") || LanguageCode == TEXT("kl") || LanguageCode == TEXT("ks") ||
			LanguageCode == TEXT("ksb") || LanguageCode == TEXT("ku") || LanguageCode == TEXT("ky") ||
			LanguageCode == TEXT("lb") || LanguageCode == TEXT("lg") || LanguageCode == TEXT("mas") ||
			LanguageCode == TEXT("mgo") || LanguageCode == TEXT("ml") || LanguageCode == TEXT("mn") ||
			LanguageCode == TEXT("mr") || LanguageCode == TEXT("nah") || LanguageCode == TEXT("nb") ||
			LanguageCode == TEXT("nd") || LanguageCode == TEXT("ne") || LanguageCode == TEXT("nn") ||
			LanguageCode == TEXT("nnh") || LanguageCode == TEXT("no") || LanguageCode == TEXT("nr") ||
			LanguageCode == TEXT("ny") || LanguageCode == TEXT("nyn") || LanguageCode == TEXT("om") ||
			LanguageCode == TEXT("or") || LanguageCode == TEXT("os") || LanguageCode == TEXT("pap") ||
			LanguageCode == TEXT("ps") || LanguageCode == TEXT("rm") || LanguageCode == TEXT("rof") ||
			LanguageCode == TEXT("rwk") || LanguageCode == TEXT("saq") || LanguageCode == TEXT("sd") ||
			LanguageCode == TEXT("sdh") || LanguageCode == TEXT("seh") || LanguageCode == TEXT("sn") ||
			LanguageCode == TEXT("so") || LanguageCode == TEXT("sq") || LanguageCode == TEXT("ss") ||
			LanguageCode == TEXT("ssy") || LanguageCode == TEXT("st") || LanguageCode == TEXT("syr") ||
			LanguageCode == TEXT("ta") || LanguageCode == TEXT("te") || LanguageCode == TEXT("teo") ||
			LanguageCode == TEXT("tig") || LanguageCode == TEXT("tk") || LanguageCode == TEXT("tn") ||
			LanguageCode == TEXT("tr") || LanguageCode == TEXT("ts") || LanguageCode == TEXT("ug") ||
			LanguageCode == TEXT("uz") || LanguageCode == TEXT("ve") || LanguageCode == TEXT("vo") ||
			LanguageCode == TEXT("vun") || LanguageCode == TEXT("wae") || LanguageCode == TEXT("xh") ||
			LanguageCode == TEXT("xog"))
			return GetCardinalPluralCase_7(Value);

		if (LanguageCode == TEXT("da")) return GetCardinalPluralCase_8(Value);
		if (LanguageCode == TEXT("is")) return GetCardinalPluralCase_9(Value);
		if (LanguageCode == TEXT("mk")) return GetCardinalPluralCase_10(Value);

		if (LanguageCode == TEXT("ceb") || LanguageCode == TEXT("fil") || LanguageCode == TEXT("tl"))
			return GetCardinalPluralCase_11(Value);

		if (LanguageCode == TEXT("lv") || LanguageCode == TEXT("prg"))
			return GetCardinalPluralCase_12(Value);

		if (LanguageCode == TEXT("lag")) return GetCardinalPluralCase_13(Value);
		if (LanguageCode == TEXT("ksh")) return GetCardinalPluralCase_14(Value);

		if (LanguageCode == TEXT("he") || LanguageCode == TEXT("iw"))
			return GetCardinalPluralCase_15(Value);

		if (LanguageCode == TEXT("iu") || LanguageCode == TEXT("naq") || LanguageCode == TEXT("sat") ||
			LanguageCode == TEXT("se") || LanguageCode == TEXT("sma") || LanguageCode == TEXT("smi") ||
			LanguageCode == TEXT("smj") || LanguageCode == TEXT("smn") || LanguageCode == TEXT("sms"))
			return GetCardinalPluralCase_16(Value);

		if (LanguageCode == TEXT("shi")) return GetCardinalPluralCase_17(Value);

		if (LanguageCode == TEXT("mo") || LanguageCode == TEXT("ro"))
			return GetCardinalPluralCase_18(Value);

		if (LanguageCode == TEXT("bs") || LanguageCode == TEXT("hr") ||
			LanguageCode == TEXT("sh") || LanguageCode == TEXT("sr"))
			return GetCardinalPluralCase_19(Value);

		if (LanguageCode == TEXT("fr")) return GetCardinalPluralCase_20(Value);
		if (LanguageCode == TEXT("pt")) return GetCardinalPluralCase_21(Value);

		if (LanguageCode == TEXT("ca") || LanguageCode == TEXT("it") ||
			LanguageCode == TEXT("pt_PT") || LanguageCode == TEXT("vec"))
			return GetCardinalPluralCase_22(Value);

		if (LanguageCode == TEXT("es")) return GetCardinalPluralCase_23(Value);
		if (LanguageCode == TEXT("gd")) return GetCardinalPluralCase_24(Value);
		if (LanguageCode == TEXT("sl")) return GetCardinalPluralCase_25(Value);

		if (LanguageCode == TEXT("dsb") || LanguageCode == TEXT("hsb"))
			return GetCardinalPluralCase_26(Value);

		if (LanguageCode == TEXT("cs") || LanguageCode == TEXT("sk"))
			return GetCardinalPluralCase_27(Value);

		if (LanguageCode == TEXT("pl")) return GetCardinalPluralCase_28(Value);
		if (LanguageCode == TEXT("be")) return GetCardinalPluralCase_29(Value);
		if (LanguageCode == TEXT("lt")) return GetCardinalPluralCase_30(Value);

		if (LanguageCode == TEXT("ru") || LanguageCode == TEXT("uk"))
			return GetCardinalPluralCase_31(Value);

		if (LanguageCode == TEXT("br")) return GetCardinalPluralCase_32(Value);
		if (LanguageCode == TEXT("mt")) return GetCardinalPluralCase_33(Value);
		if (LanguageCode == TEXT("ga")) return GetCardinalPluralCase_34(Value);
		if (LanguageCode == TEXT("gv")) return GetCardinalPluralCase_35(Value);
		if (LanguageCode == TEXT("kw")) return GetCardinalPluralCase_36(Value);

		if (LanguageCode == TEXT("ar") || LanguageCode == TEXT("ars"))
			return GetCardinalPluralCase_37(Value);

		if (LanguageCode == TEXT("cy")) return GetCardinalPluralCase_38(Value);

		return EPluralCase::Other;
	}

	EPluralCase GetOrdinalPluralCase(const FString& LanguageCode, double Value)
	{
		// Most languages: always Other
		if (LanguageCode == TEXT("af") || LanguageCode == TEXT("am") || LanguageCode == TEXT("an") ||
			LanguageCode == TEXT("ar") || LanguageCode == TEXT("ast") || LanguageCode == TEXT("bg") ||
			LanguageCode == TEXT("bs") || LanguageCode == TEXT("ce") || LanguageCode == TEXT("cs") ||
			LanguageCode == TEXT("da") || LanguageCode == TEXT("de") || LanguageCode == TEXT("dsb") ||
			LanguageCode == TEXT("el") || LanguageCode == TEXT("es") || LanguageCode == TEXT("et") ||
			LanguageCode == TEXT("eu") || LanguageCode == TEXT("fa") || LanguageCode == TEXT("fi") ||
			LanguageCode == TEXT("fy") || LanguageCode == TEXT("gl") || LanguageCode == TEXT("gsw") ||
			LanguageCode == TEXT("he") || LanguageCode == TEXT("hr") || LanguageCode == TEXT("hsb") ||
			LanguageCode == TEXT("ia") || LanguageCode == TEXT("id") || LanguageCode == TEXT("in") ||
			LanguageCode == TEXT("is") || LanguageCode == TEXT("iw") || LanguageCode == TEXT("ja") ||
			LanguageCode == TEXT("km") || LanguageCode == TEXT("kn") || LanguageCode == TEXT("ko") ||
			LanguageCode == TEXT("ky") || LanguageCode == TEXT("lt") || LanguageCode == TEXT("lv") ||
			LanguageCode == TEXT("ml") || LanguageCode == TEXT("mn") || LanguageCode == TEXT("my") ||
			LanguageCode == TEXT("nb") || LanguageCode == TEXT("nl") || LanguageCode == TEXT("no") ||
			LanguageCode == TEXT("pa") || LanguageCode == TEXT("pl") || LanguageCode == TEXT("prg") ||
			LanguageCode == TEXT("ps") || LanguageCode == TEXT("pt") || LanguageCode == TEXT("root") ||
			LanguageCode == TEXT("ru") || LanguageCode == TEXT("sd") || LanguageCode == TEXT("sh") ||
			LanguageCode == TEXT("si") || LanguageCode == TEXT("sk") || LanguageCode == TEXT("sl") ||
			LanguageCode == TEXT("sr") || LanguageCode == TEXT("sw") || LanguageCode == TEXT("ta") ||
			LanguageCode == TEXT("te") || LanguageCode == TEXT("th") || LanguageCode == TEXT("tpi") ||
			LanguageCode == TEXT("tr") || LanguageCode == TEXT("ur") || LanguageCode == TEXT("uz") ||
			LanguageCode == TEXT("yue") || LanguageCode == TEXT("zh") || LanguageCode == TEXT("zu"))
			return GetOrdinalPluralCase_39(Value);

		if (LanguageCode == TEXT("sv")) return GetOrdinalPluralCase_40(Value);

		if (LanguageCode == TEXT("bal") || LanguageCode == TEXT("fil") || LanguageCode == TEXT("fr") ||
			LanguageCode == TEXT("ga") || LanguageCode == TEXT("hy") || LanguageCode == TEXT("lo") ||
			LanguageCode == TEXT("mo") || LanguageCode == TEXT("ms") || LanguageCode == TEXT("ro") ||
			LanguageCode == TEXT("tl") || LanguageCode == TEXT("vi"))
			return GetOrdinalPluralCase_41(Value);

		if (LanguageCode == TEXT("hu")) return GetOrdinalPluralCase_42(Value);
		if (LanguageCode == TEXT("ne")) return GetOrdinalPluralCase_43(Value);
		if (LanguageCode == TEXT("be")) return GetOrdinalPluralCase_44(Value);
		if (LanguageCode == TEXT("uk")) return GetOrdinalPluralCase_45(Value);
		if (LanguageCode == TEXT("tk")) return GetOrdinalPluralCase_46(Value);
		if (LanguageCode == TEXT("kk")) return GetOrdinalPluralCase_47(Value);

		if (LanguageCode == TEXT("it") || LanguageCode == TEXT("sc") ||
			LanguageCode == TEXT("scn") || LanguageCode == TEXT("vec"))
			return GetOrdinalPluralCase_48(Value);

		if (LanguageCode == TEXT("lij")) return GetOrdinalPluralCase_49(Value);
		if (LanguageCode == TEXT("ka")) return GetOrdinalPluralCase_50(Value);
		if (LanguageCode == TEXT("sq")) return GetOrdinalPluralCase_51(Value);
		if (LanguageCode == TEXT("kw")) return GetOrdinalPluralCase_52(Value);
		if (LanguageCode == TEXT("en")) return GetOrdinalPluralCase_53(Value);
		if (LanguageCode == TEXT("mr")) return GetOrdinalPluralCase_54(Value);
		if (LanguageCode == TEXT("gd")) return GetOrdinalPluralCase_55(Value);
		if (LanguageCode == TEXT("ca")) return GetOrdinalPluralCase_56(Value);
		if (LanguageCode == TEXT("mk")) return GetOrdinalPluralCase_57(Value);
		if (LanguageCode == TEXT("az")) return GetOrdinalPluralCase_58(Value);

		if (LanguageCode == TEXT("gu") || LanguageCode == TEXT("hi"))
			return GetOrdinalPluralCase_59(Value);

		if (LanguageCode == TEXT("as") || LanguageCode == TEXT("bn"))
			return GetOrdinalPluralCase_60(Value);

		if (LanguageCode == TEXT("or")) return GetOrdinalPluralCase_61(Value);
		if (LanguageCode == TEXT("cy")) return GetOrdinalPluralCase_62(Value);

		return EPluralCase::Other;
	}

	FString PluralCaseToString(EPluralCase Case)
	{
		switch (Case)
		{
		case EPluralCase::Zero: return TEXT("ZERO");
		case EPluralCase::One: return TEXT("ONE");
		case EPluralCase::Two: return TEXT("TWO");
		case EPluralCase::Few: return TEXT("FEW");
		case EPluralCase::Many: return TEXT("MANY");
		case EPluralCase::Other: return TEXT("OTHER");
		default: return TEXT("OTHER");
		}
	}
}

// ============================================================================
// Markup Parser
// ============================================================================

// Maximum nesting depth for markup tags
static constexpr int32 MaxMarkupNestingDepth = 50;

// Special attribute names
static const FString NoMarkupAttribute = TEXT("nomarkup");
static const FString CharacterAttribute = TEXT("character");
static const FString TrimWhitespaceProperty = TEXT("trimwhitespace");

// Built-in replacement attribute names
static const FString SelectAttribute = TEXT("select");
static const FString PluralAttribute = TEXT("plural");
static const FString OrdinalAttribute = TEXT("ordinal");

// Internal struct to track open markup tags during parsing
struct FMarkupOpenTag
{
	FString Name;
	int32 Position;
	int32 SourcePosition = -1;
	TMap<FString, FYarnMarkupValue> Properties;
	bool bIsSelfClosing = false;
	bool bTrimWhitespace = false;

	// For adoption agency: tracking ID for split attributes
	int32 TrackingID = -1;
};

// Detect the type of a property value string and create a typed FYarnMarkupValue.
// Tries int, then float, then bool, then defaults to string.
static FYarnMarkupValue DetectPropertyValueType(const FString& RawValue)
{
	// Try integer first
	if (!RawValue.IsEmpty())
	{
		bool bAllDigits = true;
		int32 StartIdx = 0;
		if (RawValue[0] == TEXT('-') && RawValue.Len() > 1)
		{
			StartIdx = 1;
		}
		for (int32 c = StartIdx; c < RawValue.Len(); c++)
		{
			if (!FChar::IsDigit(RawValue[c]))
			{
				bAllDigits = false;
				break;
			}
		}
		if (bAllDigits && RawValue.Len() > StartIdx)
		{
			int32 IntVal = FCString::Atoi(*RawValue);
			return FYarnMarkupValue::MakeInteger(IntVal);
		}

		// Try float (contains digits and a decimal point)
		bool bHasDot = false;
		bool bIsNumber = true;
		StartIdx = 0;
		if (RawValue[0] == TEXT('-') && RawValue.Len() > 1)
		{
			StartIdx = 1;
		}
		for (int32 c = StartIdx; c < RawValue.Len(); c++)
		{
			if (RawValue[c] == TEXT('.'))
			{
				if (bHasDot) { bIsNumber = false; break; }
				bHasDot = true;
			}
			else if (!FChar::IsDigit(RawValue[c]))
			{
				bIsNumber = false;
				break;
			}
		}
		if (bIsNumber && bHasDot && RawValue.Len() > StartIdx)
		{
			float FloatVal = FCString::Atof(*RawValue);
			return FYarnMarkupValue::MakeFloat(FloatVal);
		}
	}

	// Try boolean (case-insensitive)
	if (RawValue.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		return FYarnMarkupValue::MakeBool(true);
	}
	if (RawValue.Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		return FYarnMarkupValue::MakeBool(false);
	}

	// Default to string
	return FYarnMarkupValue::MakeString(RawValue);
}

// Parse properties from a tag content string (after the tag name)
static void ParseTagProperties(const FString& PropsStr, TMap<FString, FYarnMarkupValue>& OutProperties)
{
	if (PropsStr.IsEmpty())
	{
		return;
	}

	FString CurrentPropName;
	FString CurrentPropValue;
	bool bInQuotes = false;
	bool bWasQuoted = false;
	bool bEscapeNext = false;
	bool bParsingValue = false;

	auto CommitProperty = [&]()
	{
		if (!CurrentPropName.IsEmpty())
		{
			FString TrimmedValue = CurrentPropValue.TrimStartAndEnd();
			if (bWasQuoted)
			{
				// Quoted values are always strings
				OutProperties.Add(CurrentPropName.TrimStartAndEnd(), FYarnMarkupValue::MakeString(TrimmedValue));
			}
			else
			{
				// Unquoted values get type detection
				OutProperties.Add(CurrentPropName.TrimStartAndEnd(), DetectPropertyValueType(TrimmedValue));
			}
		}
		CurrentPropName.Empty();
		CurrentPropValue.Empty();
		bParsingValue = false;
		bWasQuoted = false;
	};

	for (int32 p = 0; p < PropsStr.Len(); p++)
	{
		TCHAR C = PropsStr[p];

		if (bEscapeNext)
		{
			bEscapeNext = false;
			if (bParsingValue)
				CurrentPropValue.AppendChar(C);
			else
				CurrentPropName.AppendChar(C);
			continue;
		}

		if (C == TEXT('\\') && bInQuotes)
		{
			bEscapeNext = true;
			continue;
		}

		if (C == TEXT('"'))
		{
			bInQuotes = !bInQuotes;
			if (bParsingValue)
			{
				bWasQuoted = true;
			}
		}
		else if (C == TEXT('=') && !bInQuotes && !bParsingValue)
		{
			bParsingValue = true;
		}
		else if (C == TEXT(' ') && !bInQuotes)
		{
			CommitProperty();
		}
		else
		{
			if (bParsingValue)
				CurrentPropValue.AppendChar(C);
			else
				CurrentPropName.AppendChar(C);
		}
	}

	// Commit final property
	CommitProperty();
}

// Process select/plural/ordinal built-in replacement markers
// Returns true if the marker was processed, with replacement text in OutText
static bool ProcessBuiltInReplacement(
	const FString& MarkerName,
	const TMap<FString, FYarnMarkupValue>& Properties,
	const FString& LocaleCode,
	FString& OutText)
{
	// All three require a "value" property.
	// The value can come from an explicit "value" property, or from shorthand syntax
	// where [select=male ...] stores the value as the "select" property.
	const FYarnMarkupValue* ValueProp = Properties.Find(TEXT("value"));
	if (!ValueProp)
	{
		// Try shorthand: property with same name as the marker (e.g., "select" for [select=male ...])
		ValueProp = Properties.Find(MarkerName);
	}
	if (!ValueProp)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Markup: [%s] marker missing required 'value' property"), *MarkerName);
		return false;
	}

	FString ValueStr = ValueProp->ToString();

	if (MarkerName.Equals(SelectAttribute, ESearchCase::CaseSensitive))
	{
		// [select value=x 1=one 2=two/] - look up property matching the value
		const FYarnMarkupValue* Replacement = Properties.Find(ValueStr);
		if (!Replacement)
		{
			UE_LOG(LogYarnSpinner, Warning, TEXT("Markup: [select] no property found for value '%s'"), *ValueStr);
			OutText = ValueStr;
			return true;
		}

		FString ReplacementStr = Replacement->ToString();

		// Replace % with the value (but not \% which is a literal %)
		OutText = ReplacementStr;
		// First, temporarily replace \% with a placeholder
		static const FString EscapedPercent = TEXT("\x01ESCAPED_PERCENT\x01");
		OutText = OutText.Replace(TEXT("\\%"), *EscapedPercent);
		// Replace unescaped % with the value
		OutText = OutText.Replace(TEXT("%"), *ValueStr);
		// Restore escaped percent as literal \%
		OutText = OutText.Replace(*EscapedPercent, TEXT("\\%"));
		return true;
	}

	if (MarkerName.Equals(PluralAttribute, ESearchCase::CaseSensitive) ||
		MarkerName.Equals(OrdinalAttribute, ESearchCase::CaseSensitive))
	{
		double NumericValue = FCString::Atod(*ValueStr);

		// Resolve locale to neutral culture (e.g., "en-AU" -> "en")
		FString LangCode = LocaleCode;
		int32 DashIdx;
		if (LangCode.FindChar(TEXT('-'), DashIdx))
		{
			LangCode = LangCode.Left(DashIdx);
		}

		// Get the plural case
		YarnPlurals::EPluralCase PluralCase;
		if (MarkerName.Equals(PluralAttribute, ESearchCase::CaseSensitive))
		{
			PluralCase = YarnPlurals::GetCardinalPluralCase(LangCode, NumericValue);
		}
		else
		{
			PluralCase = YarnPlurals::GetOrdinalPluralCase(LangCode, NumericValue);
		}

		FString CaseName = YarnPlurals::PluralCaseToString(PluralCase);

		// Look up the property with the plural case name (case-insensitive)
		FString ReplacementStr;
		bool bFoundReplacement = false;
		for (const auto& Pair : Properties)
		{
			if (Pair.Key.Equals(CaseName, ESearchCase::IgnoreCase))
			{
				ReplacementStr = Pair.Value.ToString();
				bFoundReplacement = true;
				break;
			}
		}

		if (!bFoundReplacement)
		{
			// Fall back to OTHER
			for (const auto& Pair : Properties)
			{
				if (Pair.Key.Equals(TEXT("OTHER"), ESearchCase::IgnoreCase))
				{
					ReplacementStr = Pair.Value.ToString();
					bFoundReplacement = true;
					break;
				}
			}
		}

		if (!bFoundReplacement)
		{
			UE_LOG(LogYarnSpinner, Warning, TEXT("Markup: [%s] no property found for plural case '%s'"), *MarkerName, *CaseName);
			OutText = ValueStr;
			return true;
		}

		// Replace % with the numeric value (formatted with current culture)
		// but not \% which is a literal %
		FString FormattedValue;
		if (FMath::IsNearlyEqual(FMath::Frac(NumericValue), 0.0f))
		{
			FormattedValue = FString::Printf(TEXT("%d"), static_cast<int32>(NumericValue));
		}
		else
		{
			FormattedValue = FString::SanitizeFloat(NumericValue);
		}
		OutText = ReplacementStr;
		// First, temporarily replace \% with a placeholder
		static const FString EscapedPercent2 = TEXT("\x01ESCAPED_PERCENT\x01");
		OutText = OutText.Replace(TEXT("\\%"), *EscapedPercent2);
		// Replace unescaped % with the formatted value
		OutText = OutText.Replace(TEXT("%"), *FormattedValue);
		// Restore escaped percent as literal \%
		OutText = OutText.Replace(*EscapedPercent2, TEXT("\\%"));
		return true;
	}

	return false;
}

FYarnMarkupParseResult UYarnMarkupLibrary::ParseMarkup(const FString& Text)
{
	return ParseMarkupFull(Text, TEXT("en"), true);
}

FYarnMarkupParseResult UYarnMarkupLibrary::ParseMarkupFull(const FString& Text, const FString& LocaleCode, bool bAddImplicitCharacterAttribute)
{
	FYarnMarkupParseResult Result;

	if (Text.IsEmpty())
	{
		return Result;
	}

	// ========================================================================
	// Phase 1: Parse markup tags into plain text and attributes
	// Handles escape sequences, close-all [/], nomarkup, self-closing tags
	// with whitespace trimming, adoption agency for misnested tags, and
	// built-in replacement markers (select/plural/ordinal)
	// ========================================================================

	FString PlainText;
	TArray<FMarkupOpenTag> OpenTags;
	int32 NextTrackingID = 0;

	// Track whether the previous sibling had trimwhitespace=true
	bool bTrimNextWhitespace = false;

	int32 i = 0;
	while (i < Text.Len())
	{
		TCHAR C = Text[i];

		// ---- Escape sequences ----
		// \[ becomes [ in the output, \] becomes ]
		if (C == TEXT('\\') && i + 1 < Text.Len())
		{
			TCHAR NextChar = Text[i + 1];
			if (NextChar == TEXT('[') || NextChar == TEXT(']'))
			{
				PlainText.AppendChar(NextChar);
				bTrimNextWhitespace = false;
				i += 2;
				continue;
			}
		}

		// ---- Tag start ----
		if (C == TEXT('['))
		{
			// Track the position of '[' in the original source text
			int32 TagSourcePosition = i;

			// Find the end of the tag
			int32 TagEnd = INDEX_NONE;
			bool bInTagQuotes = false;
			for (int32 j = i + 1; j < Text.Len(); j++)
			{
				if (Text[j] == TEXT('"'))
				{
					bInTagQuotes = !bInTagQuotes;
				}
				else if (Text[j] == TEXT(']') && !bInTagQuotes)
				{
					TagEnd = j;
					break;
				}
			}

			if (TagEnd == INDEX_NONE)
			{
				// No closing bracket, treat as literal
				PlainText.AppendChar(C);
				bTrimNextWhitespace = false;
				i++;
				continue;
			}

			FString TagContent = Text.Mid(i + 1, TagEnd - i - 1);

			// ---- Close-all [/] ----
			if (TagContent == TEXT("/"))
			{
				// Close all open tags by creating attributes for each
				for (int32 j = OpenTags.Num() - 1; j >= 0; j--)
				{
					FYarnMarkupAttribute Attr;
					Attr.Name = OpenTags[j].Name;
					Attr.Position = OpenTags[j].Position;
					Attr.SourcePosition = OpenTags[j].SourcePosition;
					Attr.Length = PlainText.Len() - OpenTags[j].Position;
					Attr.Properties = OpenTags[j].Properties;
					if (OpenTags[j].TrackingID >= 0)
					{
						Attr.Properties.Add(TEXT("_splitID"), FYarnMarkupValue::MakeString(FString::FromInt(OpenTags[j].TrackingID)));
					}
					Result.Attributes.Add(Attr);
				}
				OpenTags.Empty();
				i = TagEnd + 1;
				continue;
			}

			// ---- Closing tag [/name] ----
			if (TagContent.StartsWith(TEXT("/")))
			{
				FString ClosingTagName = TagContent.Mid(1).TrimStartAndEnd();

				// Find matching open tag (search from most recent)
				int32 MatchIndex = -1;
				for (int32 j = OpenTags.Num() - 1; j >= 0; j--)
				{
					if (OpenTags[j].Name.Equals(ClosingTagName, ESearchCase::CaseSensitive))
					{
						MatchIndex = j;
						break;
					}
				}

				if (MatchIndex >= 0)
				{
					// ---- Adoption agency algorithm ----
					// If there are open tags between the match and the top of the stack,
					// they are "orphans" that need to be re-opened after this close.
					TArray<FMarkupOpenTag> Orphans;

					for (int32 j = OpenTags.Num() - 1; j > MatchIndex; j--)
					{
						// Close the orphaned tag at current position
						FYarnMarkupAttribute OrphanAttr;
						OrphanAttr.Name = OpenTags[j].Name;
						OrphanAttr.Position = OpenTags[j].Position;
						OrphanAttr.SourcePosition = OpenTags[j].SourcePosition;
						OrphanAttr.Length = PlainText.Len() - OpenTags[j].Position;
						OrphanAttr.Properties = OpenTags[j].Properties;

						// Assign tracking ID if not already set
						if (OpenTags[j].TrackingID < 0)
						{
							OpenTags[j].TrackingID = NextTrackingID++;
						}
						OrphanAttr.Properties.Add(TEXT("_splitID"), FYarnMarkupValue::MakeString(FString::FromInt(OpenTags[j].TrackingID)));

						Result.Attributes.Add(OrphanAttr);

						// Save for re-opening
						Orphans.Add(OpenTags[j]);
					}

					// Close the matched tag
					FYarnMarkupAttribute Attr;
					Attr.Name = OpenTags[MatchIndex].Name;
					Attr.Position = OpenTags[MatchIndex].Position;
					Attr.SourcePosition = OpenTags[MatchIndex].SourcePosition;
					Attr.Length = PlainText.Len() - OpenTags[MatchIndex].Position;
					Attr.Properties = OpenTags[MatchIndex].Properties;
					if (OpenTags[MatchIndex].TrackingID >= 0)
					{
						Attr.Properties.Add(TEXT("_splitID"), FYarnMarkupValue::MakeString(FString::FromInt(OpenTags[MatchIndex].TrackingID)));
					}
					Result.Attributes.Add(Attr);

					// Remove the matched tag and everything above it
					OpenTags.RemoveAt(MatchIndex, OpenTags.Num() - MatchIndex);

					// Re-open orphaned tags (adoption agency)
					for (int32 j = Orphans.Num() - 1; j >= 0; j--)
					{
						FMarkupOpenTag Reopened;
						Reopened.Name = Orphans[j].Name;
						Reopened.Position = PlainText.Len();
						Reopened.SourcePosition = Orphans[j].SourcePosition;
						Reopened.Properties = Orphans[j].Properties;
						Reopened.TrackingID = Orphans[j].TrackingID;
						OpenTags.Add(Reopened);
					}
				}

				i = TagEnd + 1;
				continue;
			}

			// ---- Opening tag ----
			FString TagName;
			TMap<FString, FYarnMarkupValue> Properties;
			bool bSelfClosing = false;

			// Check if tag content ends with / (self-closing before property parsing)
			FString TrimmedContent = TagContent.TrimStartAndEnd();

			// Check if last non-whitespace char is /
			if (TrimmedContent.EndsWith(TEXT("/")))
			{
				bSelfClosing = true;
				TrimmedContent = TrimmedContent.LeftChop(1).TrimEnd();
			}

			// Split by space for properties
			int32 SpaceIdx = TrimmedContent.Find(TEXT(" "));
			if (SpaceIdx != INDEX_NONE)
			{
				TagName = TrimmedContent.Left(SpaceIdx).TrimStartAndEnd();
				FString PropsStr = TrimmedContent.Mid(SpaceIdx + 1);

				// Check if props end with / (self-closing with properties)
				if (PropsStr.TrimEnd().EndsWith(TEXT("/")))
				{
					bSelfClosing = true;
					PropsStr = PropsStr.TrimEnd().LeftChop(1);
				}

				ParseTagProperties(PropsStr, Properties);
			}
			else
			{
				TagName = TrimmedContent;
			}

			// Handle shorthand property syntax: [tagname=value ...]
			// If TagName contains '=', split it into the real tag name and a shorthand property.
			// E.g., [select=male ...] -> TagName="select", Property "select"="male"
			{
				int32 EqualsIdx;
				if (TagName.FindChar(TEXT('='), EqualsIdx))
				{
					FString ShorthandValue = TagName.Mid(EqualsIdx + 1);
					TagName = TagName.Left(EqualsIdx);
					// Add the shorthand property (tagname=value)
					Properties.Add(TagName, DetectPropertyValueType(ShorthandValue));
				}
			}

			// ---- Nomarkup mode ----
			if (TagName.Equals(NoMarkupAttribute, ESearchCase::CaseSensitive) && !bSelfClosing)
			{
				// Find [/nomarkup] closing tag
				FString CloseTag = TEXT("[/nomarkup]");
				int32 CloseIdx = Text.Find(CloseTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, TagEnd + 1);

				if (CloseIdx != INDEX_NONE)
				{
					// Everything between [nomarkup] and [/nomarkup] is literal text
					FString LiteralText = Text.Mid(TagEnd + 1, CloseIdx - TagEnd - 1);
					int32 StartPos = PlainText.Len();
					PlainText += LiteralText;

					// Add nomarkup as an attribute covering the literal text
					FYarnMarkupAttribute Attr;
					Attr.Name = NoMarkupAttribute;
					Attr.Position = StartPos;
					Attr.SourcePosition = TagSourcePosition;
					Attr.Length = LiteralText.Len();
					Result.Attributes.Add(Attr);

					i = CloseIdx + CloseTag.Len();
				}
				else
				{
					// No closing [/nomarkup] - treat rest as literal
					FString LiteralText = Text.Mid(TagEnd + 1);
					int32 StartPos = PlainText.Len();
					PlainText += LiteralText;

					FYarnMarkupAttribute Attr;
					Attr.Name = NoMarkupAttribute;
					Attr.Position = StartPos;
					Attr.SourcePosition = TagSourcePosition;
					Attr.Length = LiteralText.Len();
					Result.Attributes.Add(Attr);

					i = Text.Len();
				}
				bTrimNextWhitespace = false;
				continue;
			}

			// ---- Built-in replacement markers (select/plural/ordinal) ----
			if (bSelfClosing && (
				TagName.Equals(SelectAttribute, ESearchCase::CaseSensitive) ||
				TagName.Equals(PluralAttribute, ESearchCase::CaseSensitive) ||
				TagName.Equals(OrdinalAttribute, ESearchCase::CaseSensitive)))
			{
				FString ReplacementText;
				if (ProcessBuiltInReplacement(TagName, Properties, LocaleCode, ReplacementText))
				{
					PlainText += ReplacementText;
					bTrimNextWhitespace = false;
					i = TagEnd + 1;
					continue;
				}
			}

			if (bSelfClosing)
			{
				// Self-closing tag - add attribute at current position with length 0
				FYarnMarkupAttribute Attr;
				Attr.Name = TagName;
				Attr.Position = PlainText.Len();
				Attr.SourcePosition = TagSourcePosition;
				Attr.Length = 0;
				Attr.Properties = Properties;
				Result.Attributes.Add(Attr);

				// Self-closing tags implicitly have trimwhitespace=true
				// unless explicitly set to false
				const FYarnMarkupValue* TrimProp = Properties.Find(TrimWhitespaceProperty);
				if (TrimProp && TrimProp->ToString().Equals(TEXT("false"), ESearchCase::IgnoreCase))
				{
					bTrimNextWhitespace = false;
				}
				else
				{
					bTrimNextWhitespace = true;
				}
			}
			else
			{
				// Check nesting depth
				if (OpenTags.Num() >= MaxMarkupNestingDepth)
				{
					UE_LOG(LogYarnSpinner, Warning, TEXT("Markup nesting depth exceeded maximum of %d - ignoring tag [%s]"), MaxMarkupNestingDepth, *TagName);
					i = TagEnd + 1;
					continue;
				}

				// Record open tag
				FMarkupOpenTag OpenTag;
				OpenTag.Name = TagName;
				OpenTag.Position = PlainText.Len();
				OpenTag.SourcePosition = TagSourcePosition;
				OpenTag.Properties = Properties;
				OpenTags.Add(OpenTag);
				bTrimNextWhitespace = false;
			}

			i = TagEnd + 1;
		}
		else
		{
			// Regular text character
			// Apply whitespace trimming if the previous sibling was self-closing
			if (bTrimNextWhitespace && FChar::IsWhitespace(C))
			{
				bTrimNextWhitespace = false;
				i++;
				continue; // skip one whitespace character
			}
			bTrimNextWhitespace = false;

			PlainText.AppendChar(C);
			i++;
		}
	}

	// Close any remaining open tags at end of text
	for (int32 j = OpenTags.Num() - 1; j >= 0; j--)
	{
		FYarnMarkupAttribute Attr;
		Attr.Name = OpenTags[j].Name;
		Attr.Position = OpenTags[j].Position;
		Attr.SourcePosition = OpenTags[j].SourcePosition;
		Attr.Length = PlainText.Len() - OpenTags[j].Position;
		Attr.Properties = OpenTags[j].Properties;
		if (OpenTags[j].TrackingID >= 0)
		{
			Attr.Properties.Add(TEXT("_splitID"), FYarnMarkupValue::MakeString(FString::FromInt(OpenTags[j].TrackingID)));
		}
		Result.Attributes.Add(Attr);
	}

	// ========================================================================
	// Phase 2: Merge split attributes (from adoption agency)
	// Attributes with the same _splitID are merged: lowest position, summed lengths
	// ========================================================================
	{
		TMap<int32, int32> SplitIDToAttrIndex; // splitID -> index in Result.Attributes

		for (int32 j = 0; j < Result.Attributes.Num(); j++)
		{
			const FYarnMarkupValue* SplitIDVal = Result.Attributes[j].Properties.Find(TEXT("_splitID"));
			if (SplitIDVal)
			{
				int32 SplitID = FCString::Atoi(*SplitIDVal->ToString());

				if (int32* ExistingIdx = SplitIDToAttrIndex.Find(SplitID))
				{
					// Merge into existing by summing lengths
					FYarnMarkupAttribute& Existing = Result.Attributes[*ExistingIdx];
					Existing.Length += Result.Attributes[j].Length;
					// Keep the minimum position
					if (Result.Attributes[j].Position < Existing.Position)
					{
						Existing.Position = Result.Attributes[j].Position;
					}

					// Mark for removal
					Result.Attributes[j].Name = TEXT(""); // will be cleaned up below
				}
				else
				{
					SplitIDToAttrIndex.Add(SplitID, j);
				}
			}
		}

		// Remove merged duplicates and _splitID properties
		for (int32 j = Result.Attributes.Num() - 1; j >= 0; j--)
		{
			if (Result.Attributes[j].Name.IsEmpty())
			{
				Result.Attributes.RemoveAt(j);
			}
			else
			{
				Result.Attributes[j].Properties.Remove(TEXT("_splitID"));
			}
		}
	}

	// ========================================================================
	// Phase 3: Sort attributes by position (ascending)
	// ========================================================================
	Result.Attributes.Sort([](const FYarnMarkupAttribute& A, const FYarnMarkupAttribute& B)
	{
		return A.Position < B.Position;
	});

	// ========================================================================
	// Phase 4: Character name extraction
	// Implicit character attribute: "Name: text" pattern
	// Only if no explicit "character" attribute exists
	// ========================================================================
	if (bAddImplicitCharacterAttribute)
	{
		// Check if there's already a character attribute
		bool bHasCharacterAttr = false;
		for (const FYarnMarkupAttribute& Attr : Result.Attributes)
		{
			if (Attr.Name.Equals(CharacterAttribute, ESearchCase::CaseSensitive))
			{
				bHasCharacterAttr = true;
				// Extract character name from the attribute
				Result.CharacterName = Attr.GetProperty(TEXT("name"));
				if (Attr.Length > 0)
				{
					Result.TextWithoutCharacterName = PlainText.Mid(Attr.Length);
				}
				else
				{
					Result.TextWithoutCharacterName = PlainText;
				}
				break;
			}
		}

		if (!bHasCharacterAttr)
		{
			// Match "Name: " at the start of text, handling escaped colons (\:)
			FRegexPattern Pattern(TEXT("^((?:[^:\\\\]|\\\\.)*?(?<!\\\\)):\\s*"));
			FRegexMatcher Matcher(Pattern, PlainText);

			if (Matcher.FindNext())
			{
				FString Match = PlainText.Mid(Matcher.GetMatchBeginning(), Matcher.GetMatchEnding() - Matcher.GetMatchBeginning());
				int32 MatchLen = Match.Len();

				// Extract character name from capture group 1
				FString CharName = PlainText.Mid(Matcher.GetCaptureGroupBeginning(1), Matcher.GetCaptureGroupEnding(1) - Matcher.GetCaptureGroupBeginning(1));
				CharName.TrimStartAndEndInline();

				if (!CharName.IsEmpty())
				{
					Result.CharacterName = CharName;

					// Add implicit character attribute at position 0
					FYarnMarkupAttribute CharAttr;
					CharAttr.Name = CharacterAttribute;
					CharAttr.Position = 0;
					CharAttr.SourcePosition = -1;
					CharAttr.Length = MatchLen;
					CharAttr.Properties.Add(TEXT("name"), FYarnMarkupValue::MakeString(CharName));
					Result.Attributes.Insert(CharAttr, 0); // Insert at beginning

					Result.TextWithoutCharacterName = PlainText.Mid(MatchLen);
				}
				else
				{
					Result.TextWithoutCharacterName = PlainText;
				}
			}
			else
			{
				Result.TextWithoutCharacterName = PlainText;
			}
		}
	}
	else
	{
		Result.TextWithoutCharacterName = PlainText;
	}

	// Unescape \: to : now that character detection is done
	PlainText = PlainText.Replace(TEXT("\\:"), TEXT(":"));
	Result.TextWithoutCharacterName = Result.TextWithoutCharacterName.Replace(TEXT("\\:"), TEXT(":"));
	if (!Result.CharacterName.IsEmpty())
	{
		Result.CharacterName = Result.CharacterName.Replace(TEXT("\\:"), TEXT(":"));
	}

	Result.Text = PlainText;
	return Result;
}

// ============================================================================
// UYarnMarkupLibrary utilities
// ============================================================================

FString UYarnMarkupLibrary::ApplyMarkupProcessors(
	const FYarnMarkupParseResult& ParseResult,
	const TArray<TScriptInterface<IYarnMarkupProcessor>>& Processors,
	const FString& LocaleCode)
{
	if (Processors.Num() == 0)
	{
		return ParseResult.Text;
	}

	FString ResultText = ParseResult.Text;
	TArray<FYarnMarkupAttribute> Attributes = ParseResult.Attributes;

	// Sort attributes by position (reverse order to process from end to start)
	Attributes.Sort([](const FYarnMarkupAttribute& A, const FYarnMarkupAttribute& B)
	{
		return A.Position > B.Position;
	});

	// Process each attribute
	for (const FYarnMarkupAttribute& Attr : Attributes)
	{
		// Find a processor for this attribute
		for (const TScriptInterface<IYarnMarkupProcessor>& Processor : Processors)
		{
			if (!Processor.GetInterface())
			{
				continue;
			}

			// Extract the substring for this attribute
			FString SubText = ResultText.Mid(Attr.Position, Attr.Length);
			TArray<FYarnMarkupAttribute> ChildAttrs;

			FYarnMarkupReplacementResult ProcResult = IYarnMarkupProcessor::Execute_ProcessMarkup(
				Processor.GetObject(),
				Attr,
				SubText,
				ChildAttrs,
				LocaleCode
			);

			// If the processor modified the text, update the result
			if (!SubText.Equals(ResultText.Mid(Attr.Position, Attr.Length)))
			{
				ResultText = ResultText.Left(Attr.Position) + SubText + ResultText.Mid(Attr.Position + Attr.Length);
				break; // Only one processor per attribute
			}
		}
	}

	return ResultText;
}

FString UYarnMarkupLibrary::ColorToHexString(const FLinearColor& Color)
{
	FColor SRGBColor = Color.ToFColor(true);
	return FString::Printf(TEXT("#%02X%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B, SRGBColor.A);
}

FString UYarnMarkupLibrary::EscapeRichText(const FString& Text)
{
	FString Result = Text;
	Result = Result.Replace(TEXT("<"), TEXT("&lt;"));
	Result = Result.Replace(TEXT(">"), TEXT("&gt;"));
	return Result;
}

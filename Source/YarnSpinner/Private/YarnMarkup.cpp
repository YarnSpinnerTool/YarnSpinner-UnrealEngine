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
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

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

		FCulturePtr Culture = FInternationalization::Get().GetCulture(LocaleCode);
		if (!Culture.IsValid())
		{
			Culture = FInternationalization::Get().GetCurrentLanguage();
		}

		const ETextPluralType PluralType =
			MarkerName.Equals(PluralAttribute, ESearchCase::CaseSensitive)
				? ETextPluralType::Cardinal
				: ETextPluralType::Ordinal;

		const ETextPluralForm PluralForm = Culture->GetPluralForm(NumericValue, PluralType);

		FString CaseName;
		switch (PluralForm)
		{
		case ETextPluralForm::Zero: CaseName = TEXT("zero"); break;
		case ETextPluralForm::One:  CaseName = TEXT("one");  break;
		case ETextPluralForm::Two:  CaseName = TEXT("two");  break;
		case ETextPluralForm::Few:  CaseName = TEXT("few");  break;
		case ETextPluralForm::Many: CaseName = TEXT("many"); break;
		default:                    CaseName = TEXT("other"); break;
		}

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
		if (FMath::IsNearlyEqual(FMath::Frac(NumericValue), 0.0))
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
	static const TMap<FString, TScriptInterface<IYarnMarkupProcessor>> NoProcessors;
	return ParseMarkupFull(Text, LocaleCode, bAddImplicitCharacterAttribute, NoProcessors);
}

FYarnMarkupParseResult UYarnMarkupLibrary::ParseMarkupFull(const FString& Text, const FString& LocaleCode, bool bAddImplicitCharacterAttribute, const TMap<FString, TScriptInterface<IYarnMarkupProcessor>>& MarkerProcessors)
{
	FYarnMarkupParseResult Result;

	if (Text.IsEmpty())
	{
		return Result;
	}

	// ========================================================================
	// Phase 0: Implicit character detection (before markup parsing)
	// Inject [character] markup into the input so the character attribute
	// goes through the full markup pipeline (matching C# LineParser)
	// ========================================================================

	FString Input = Text;

	if (bAddImplicitCharacterAttribute)
	{
		// Check if there's already an explicit [character marker
		FRegexPattern ExplicitPattern(TEXT("^\\s*\\[character"));
		FRegexMatcher ExplicitMatcher(ExplicitPattern, Input);

		if (!ExplicitMatcher.FindNext())
		{
			FRegexPattern ImplicitPattern(TEXT("^((?:[^:\\\\]|\\\\.)*):\\s*"));
			FRegexMatcher ImplicitMatcher(ImplicitPattern, Input);

			if (ImplicitMatcher.FindNext())
			{
				int32 MatchEnd = ImplicitMatcher.GetMatchEnding();
				FString CharName = Input.Mid(
					ImplicitMatcher.GetCaptureGroupBeginning(1),
					ImplicitMatcher.GetCaptureGroupEnding(1) - ImplicitMatcher.GetCaptureGroupBeginning(1));
				FString MatchStr = Input.Mid(0, MatchEnd);
				FString Rest = Input.Mid(MatchEnd);

				Input = FString::Printf(
					TEXT("[character name=\"%s\"]%s[/character]%s"),
					*CharName, *MatchStr, *Rest);
			}
		}
	}

	// Unescape \: to : now that character detection is done
	Input = Input.Replace(TEXT("\\:"), TEXT(":"));

	// ========================================================================
	// Phase 1: Parse markup tags into plain text and attributes
	// Handles escape sequences, close-all [/], nomarkup, self-closing tags
	// with whitespace trimming, adoption agency for misnested tags, and
	// built-in replacement markers (select/plural/ordinal)
	// ========================================================================

	FString PlainText;
	TArray<FMarkupOpenTag> OpenTags;
	int32 NextTrackingID = 0;

	auto CollectChildAttrs = [&Result](const FYarnMarkupAttribute& Attr, int32 ExcludeIndex, TArray<FYarnMarkupAttribute>& OutChildAttrs, TArray<int32>& OutChildIndices)
	{
		for (int32 k = 0; k < Result.Attributes.Num(); k++)
		{
			if (k == ExcludeIndex)
			{
				continue;
			}
			const FYarnMarkupAttribute& Existing = Result.Attributes[k];
			if (Existing.Position >= Attr.Position && Existing.Position + Existing.Length <= Attr.Position + Attr.Length)
			{
				FYarnMarkupAttribute Relative = Existing;
				Relative.Position -= Attr.Position;
				OutChildAttrs.Add(Relative);
				OutChildIndices.Add(k);
			}
		}
	};

	// Applies a registered marker processor to a just-completed attribute.
	// At close time the attribute's span is always the suffix of PlainText,
	// so the rewrite is a splice at Attr.Position with no position fix-ups
	// needed for attributes outside the span. Child attributes inside the
	auto TryRunMarkerProcessor = [&](const FYarnMarkupAttribute& Attr, bool bIsSplit) -> bool
	{
		if (MarkerProcessors.Num() == 0)
		{
			return false;
		}

		const TScriptInterface<IYarnMarkupProcessor>* Found = MarkerProcessors.Find(Attr.Name);
		if (!Found || !Found->GetObject())
		{
			return false;
		}

		if (bIsSplit)
		{
			return false;
		}

		FString ChildText = PlainText.Mid(Attr.Position, Attr.Length);

		TArray<FYarnMarkupAttribute> ChildAttrs;
		TArray<int32> ChildIndices;
		CollectChildAttrs(Attr, INDEX_NONE, ChildAttrs, ChildIndices);

		FYarnMarkupReplacementResult ProcResult = IYarnMarkupProcessor::Execute_ProcessMarkup(
			Found->GetObject(), Attr, ChildText, ChildAttrs, LocaleCode);

		for (const FString& Diagnostic : ProcResult.Diagnostics)
		{
			UE_LOG(LogYarnSpinner, Warning, TEXT("Markup: [%s] processor: %s"), *Attr.Name, *Diagnostic);
		}

		// Splice the rewritten text over the span (the current PlainText suffix).
		PlainText = PlainText.Left(Attr.Position) + ChildText;

		// Replace the original child attributes with the processor's versions.
		for (int32 k = ChildIndices.Num() - 1; k >= 0; k--)
		{
			Result.Attributes.RemoveAt(ChildIndices[k]);
		}
		for (FYarnMarkupAttribute& Relative : ChildAttrs)
		{
			Relative.Position += Attr.Position;
			Result.Attributes.Add(Relative);
		}

		return true;
	};

	// Track whether the previous sibling had trimwhitespace=true
	bool bTrimNextWhitespace = false;

	int32 i = 0;
	while (i < Input.Len())
	{
		TCHAR C = Input[i];

		// ---- Escape sequences ----
		// \[ becomes [ in the output, \] becomes ]
		if (C == TEXT('\\') && i + 1 < Input.Len())
		{
			TCHAR NextChar = Input[i + 1];
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
			for (int32 j = i + 1; j < Input.Len(); j++)
			{
				if (Input[j] == TEXT('"'))
				{
					bInTagQuotes = !bInTagQuotes;
				}
				else if (Input[j] == TEXT(']') && !bInTagQuotes)
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

			FString TagContent = Input.Mid(i + 1, TagEnd - i - 1);

			// ---- Close-all [/] ----
			if (TagContent == TEXT("/"))
			{
				// Close all open tags by creating attributes for each.
				// Innermost first, so a marker processor's rewrite of an inner
				// span is reflected in the outer attributes' lengths.
				for (int32 j = OpenTags.Num() - 1; j >= 0; j--)
				{
					const bool bSplit = OpenTags[j].TrackingID >= 0;
					FYarnMarkupAttribute Attr;
					Attr.Name = OpenTags[j].Name;
					Attr.Position = OpenTags[j].Position;
					Attr.SourcePosition = OpenTags[j].SourcePosition;
					Attr.Length = PlainText.Len() - OpenTags[j].Position;
					Attr.Properties = OpenTags[j].Properties;
					if (bSplit)
					{
						Attr.Properties.Add(TEXT("_splitID"), FYarnMarkupValue::MakeString(FString::FromInt(OpenTags[j].TrackingID)));
					}
					if (!TryRunMarkerProcessor(Attr, bSplit))
					{
						Result.Attributes.Add(Attr);
					}
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
					const bool bSplit = OpenTags[MatchIndex].TrackingID >= 0;
					FYarnMarkupAttribute Attr;
					Attr.Name = OpenTags[MatchIndex].Name;
					Attr.Position = OpenTags[MatchIndex].Position;
					Attr.SourcePosition = OpenTags[MatchIndex].SourcePosition;
					Attr.Length = PlainText.Len() - OpenTags[MatchIndex].Position;
					Attr.Properties = OpenTags[MatchIndex].Properties;
					if (bSplit)
					{
						Attr.Properties.Add(TEXT("_splitID"), FYarnMarkupValue::MakeString(FString::FromInt(OpenTags[MatchIndex].TrackingID)));
					}
					if (!TryRunMarkerProcessor(Attr, bSplit))
					{
						Result.Attributes.Add(Attr);
					}

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
				int32 CloseIdx = Input.Find(CloseTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, TagEnd + 1);

				if (CloseIdx != INDEX_NONE)
				{
					// Everything between [nomarkup] and [/nomarkup] is literal text
					FString LiteralText = Input.Mid(TagEnd + 1, CloseIdx - TagEnd - 1);
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
					FString LiteralText = Input.Mid(TagEnd + 1);
					int32 StartPos = PlainText.Len();
					PlainText += LiteralText;

					FYarnMarkupAttribute Attr;
					Attr.Name = NoMarkupAttribute;
					Attr.Position = StartPos;
					Attr.SourcePosition = TagSourcePosition;
					Attr.Length = LiteralText.Len();
					Result.Attributes.Add(Attr);

					i = Input.Len();
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

				if (TryRunMarkerProcessor(Attr, false))
				{
					// Consumed as a replacement marker, like built-in select/plural/ordinal
					bTrimNextWhitespace = false;
					i = TagEnd + 1;
					continue;
				}

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

	// Close any remaining open tags at end of text (innermost first, so
	// marker-processor rewrites of inner spans land before outer lengths
	// are computed)
	for (int32 j = OpenTags.Num() - 1; j >= 0; j--)
	{
		const bool bSplit = OpenTags[j].TrackingID >= 0;
		FYarnMarkupAttribute Attr;
		Attr.Name = OpenTags[j].Name;
		Attr.Position = OpenTags[j].Position;
		Attr.SourcePosition = OpenTags[j].SourcePosition;
		Attr.Length = PlainText.Len() - OpenTags[j].Position;
		Attr.Properties = OpenTags[j].Properties;
		if (bSplit)
		{
			Attr.Properties.Add(TEXT("_splitID"), FYarnMarkupValue::MakeString(FString::FromInt(OpenTags[j].TrackingID)));
		}
		if (!TryRunMarkerProcessor(Attr, bSplit))
		{
			Result.Attributes.Add(Attr);
		}
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
	// ========================================================================
	if (MarkerProcessors.Num() > 0)
	{
		int32 RewritesRemaining = Result.Attributes.Num() + 8;

		bool bDidRewrite = true;
		while (bDidRewrite && RewritesRemaining-- > 0)
		{
			bDidRewrite = false;

			for (int32 j = 0; j < Result.Attributes.Num(); j++)
			{
				const FYarnMarkupAttribute Attr = Result.Attributes[j];

				const TScriptInterface<IYarnMarkupProcessor>* Found = MarkerProcessors.Find(Attr.Name);
				if (!Found || !Found->GetObject())
				{
					continue;
				}

				FString ChildText = PlainText.Mid(Attr.Position, Attr.Length);

				TArray<FYarnMarkupAttribute> ChildAttrs;
				TArray<int32> ChildIndices;
				CollectChildAttrs(Attr, j, ChildAttrs, ChildIndices);

				FYarnMarkupReplacementResult ProcResult = IYarnMarkupProcessor::Execute_ProcessMarkup(
					Found->GetObject(), Attr, ChildText, ChildAttrs, LocaleCode);

				for (const FString& Diagnostic : ProcResult.Diagnostics)
				{
					UE_LOG(LogYarnSpinner, Warning, TEXT("Markup: [%s] processor: %s"), *Attr.Name, *Diagnostic);
				}

				const int32 SpanStart = Attr.Position;
				const int32 SpanEnd = Attr.Position + Attr.Length;
				const int32 Delta = ChildText.Len() - Attr.Length;

				PlainText = PlainText.Left(SpanStart) + ChildText + PlainText.Mid(SpanEnd);

				TArray<int32> ToRemove = ChildIndices;
				ToRemove.Add(j);
				ToRemove.Sort();
				for (int32 r = ToRemove.Num() - 1; r >= 0; r--)
				{
					Result.Attributes.RemoveAt(ToRemove[r]);
				}

				for (FYarnMarkupAttribute& Other : Result.Attributes)
				{
					if (Other.Position >= SpanEnd)
					{
						Other.Position += Delta;
					}
					else if (Other.Position <= SpanStart && Other.Position + Other.Length >= SpanEnd)
					{
						Other.Length += Delta;
					}
				}

				for (FYarnMarkupAttribute& Relative : ChildAttrs)
				{
					Relative.Position += SpanStart;
					Result.Attributes.Add(Relative);
				}

				bDidRewrite = true;
				break;
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
	// Phase 4: Extract character name and text-without-character from
	// the parsed character attribute (created by Phase 0 or explicit markup)
	// ========================================================================
	Result.TextWithoutCharacterName = PlainText;
	for (const FYarnMarkupAttribute& Attr : Result.Attributes)
	{
		if (Attr.Name.Equals(CharacterAttribute, ESearchCase::CaseSensitive))
		{
			Result.CharacterName = Attr.GetProperty(TEXT("name"));
			if (Attr.Length > 0)
			{
				Result.TextWithoutCharacterName = PlainText.Mid(Attr.Length);
			}
			break;
		}
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

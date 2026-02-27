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

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// our own header - must be included first for unreal header tool
#include "YarnLocalization.h"

// UYarnProject - we access the project to get base text, localisations, line
// metadata, and available cultures. the line providers need this to do lookups.
#include "YarnProgram.h"

// logging macros (UE_LOG with LogYarnSpinner category)
#include "YarnSpinnerModule.h"

// FCultureRef - used to get the current culture from unreal's internationalisation
// system. we match against available localisations in the yarn project.
#include "Internationalization/Culture.h"

// FInternationalization - the main entry point for unreal's i18n system.
// we use Get().GetCurrentCulture() to detect the system language.
#include "Internationalization/Internationalization.h"

// ============================================================================
// UYarnBuiltinLineProvider
// ============================================================================
//
// The default line provider. Handles looking up localised text from the yarn project's embedded string
// tables, resolving shadow lines, parsing character names, and applying
// substitutions.

FYarnLocalizedLine UYarnBuiltinLineProvider::GetLocalizedLine_Implementation(const FYarnLine& Line)
{
	FYarnLocalizedLine LocalizedLine;
	LocalizedLine.RawLine = Line;

	// can't do anything without a project
	if (!YarnProject)
	{
		return FYarnLocalizedLine::InvalidLine();
	}

	// --------------------------------------------------------------------
	// shadow line resolution
	// --------------------------------------------------------------------
	// shadow lines are a yarn spinner feature where one line can "shadow"
	// another, meaning it uses the other line's text. this is useful for
	// options that should display the same text as a line they reference.
	//
	// we need to follow the shadow chain to find the actual text, but we
	// also need to detect cycles to avoid infinite loops. we use a set to
	// track visited line ids and a depth limit as a safety measure.

	FString LookupLineID = Line.LineID;
	TSet<FString> VisitedLineIDs;
	VisitedLineIDs.Add(Line.LineID);

	// 10 is a reasonable limit - you'd never legitimately have shadow chains
	// deeper than this, and it protects against malformed data.
	constexpr int32 MaxShadowDepth = 10;
	int32 Depth = 0;

	// follow the shadow chain until we hit the end or a problem
	FString SourceLineID = GetShadowLineSource(LookupLineID);
	while (!SourceLineID.IsEmpty() && Depth < MaxShadowDepth)
	{
		// cycle detection - if we've seen this id before, we have a loop
		if (VisitedLineIDs.Contains(SourceLineID))
		{
			UE_LOG(LogYarnSpinner, Warning, TEXT("Shadow line cycle detected: %s -> %s. Breaking cycle."),
				*LookupLineID, *SourceLineID);
			break;
		}

		// record this id and move to the next in the chain
		VisitedLineIDs.Add(SourceLineID);
		LookupLineID = SourceLineID;
		SourceLineID = GetShadowLineSource(LookupLineID);
		Depth++;
	}

	// warn if we hit the depth limit - this suggests something's wrong
	if (Depth >= MaxShadowDepth)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Shadow line chain too deep (>%d) for line %s"),
			MaxShadowDepth, *Line.LineID);
	}

	// --------------------------------------------------------------------
	// text lookup with fallback chain
	// --------------------------------------------------------------------
	// we try to find localised text in this order:
	// 1. the current locale
	// 2. the fallback locale (if enabled)
	// 3. the base text from the yarn project

	FString Locale = GetLocaleCode();
	FString LocalizedText = GetLocalizedString(LookupLineID, Locale);

	// try fallback locale if we didn't find text and fallback is enabled
	if (LocalizedText.IsEmpty() && bUseFallback)
	{
		// use configured fallback or default to base language
		FString Fallback = FallbackLocaleCode.IsEmpty() ? YarnProject->BaseLanguage : FallbackLocaleCode;
		if (!Fallback.IsEmpty() && Fallback != Locale)
		{
			LocalizedText = GetLocalizedString(LookupLineID, Fallback);
		}
	}

	// last resort - use base text
	if (LocalizedText.IsEmpty())
	{
		LocalizedText = YarnProject->GetBaseText(LookupLineID);
	}

	// if we still have nothing, that's a problem
	if (LocalizedText.IsEmpty())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Missing localized line: %s"), *Line.LineID);
		return FYarnLocalizedLine::InvalidLine();
	}

	// --------------------------------------------------------------------
	// substitutions and character name parsing
	// --------------------------------------------------------------------
	// substitutions are placeholders like {0}, {1} that get replaced with
	// runtime values. we apply them before parsing character names because
	// the character name might come from a substitution.

	LocalizedText = UYarnLocalizationLibrary::ApplySubstitutions(LocalizedText, Line.Substitutions);

	// try to parse a character name from the "Name: dialogue" format.
	// if found, we strip the name from the displayed text to avoid duplication
	// (the name is typically shown separately in the ui).
	FString DialogueText;
	if (UYarnLocalizationLibrary::ParseCharacterFromLine(LocalizedText, LocalizedLine.CharacterName, DialogueText))
	{
		// character name found - use the text without the name prefix
		LocalizedLine.Text = FText::FromString(DialogueText);
		LocalizedLine.TextWithoutCharacterName = FText::FromString(DialogueText);
	}
	else
	{
		// no character name - use the full text
		LocalizedLine.Text = FText::FromString(LocalizedText);
		LocalizedLine.TextWithoutCharacterName = FText::FromString(LocalizedText);
	}

	// get metadata using the original line id, not the shadow source.
	// metadata is specific to each line, even if the text comes from elsewhere.
	LocalizedLine.Metadata = GetLineMetadata(Line.LineID);

	return LocalizedLine;
}

// ----------------------------------------------------------------------------
// locale management
// ----------------------------------------------------------------------------

FString UYarnBuiltinLineProvider::GetLocaleCode() const
{
	// if auto-detection is enabled, try to match the system culture to an
	// available localisation in the project.
	if (bAutoDetectCulture)
	{
		// get the system's two-letter language code (e.g., "en", "de", "zh").
		FString SystemCulture = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();

		// check if we have this culture in the project
		if (YarnProject && YarnProject->HasLocalization(SystemCulture))
		{
			return SystemCulture;
		}

		// try the full culture name (e.g., "en-AU", "zh-Hans") for more specific
		// matching. this handles cases like simplified vs traditional chinese.
		FString FullCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
		if (YarnProject && YarnProject->HasLocalization(FullCulture))
		{
			return FullCulture;
		}

		// fall back to manually configured locale
		if (!TextLocaleCode.IsEmpty())
		{
			return TextLocaleCode;
		}

		// fall back to the project's base language
		if (YarnProject && !YarnProject->BaseLanguage.IsEmpty())
		{
			return YarnProject->BaseLanguage;
		}

		// last resort - return the system culture even if we don't have it
		return SystemCulture;
	}

	// auto-detection disabled - use the configured locale
	if (!TextLocaleCode.IsEmpty())
	{
		return TextLocaleCode;
	}

	// fall back to base language from project
	if (YarnProject && !YarnProject->BaseLanguage.IsEmpty())
	{
		return YarnProject->BaseLanguage;
	}

	// absolute last resort
	return TEXT("en");
}

void UYarnBuiltinLineProvider::SetLocaleCode(const FString& LocaleCode)
{
	// when explicitly setting a locale, disable auto-detection since the
	// caller clearly wants a specific language.
	bAutoDetectCulture = false;
	TextLocaleCode = LocaleCode;
}

TArray<FString> UYarnBuiltinLineProvider::GetAvailableLocales() const
{
	// delegate to the project - it knows what localisations are available
	if (YarnProject)
	{
		return YarnProject->GetAvailableCultures();
	}
	return TArray<FString>();
}

// ----------------------------------------------------------------------------
// runtime string management
// ----------------------------------------------------------------------------
// these functions allow adding localised strings at runtime, which is useful
// for procedurally generated content or dynamically loaded translations.

void UYarnBuiltinLineProvider::AddLocalizedString(const FString& LineID, const FString& LocaleCode, const FString& Text)
{
	if (!YarnProject)
	{
		return;
	}

	// try to find existing localisation for this locale
	if (FYarnLocalization* Localization = YarnProject->Localizations.Find(LocaleCode))
	{
		Localization->AddLocalizedString(LineID, Text);
	}
	else
	{
		// create a new localisation entry for this locale
		FYarnLocalization NewLocalization;
		NewLocalization.AddLocalizedString(LineID, Text);
		YarnProject->Localizations.Add(LocaleCode, NewLocalization);
	}
}

FString UYarnBuiltinLineProvider::GetLocalizedString(const FString& LineID, const FString& LocaleCode) const
{
	if (!YarnProject)
	{
		return FString();
	}

	// look up the string in the localisation for this locale
	if (const FYarnLocalization* Localization = YarnProject->Localizations.Find(LocaleCode))
	{
		return Localization->GetLocalizedString(LineID);
	}

	return FString();
}

// ----------------------------------------------------------------------------
// shadow line support
// ----------------------------------------------------------------------------
// shadow lines allow one line to use another line's text. this is implemented
// via metadata tags in the format "shadow:source_line_id".

FString UYarnBuiltinLineProvider::GetShadowLineSource(const FString& LineID) const
{
	if (!YarnProject)
	{
		return FString();
	}

	// look up metadata for this line
	const FString* MetadataStr = YarnProject->LineMetadata.Find(LineID);
	if (!MetadataStr)
	{
		return FString();
	}

	// parse metadata tags - they're stored as space-separated values
	TArray<FString> Tags;
	MetadataStr->ParseIntoArray(Tags, TEXT(" "));

	// look for a shadow: tag
	for (const FString& Tag : Tags)
	{
		if (Tag.StartsWith(TEXT("shadow:")))
		{
			// extract the source line id from after the prefix
			FString SourceID = Tag.Mid(7);  // 7 = length of "shadow:"

			// ensure the source id has the standard "line:" prefix
			if (!SourceID.StartsWith(TEXT("line:")))
			{
				SourceID = TEXT("line:") + SourceID;
			}
			return SourceID;
		}
	}

	return FString();
}

TArray<FString> UYarnBuiltinLineProvider::GetLineMetadata(const FString& LineID) const
{
	TArray<FString> Result;

	if (!YarnProject)
	{
		return Result;
	}

	// look up and parse the metadata string for this line
	const FString* MetadataStr = YarnProject->LineMetadata.Find(LineID);
	if (MetadataStr)
	{
		MetadataStr->ParseIntoArray(Result, TEXT(" "));
	}

	return Result;
}

// ============================================================================
// UYarnStringTableLineProvider
// ============================================================================
//
// this provider uses unreal's string table system for localisation. string
// tables integrate with unreal's localisation dashboard and can be translated
// using standard unreal workflows.

FYarnLocalizedLine UYarnStringTableLineProvider::GetLocalizedLine_Implementation(const FYarnLine& Line)
{
	FYarnLocalizedLine LocalizedLine;
	LocalizedLine.RawLine = Line;

	FString LocalizedText;
	if (GetStringTableEntry(Line.LineID, LocalizedText))
	{
		// found the entry in the string table - apply substitutions and parse
		LocalizedText = UYarnLocalizationLibrary::ApplySubstitutions(LocalizedText, Line.Substitutions);

		// parse character name from "Name: dialogue" format
		UYarnLocalizationLibrary::ParseCharacterFromLine(
			LocalizedText,
			LocalizedLine.CharacterName,
			LocalizedText);

		LocalizedLine.Text = FText::FromString(LocalizedText);
	}
	else if (bFallbackToBaseText && YarnProject)
	{
		// string table entry not found - fall back to base text from the
		// yarn project if configured to do so
		FString BaseText = YarnProject->GetBaseText(Line.LineID);
		if (!BaseText.IsEmpty())
		{
			BaseText = UYarnLocalizationLibrary::ApplySubstitutions(BaseText, Line.Substitutions);

			UYarnLocalizationLibrary::ParseCharacterFromLine(
				BaseText,
				LocalizedLine.CharacterName,
				BaseText);

			LocalizedLine.Text = FText::FromString(BaseText);
		}
		else
		{
			// absolute last resort - show the line id so at least there's
			// something visible in the ui
			LocalizedLine.Text = FText::FromString(Line.LineID);
		}
	}
	else
	{
		LocalizedLine.Text = FText::FromString(Line.LineID);
	}

	return LocalizedLine;
}

void UYarnStringTableLineProvider::SetStringTable(UStringTable* InStringTable)
{
	StringTable = InStringTable;
}

void UYarnStringTableLineProvider::ImportYarnProjectToStringTable(UYarnProject* InYarnProject, UStringTable* InStringTable)
{
	// this is an editor-only utility for importing yarn project strings into
	// an unreal string table. useful for setting up initial localisation.
	if (!InYarnProject || !InStringTable)
	{
		return;
	}

#if WITH_EDITOR
	// get a mutable reference to the string table data
	FStringTableRef TableRef = InStringTable->GetMutableStringTable();

	// add an entry for each line in the project
	for (const auto& Pair : InYarnProject->BaseStringTable)
	{
		// convert yarn line id to string table key format
		FString Key = UYarnLocalizationLibrary::LineIDToStringTableKey(Pair.Key);
		TableRef->SetSourceString(Key, Pair.Value);
	}
#endif
}

bool UYarnStringTableLineProvider::GetStringTableEntry(const FString& LineID, FString& OutText) const
{
	if (!StringTable)
	{
		return false;
	}

	// convert yarn line id to string table key format
	FString Key = UYarnLocalizationLibrary::LineIDToStringTableKey(LineID);

	// look up the entry in the string table
	FStringTableConstRef TableRef = StringTable->GetStringTable();
	FStringTableEntryConstPtr Entry = TableRef->FindEntry(Key);
	if (Entry.IsValid())
	{
		OutText = Entry->GetSourceString();
		return true;
	}

	// entry not found - optionally log this for debugging
	if (bLogMissingEntries)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("String table entry not found for key: %s"), *Key);
	}

	return false;
}

// ============================================================================
// UYarnCultureAwareLineProvider
// ============================================================================
//
// this provider uses multiple string tables, one per culture, and automatically
// selects the right one based on unreal's current culture setting. this
// integrates cleanly with unreal's localisation system.

FYarnLocalizedLine UYarnCultureAwareLineProvider::GetLocalizedLine_Implementation(const FYarnLine& Line)
{
	FYarnLocalizedLine LocalizedLine;
	LocalizedLine.RawLine = Line;

	// get the string table for the current culture
	UStringTable* CurrentStringTable = GetCurrentCultureStringTable();

	if (CurrentStringTable)
	{
		// convert line id to key format and look up
		FString Key = UYarnLocalizationLibrary::LineIDToStringTableKey(Line.LineID);
		FStringTableConstRef TableRef = CurrentStringTable->GetStringTable();
		FStringTableEntryConstPtr Entry = TableRef->FindEntry(Key);
		if (Entry.IsValid())
		{
			FString LocalizedText = Entry->GetSourceString();
			LocalizedText = UYarnLocalizationLibrary::ApplySubstitutions(LocalizedText, Line.Substitutions);

			UYarnLocalizationLibrary::ParseCharacterFromLine(
				LocalizedText,
				LocalizedLine.CharacterName,
				LocalizedText);

			LocalizedLine.Text = FText::FromString(LocalizedText);
			return LocalizedLine;
		}
	}

	// fall back to base text from the yarn project
	if (bFallbackToBaseText && YarnProject)
	{
		FString BaseText = YarnProject->GetBaseText(Line.LineID);
		if (!BaseText.IsEmpty())
		{
			BaseText = UYarnLocalizationLibrary::ApplySubstitutions(BaseText, Line.Substitutions);

			UYarnLocalizationLibrary::ParseCharacterFromLine(
				BaseText,
				LocalizedLine.CharacterName,
				BaseText);

			LocalizedLine.Text = FText::FromString(BaseText);
			return LocalizedLine;
		}
	}

	// last resort
	LocalizedLine.Text = FText::FromString(Line.LineID);
	return LocalizedLine;
}

void UYarnCultureAwareLineProvider::AddCultureStringTable(const FString& CultureCode, UStringTable* InStringTable)
{
	// register a string table for a specific culture code
	CultureStringTables.Add(CultureCode, InStringTable);
}

FString UYarnCultureAwareLineProvider::GetCurrentCulture()
{
	// get the full culture name from unreal's internationalisation system
	return FInternationalization::Get().GetCurrentCulture()->GetName();
}

FString UYarnCultureAwareLineProvider::GetCurrentBaseCulture()
{
	FString Culture = GetCurrentCulture();

	// extract the base culture by stripping the region suffix.
	// for example, "en-AU" becomes "en", "zh-Hans" becomes "zh".
	int32 DashIndex;
	if (Culture.FindChar(TEXT('-'), DashIndex))
	{
		return Culture.Left(DashIndex);
	}

	return Culture;
}

UStringTable* UYarnCultureAwareLineProvider::GetCurrentCultureStringTable() const
{
	// try to find a string table for the current culture, with fallback to
	// less specific matches.

	// first, try the full culture name (e.g., "en-AU")
	FString FullCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
	if (UStringTable* const* Found = CultureStringTables.Find(FullCulture))
	{
		return *Found;
	}

	// try the base culture (e.g., "en" from "en-AU")
	int32 DashIndex;
	if (FullCulture.FindChar(TEXT('-'), DashIndex))
	{
		FString BaseCulture = FullCulture.Left(DashIndex);
		if (UStringTable* const* Found = CultureStringTables.Find(BaseCulture))
		{
			return *Found;
		}
	}

	// try the configured default culture
	if (!DefaultCulture.IsEmpty())
	{
		if (UStringTable* const* Found = CultureStringTables.Find(DefaultCulture))
		{
			return *Found;
		}
	}

	// no matching string table found
	return nullptr;
}

// ============================================================================
// UYarnLocalizationLibrary - static utility functions
// ============================================================================

bool UYarnLocalizationLibrary::ParseCharacterFromLine(const FString& LineText, FString& OutCharacterName, FString& OutDialogueText)
{
	// yarn spinner uses the convention "Character: dialogue text" to indicate
	// who is speaking. we parse this out so the character name can be displayed
	// separately (e.g., in a name plate above the dialogue box).

	int32 ColonIndex;
	if (LineText.FindChar(TEXT(':'), ColonIndex))
	{
		// sanity check - the colon should be reasonably early in the line.
		// if it's 50+ characters in, it's probably part of the dialogue
		// (like a time "10:30" or a url).
		if (ColonIndex > 0 && ColonIndex < 50)
		{
			FString PotentialName = LineText.Left(ColonIndex).TrimStartAndEnd();

			// validate that it looks like a character name. we allow letters,
			// numbers, underscores, and spaces. anything else (like punctuation
			// or special characters) suggests this isn't actually a name.
			bool bValidName = true;
			for (TCHAR Char : PotentialName)
			{
				if (!FChar::IsAlnum(Char) && Char != TEXT('_') && Char != TEXT(' '))
				{
					bValidName = false;
					break;
				}
			}

			if (bValidName && PotentialName.Len() > 0)
			{
				OutCharacterName = PotentialName;
				// strip the name and colon, trim leading whitespace from dialogue
				OutDialogueText = LineText.Mid(ColonIndex + 1).TrimStartAndEnd();
				return true;
			}
		}
	}

	// no valid character name found - return the full text as dialogue
	OutCharacterName.Empty();
	OutDialogueText = LineText;
	return false;
}

FString UYarnLocalizationLibrary::ApplySubstitutions(const FString& Text, const TArray<FString>& Substitutions)
{
	// yarn uses {0}, {1}, etc. as placeholders for runtime values.
	// this function replaces them with the actual values.

	FString Result = Text;

	for (int32 i = 0; i < Substitutions.Num(); i++)
	{
		FString Placeholder = FString::Printf(TEXT("{%d}"), i);
		Result = Result.Replace(*Placeholder, *Substitutions[i]);
	}

	return Result;
}

FString UYarnLocalizationLibrary::LineIDToStringTableKey(const FString& LineID)
{
	// convert a yarn line id to a format suitable for unreal string table keys.
	// mainly this just strips the "line:" prefix that yarn adds to all line ids.

	FString Key = LineID;

	if (Key.StartsWith(TEXT("line:")))
	{
		Key = Key.Mid(5);  // 5 = length of "line:"
	}

	return Key;
}

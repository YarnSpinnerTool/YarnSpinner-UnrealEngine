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

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YarnDialoguePresenter.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "YarnLocalization.generated.h"

class UStringTable;

/**
 * Built-in line provider that uses localization data stored in the Yarn Project.
 *
 * This is the recommended line provider for most projects. It automatically
 * loads localization from CSV files specified in the .yarnproject file and
 * selects the appropriate translation based on the current culture.
 *
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API UYarnBuiltinLineProvider : public UYarnLineProvider
{
	GENERATED_BODY()

public:
	/**
	 * Whether to automatically detect the current culture from the system.
	 * If false, uses the TextLocaleCode setting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization")
	bool bAutoDetectCulture = true;

	/**
	 * The locale code to use for text (e.g., "en", "fr", "de").
	 * Only used when bAutoDetectCulture is false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization", meta = (EditCondition = "!bAutoDetectCulture"))
	FString TextLocaleCode;

	/**
	 * Whether to use a fallback locale when a line is not found.
	 * If true, tries the fallback locale when a line isn't found in the primary locale.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization|Fallback")
	bool bUseFallback = true;

	/**
	 * The fallback locale to use when a line is not found in the primary locale.
	 * If empty, falls back to the project's base language.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization|Fallback", meta = (EditCondition = "bUseFallback"))
	FString FallbackLocaleCode;

	// UYarnLineProvider interface
	virtual FYarnLocalizedLine GetLocalizedLine_Implementation(const FYarnLine& Line) override;

	/**
	 * Get the current text locale being used.
	 * @return The locale code (e.g., "en", "fr").
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Yarn Spinner|Localization")
	FString GetLocaleCode() const;

	/**
	 * Set the locale to use for text localization.
	 * Also disables auto-detection.
	 * @param LocaleCode The locale code (e.g., "en", "fr").
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localization")
	void SetLocaleCode(const FString& LocaleCode);

	/**
	 * Get all available locales from the Yarn Project.
	 * @return Array of locale codes that have localization data.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Yarn Spinner|Localization")
	TArray<FString> GetAvailableLocales() const;

	/**
	 * Add a localized string at runtime (not persisted).
	 * @param LineID The line ID.
	 * @param LocaleCode The locale for this string.
	 * @param Text The localized text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localization")
	void AddLocalizedString(const FString& LineID, const FString& LocaleCode, const FString& Text);

protected:
	/**
	 * Get the localized string for a line, handling shadow lines and fallback.
	 * @param LineID The line ID (may be redirected if it's a shadow line).
	 * @param LocaleCode The locale to look up.
	 * @return The localized text, or empty if not found.
	 */
	FString GetLocalizedString(const FString& LineID, const FString& LocaleCode) const;

	/**
	 * Check if a line is a shadow line and get its source.
	 * Shadow lines redirect to another line's text.
	 * @param LineID The line ID to check.
	 * @return The source line ID if this is a shadow line, or empty string if not.
	 */
	FString GetShadowLineSource(const FString& LineID) const;

	/**
	 * Get line metadata as an array.
	 * @param LineID The line ID.
	 * @return Array of metadata tags.
	 */
	TArray<FString> GetLineMetadata(const FString& LineID) const;
};

/**
 * Line provider that uses Unreal's string table system for localisation.
 *
 * This provider maps Yarn line IDs to string table entries, allowing you
 * to use Unreal's built-in localisation workflow.
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API UYarnStringTableLineProvider : public UYarnLineProvider
{
	GENERATED_BODY()

public:
	/**
	 * The string table to use for localisation.
	 * Each entry should have a key matching the Yarn line ID (e.g., "line:abc123").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localisation")
	UStringTable* StringTable;

	/**
	 * Whether to fall back to base text if a string table entry is not found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localisation")
	bool bFallbackToBaseText = true;

	/**
	 * Whether to log warnings when entries are not found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localisation|Debug")
	bool bLogMissingEntries = false;

	// UYarnLineProvider interface
	virtual FYarnLocalizedLine GetLocalizedLine_Implementation(const FYarnLine& Line) override;

	/**
	 * Set the string table to use for localisation.
	 * @param InStringTable The string table asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
	void SetStringTable(UStringTable* InStringTable);

	/**
	 * Import lines from a Yarn Project into a string table.
	 * This creates entries for all lines in the project.
	 *
	 * @param InYarnProject The Yarn project to import from.
	 * @param InStringTable The string table to import to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
	static void ImportYarnProjectToStringTable(UYarnProject* InYarnProject, UStringTable* InStringTable);

protected:
	/**
	 * Get the text for a line ID from the string table.
	 * @param LineID The line ID to look up.
	 * @param OutText The text if found.
	 * @return True if the entry was found.
	 */
	bool GetStringTableEntry(const FString& LineID, FString& OutText) const;
};

/**
 * Line provider that uses Unreal's culture-aware string tables.
 *
 * This provider automatically selects the appropriate string table based
 * on the current culture setting.
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API UYarnCultureAwareLineProvider : public UYarnLineProvider
{
	GENERATED_BODY()

public:
	/**
	 * Map of culture codes to string tables.
	 * Key is the culture code (e.g., "en", "fr", "de").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localisation")
	TMap<FString, UStringTable*> CultureStringTables;

	/**
	 * The default culture to use if the current culture is not found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localisation")
	FString DefaultCulture = TEXT("en");

	/**
	 * Whether to fall back to base text if no matching culture is found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localisation")
	bool bFallbackToBaseText = true;

	// UYarnLineProvider interface
	virtual FYarnLocalizedLine GetLocalizedLine_Implementation(const FYarnLine& Line) override;

	/**
	 * Add a string table for a culture.
	 * @param CultureCode The culture code (e.g., "en", "fr").
	 * @param InStringTable The string table for this culture.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
	void AddCultureStringTable(const FString& CultureCode, UStringTable* InStringTable);

	/**
	 * Get the current culture code.
	 * @return The current culture code (e.g., "en-GB").
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Yarn Spinner|Localisation")
	static FString GetCurrentCulture();

	/**
	 * Get the base culture code (without region).
	 * @return The base culture code (e.g., "en" from "en-GB").
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Yarn Spinner|Localisation")
	static FString GetCurrentBaseCulture();

protected:
	/**
	 * Get the string table for the current culture.
	 * @return The string table, or nullptr if none found.
	 */
	UStringTable* GetCurrentCultureStringTable() const;
};

/**
 * Utility functions for Yarn localisation.
 */
UCLASS()
class YARNSPINNER_API UYarnLocalizationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Extract character name from a line using the character attribute.
	 *
	 * Yarn Spinner uses the format "Character: Line text" or the character
	 * attribute syntax. This function parses both formats.
	 *
	 * @param LineText The full line text.
	 * @param OutCharacterName The extracted character name.
	 * @param OutDialogueText The dialogue text without the character prefix.
	 * @return True if a character name was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
	static bool ParseCharacterFromLine(const FString& LineText, FString& OutCharacterName, FString& OutDialogueText);

	/**
	 * Perform substitutions on localised text.
	 *
	 * Replaces {0}, {1}, etc. with the provided values.
	 *
	 * @param Text The text containing substitution markers.
	 * @param Substitutions The values to substitute.
	 * @return The text with substitutions applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
	static FString ApplySubstitutions(const FString& Text, const TArray<FString>& Substitutions);

	/**
	 * Convert a Yarn line ID to a string table key.
	 * This sanitises the ID for use in string tables.
	 *
	 * @param LineID The Yarn line ID.
	 * @return A sanitised key for use in string tables.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Yarn Spinner|Localisation")
	static FString LineIDToStringTableKey(const FString& LineID);
};

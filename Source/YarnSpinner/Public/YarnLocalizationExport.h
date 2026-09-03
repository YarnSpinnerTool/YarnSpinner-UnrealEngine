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
#include "YarnLocalizationExport.generated.h"

// forward declaration - the actual yarn project asset type
class UYarnProject;

 /**
 * Result of a localization export operation.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnLocalizationExportResult
{
    GENERATED_BODY()

    /** Whether the export succeeded */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Localisation")
    bool bSuccess = false;

    /** Number of lines exported */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Localisation")
    int32 LineCount = 0;

    /** Path to the exported file */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Localisation")
    FString FilePath;

    /** Error message if failed */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Localisation")
    FString ErrorMessage;
};

 /**
 * Options for CSV export.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnCSVExportOptions
{
    GENERATED_BODY()

    /** Include the line ID column */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    bool bIncludeLineID = true;

    /** Include the character name column (extracted from "Character: Text" format) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    bool bIncludeCharacter = true;

    /** Include the node name column */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    bool bIncludeNodeName = true;

    /** Include line tags column */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    bool bIncludeTags = false;

    /** Include a column for the translation (empty, for translators to fill) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    bool bIncludeTranslationColumn = true;

    /** Header name for the translation column */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    FString TranslationColumnHeader = TEXT("Translation");

    /** Delimiter character (comma by default) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    FString Delimiter = TEXT(",");

    /** Whether to use BOM for UTF-8 (some programs like Excel need this) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    bool bIncludeUTF8BOM = true;

    /** Sort lines by node name then line ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Localisation")
    bool bSortLines = true;
};

 /**
 * Blueprint function library for Yarn Spinner localization export.
 * Provides functions to export dialogue strings to CSV format
 * for translation workflows.
 */
UCLASS()
class YARNSPINNER_API UYarnLocalizationExportLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
     /**
     * Export all strings from a Yarn project to a CSV file.
     * The CSV includes columns for:
     * - Line ID (unique identifier for each line)
     * - Character (speaker name, if line is in "Character: Text" format)
     * - Text (the actual dialogue text)
     * - Node (the node the line belongs to)
     * - Translation (empty column for translators)
     * @param YarnProject The project to export strings from
     * @param FilePath The path to save the CSV file
     * @param Options Export options
     * @return Export result with success status and line count
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
    static FYarnLocalizationExportResult ExportStringsToCSV(
        UYarnProject* YarnProject,
        const FString& FilePath,
        const FYarnCSVExportOptions& Options);

     /**
     * Export strings to CSV with default options.
     * @param YarnProject The project to export
     * @param FilePath The path to save the CSV file
     * @return Export result
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
    static FYarnLocalizationExportResult ExportStringsToCSVSimple(
        UYarnProject* YarnProject,
        const FString& FilePath);

     /**
     * Get the default export path for a project (next to the .uasset).
     * @param YarnProject The project
     * @return Suggested file path for the CSV
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
    static FString GetDefaultExportPath(UYarnProject* YarnProject);

     /**
     * Extract character name from a line in "Character: Text" format.
     * @param LineText The full line text
     * @param OutCharacter The extracted character name
     * @param OutText The remaining text after the character name
     * @return True if a character name was found
     */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Localisation")
    static bool ExtractCharacterFromLine(
        const FString& LineText,
        FString& OutCharacter,
        FString& OutText);

private:
    // escapes special characters in a value so it can be safely included in csv
    // handles quotes, newlines, and the delimiter character
    static FString EscapeCSV(const FString& Value, const FString& Delimiter);

    // searches through the compiled program to find which node contains a given line id
    // falls back to parsing the line id format if instruction search fails
    static FString FindNodeForLineID(UYarnProject* YarnProject, const FString& LineID);
};

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

#include "YarnLocalizationExport.h"
#include "YarnProgram.h"
#include "YarnSpinnerModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Exports all dialogue strings from a yarn project to a CSV file for
// translators or localisation workflows.
FYarnLocalizationExportResult UYarnLocalizationExportLibrary::ExportStringsToCSV(
    UYarnProject* YarnProject,
    const FString& FilePath,
    const FYarnCSVExportOptions& Options)
{
    FYarnLocalizationExportResult Result;
    Result.FilePath = FilePath;

    if (!YarnProject)
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("YarnProject is null");
        return Result;
    }

    if (YarnProject->Program.Nodes.Num() == 0)
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("YarnProject has no valid program");
        return Result;
    }

    if (FilePath.IsEmpty())
    {
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("FilePath is empty");
        return Result;
    }

    FString CSVContent;

    // Build the header row based on which columns the user wants
    TArray<FString> Headers;
    if (Options.bIncludeLineID)
    {
        Headers.Add(TEXT("id"));
    }
    if (Options.bIncludeNodeName)
    {
        Headers.Add(TEXT("node"));
    }
    if (Options.bIncludeCharacter)
    {
        Headers.Add(TEXT("character"));
    }
    Headers.Add(TEXT("text"));
    if (Options.bIncludeTags)
    {
        Headers.Add(TEXT("tags"));
    }
    if (Options.bIncludeTranslationColumn)
    {
        Headers.Add(Options.TranslationColumnHeader);
    }

    CSVContent += FString::Join(Headers, *Options.Delimiter) + TEXT("\n");

    struct FLineExportData
    {
        FString LineID;
        FString NodeName;
        FString Character;
        FString Text;
        FString Tags;
    };
    TArray<FLineExportData> Lines;

    for (const auto& StringPair : YarnProject->BaseStringTable)
    {
        const FString& LineID = StringPair.Key;
        const FString& FullText = StringPair.Value;

        FLineExportData LineData;
        LineData.LineID = LineID;

        LineData.NodeName = FindNodeForLineID(YarnProject, LineID);

        if (!ExtractCharacterFromLine(FullText, LineData.Character, LineData.Text))
        {
            LineData.Text = FullText;
        }

        if (Options.bIncludeTags)
        {
            LineData.Tags = TEXT("");
        }

        Lines.Add(LineData);
    }

    if (Options.bSortLines)
    {
        Lines.Sort([](const FLineExportData& A, const FLineExportData& B)
        {
            int32 NodeCompare = A.NodeName.Compare(B.NodeName);
            if (NodeCompare != 0)
            {
                return NodeCompare < 0;
            }
            return A.LineID.Compare(B.LineID) < 0;
        });
    }

    for (const FLineExportData& LineData : Lines)
    {
        TArray<FString> Row;

        if (Options.bIncludeLineID)
        {
            Row.Add(EscapeCSV(LineData.LineID, Options.Delimiter));
        }
        if (Options.bIncludeNodeName)
        {
            Row.Add(EscapeCSV(LineData.NodeName, Options.Delimiter));
        }
        if (Options.bIncludeCharacter)
        {
            Row.Add(EscapeCSV(LineData.Character, Options.Delimiter));
        }
        Row.Add(EscapeCSV(LineData.Text, Options.Delimiter));
        if (Options.bIncludeTags)
        {
            Row.Add(EscapeCSV(LineData.Tags, Options.Delimiter));
        }
        if (Options.bIncludeTranslationColumn)
        {
            Row.Add(TEXT(""));
        }

        CSVContent += FString::Join(Row, *Options.Delimiter) + TEXT("\n");
        Result.LineCount++;
    }

    bool bSaved = FFileHelper::SaveStringToFile(CSVContent, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    if (bSaved)
    {
        Result.bSuccess = true;
        UE_LOG(LogYarnSpinner, Log, TEXT("Exported %d lines to %s"), Result.LineCount, *FilePath);
    }
    else
    {
        Result.bSuccess = false;
        Result.ErrorMessage = FString::Printf(TEXT("Failed to save file to %s"), *FilePath);
    }

    return Result;
}

// Simple version that uses default export options.
FYarnLocalizationExportResult UYarnLocalizationExportLibrary::ExportStringsToCSVSimple(
    UYarnProject* YarnProject,
    const FString& FilePath)
{
    FYarnCSVExportOptions DefaultOptions;
    return ExportStringsToCSV(YarnProject, FilePath, DefaultOptions);
}

// Generates a default path for the exported CSV file next to the project asset.
FString UYarnLocalizationExportLibrary::GetDefaultExportPath(UYarnProject* YarnProject)
{
    if (!YarnProject)
    {
        return TEXT("");
    }

    FString PackagePath = YarnProject->GetPathName();
    FString Directory = FPaths::GetPath(PackagePath);
    FString AssetName = FPaths::GetBaseFilename(PackagePath);

    FString ProjectDir = FPaths::ProjectContentDir();
    Directory = Directory.Replace(TEXT("/Game/"), *ProjectDir);

    return FPaths::Combine(Directory, AssetName + TEXT("_strings.csv"));
}

// Tries to extract a character name from dialogue text in "Character: text" format.
bool UYarnLocalizationExportLibrary::ExtractCharacterFromLine(
    const FString& LineText,
    FString& OutCharacter,
    FString& OutText)
{
    int32 ColonIndex;
    if (LineText.FindChar(TEXT(':'), ColonIndex))
    {
        if (ColonIndex > 0 && ColonIndex < 50)
        {
            FString PotentialCharacter = LineText.Left(ColonIndex).TrimStartAndEnd();

            if (!PotentialCharacter.IsEmpty() &&
                !PotentialCharacter.Contains(TEXT("\"")) &&
                !PotentialCharacter.Contains(TEXT("\n")) &&
                !PotentialCharacter.Contains(TEXT("{")))
            {
                OutCharacter = PotentialCharacter;
                OutText = LineText.Mid(ColonIndex + 1).TrimStart();
                return true;
            }
        }
    }

    OutCharacter = TEXT("");
    OutText = LineText;
    return false;
}

// Escapes a string value for safe inclusion in a CSV file.
FString UYarnLocalizationExportLibrary::EscapeCSV(const FString& Value, const FString& Delimiter)
{
    FString Result = Value;

    // Wrap in quotes if the value contains quotes, the delimiter, or newlines
    bool bNeedsQuotes = Result.Contains(TEXT("\"")) ||
                        Result.Contains(Delimiter) ||
                        Result.Contains(TEXT("\n")) ||
                        Result.Contains(TEXT("\r"));

    // Double any existing quotes (CSV standard)
    if (Result.Contains(TEXT("\"")))
    {
        Result = Result.Replace(TEXT("\""), TEXT("\"\""));
    }

    // Wrap the whole value in quotes if needed
    if (bNeedsQuotes)
    {
        Result = TEXT("\"") + Result + TEXT("\"");
    }

    return Result;
}

// Finds which node a line ID belongs to by searching through the compiled
// program instructions.
FString UYarnLocalizationExportLibrary::FindNodeForLineID(UYarnProject* YarnProject, const FString& LineID)
{
    if (!YarnProject || YarnProject->Program.Nodes.Num() == 0)
    {
        return TEXT("");
    }

    // Search through node instructions for a direct reference to this line ID
    for (const auto& NodePair : YarnProject->Program.Nodes)
    {
        const FString& NodeName = NodePair.Key;
        const FYarnNode& Node = NodePair.Value;

        // RunLine and AddOption instructions reference line IDs in their string operand
        for (const FYarnInstruction& Instruction : Node.Instructions)
        {
            if (Instruction.Type == EYarnInstructionType::RunLine ||
                Instruction.Type == EYarnInstructionType::AddOption)
            {
                if (Instruction.StringOperand == LineID)
                {
                    return NodeName;
                }
            }
        }
    }

    // Fall back to parsing the line ID format "line:NodeName-lineNumber"
    if (LineID.StartsWith(TEXT("line:")))
    {
        FString AfterPrefix = LineID.Mid(5);
        int32 DashIndex;
        // Split at the last dash to separate node name from line number
        if (AfterPrefix.FindLastChar(TEXT('-'), DashIndex))
        {
            return AfterPrefix.Left(DashIndex);
        }
    }

    // Couldn't determine which node this line belongs to
    return TEXT("");
}

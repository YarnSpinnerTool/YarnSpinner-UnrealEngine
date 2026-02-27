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

#include "YarnProjectFactory.h"
#include "YarnProgram.h"
#include "YarnSpinnerModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "YarnProjectFactory"

namespace
{
	/**
	 * Normalize a path to a canonical absolute form for consistent comparison.
	 * Converts to absolute path, collapses .., normalizes slashes to forward slashes.
	 */
	FString NormalizeToAbsolute(const FString& InPath)
	{
		FString Result = FPaths::ConvertRelativePathToFull(InPath);
		FPaths::NormalizeFilename(Result);
		FPaths::CollapseRelativeDirectories(Result);
		return Result;
	}

	/**
	 * Resolve sourceFiles glob patterns from a .yarnproject into absolute file paths.
	 * Matches the behaviour of Microsoft.Extensions.FileSystemGlobbing used by the
	 * Yarn Spinner compiler:
	 * - "**\/*.yarn" matches recursively
	 * - "*.yarn" matches in project directory only
	 * - "Dir/*.yarn" matches in a specific subdirectory
	 * - Absolute paths with .yarn extension are added directly
	 * - excludeFiles patterns are subtracted from the results
	 */
	TArray<FString> ResolveSourceFileGlobs(const TArray<FString>& IncludePatterns,
		const TArray<FString>& ExcludePatterns, const FString& ProjectDir)
	{
		TArray<FString> Result;

		// Ensure ProjectDir is absolute and normalized
		FString AbsProjectDir = NormalizeToAbsolute(ProjectDir);

		for (const FString& RawPattern : IncludePatterns)
		{
			// Normalize separators in the pattern itself (Windows .yarnproject files
			// could have backslashes, though JSON typically uses forward slashes)
			FString Pattern = RawPattern;
			Pattern.ReplaceInline(TEXT("\\"), TEXT("/"));

			// Handle absolute paths as direct file references
			if (FPaths::IsRelative(Pattern) == false && Pattern.EndsWith(TEXT(".yarn")))
			{
				FString AbsPath = NormalizeToAbsolute(Pattern);
				if (IFileManager::Get().FileExists(*AbsPath))
				{
					Result.AddUnique(AbsPath);
				}
				continue;
			}

			bool bRecursive = Pattern.Contains(TEXT("**"));

			// Extract the extension from the pattern's filename part.
			// Find the last segment after the final / to get the filename glob (e.g. "*.yarn")
			FString FileGlob;
			FString PatternDir;
			int32 LastSlash;
			if (Pattern.FindLastChar(TEXT('/'), LastSlash))
			{
				PatternDir = Pattern.Left(LastSlash);
				FileGlob = Pattern.Mid(LastSlash + 1);
			}
			else
			{
				FileGlob = Pattern;
			}

			// Get extension from the file glob (e.g. "*.yarn" -> ".yarn")
			FString Extension;
			int32 DotIdx;
			if (FileGlob.FindLastChar(TEXT('.'), DotIdx))
			{
				Extension = FileGlob.Mid(DotIdx); // Includes the dot
			}
			if (Extension.IsEmpty())
			{
				Extension = TEXT(".yarn");
			}

			// Build the search directory by stripping ** from the directory part
			FString SearchDir = AbsProjectDir;
			if (!PatternDir.IsEmpty())
			{
				// Remove ** segments from the directory path
				FString CleanDir = PatternDir;
				CleanDir.ReplaceInline(TEXT("**"), TEXT(""));
				CleanDir.ReplaceInline(TEXT("//"), TEXT("/"));
				// Trim leading/trailing slashes left over from stripping
				while (CleanDir.StartsWith(TEXT("/")))
				{
					CleanDir.RightChopInline(1);
				}
				while (CleanDir.EndsWith(TEXT("/")))
				{
					CleanDir.LeftChopInline(1);
				}
				if (!CleanDir.IsEmpty())
				{
					SearchDir = FPaths::Combine(AbsProjectDir, CleanDir);
				}
			}
			SearchDir = NormalizeToAbsolute(SearchDir);

			FString WildcardFilter = TEXT("*") + Extension;

			if (bRecursive)
			{
				TArray<FString> FoundFiles;
				IFileManager::Get().FindFilesRecursive(FoundFiles, *SearchDir, *WildcardFilter, true, false);
				for (FString& FilePath : FoundFiles)
				{
					FilePath = NormalizeToAbsolute(FilePath);
					Result.AddUnique(FilePath);
				}
			}
			else
			{
				TArray<FString> FoundFiles;
				IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(SearchDir, WildcardFilter), true, false);
				for (const FString& FileName : FoundFiles)
				{
					FString FullPath = NormalizeToAbsolute(FPaths::Combine(SearchDir, FileName));
					Result.AddUnique(FullPath);
				}
			}
		}

		// Apply exclude patterns
		if (ExcludePatterns.Num() > 0)
		{
			TArray<FString> ExcludedFiles = ResolveSourceFileGlobs(ExcludePatterns, TArray<FString>(), ProjectDir);
			Result.RemoveAll([&ExcludedFiles](const FString& Path)
			{
				return ExcludedFiles.Contains(Path);
			});
		}

		return Result;
	}
}

UYarnProjectFactory::UYarnProjectFactory()
{
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	SupportedClass = UYarnProject::StaticClass();
	Formats.Add(TEXT("yarnproject;Yarn Spinner Project"));
}

bool UYarnProjectFactory::FactoryCanImport(const FString& Filename)
{
	return Filename.EndsWith(TEXT(".yarnproject"));
}

UObject* UYarnProjectFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	// Compile the yarn project
	FString CompiledPath, LinesPath, MetadataPath, Error;
	if (!CompileYarnProject(Filename, CompiledPath, LinesPath, MetadataPath, Error))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Failed to compile Yarn project: %s"), *Error);
		return nullptr;
	}

	// Create the asset
	UYarnProject* YarnProject = NewObject<UYarnProject>(InParent, InClass, InName, Flags);

	// Parse the compiled program
	if (!ParseCompiledProgram(CompiledPath, YarnProject->Program, Error))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Failed to parse compiled Yarn program: %s"), *Error);
		return nullptr;
	}

	// Parse the string table
	if (!ParseLinesCSV(LinesPath, YarnProject->BaseStringTable, Error))
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Failed to parse lines CSV: %s"), *Error);
	}

	// Parse the metadata
	if (!ParseMetadataCSV(MetadataPath, YarnProject->LineMetadata, Error))
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Failed to parse metadata CSV: %s"), *Error);
	}

	// Parse the .yarnproject JSON for localization settings
	FString ProjectDir = FPaths::GetPath(Filename);
	ParseYarnProjectLocalization(Filename, ProjectDir, YarnProject, Error);

#if WITH_EDITORONLY_DATA
	// Store the source project path for file watching (absolute, normalized)
	YarnProject->SourceProjectPath = NormalizeToAbsolute(Filename);
#endif

	// Populate node names
	for (const auto& Pair : YarnProject->Program.Nodes)
	{
		YarnProject->NodeNames.Add(Pair.Key);
	}

	// Set up asset import data for reimport support
	if (!YarnProject->AssetImportData)
	{
		YarnProject->AssetImportData = NewObject<UAssetImportData>(YarnProject, TEXT("AssetImportData"));
	}
	YarnProject->AssetImportData->Update(Filename);
	YarnProject->MarkPackageDirty();

	// Clean up temp files
	IFileManager::Get().Delete(*CompiledPath);
	IFileManager::Get().Delete(*LinesPath);
	IFileManager::Get().Delete(*MetadataPath);

	return YarnProject;
}

bool UYarnProjectFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UYarnProject* YarnProject = Cast<UYarnProject>(Obj);
	if (YarnProject && YarnProject->AssetImportData)
	{
		YarnProject->AssetImportData->ExtractFilenames(OutFilenames);
		return OutFilenames.Num() > 0;
	}
	return false;
}

void UYarnProjectFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UYarnProject* YarnProject = Cast<UYarnProject>(Obj);
	if (YarnProject && YarnProject->AssetImportData && NewReimportPaths.Num() > 0)
	{
		YarnProject->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UYarnProjectFactory::Reimport(UObject* Obj)
{
	UYarnProject* YarnProject = Cast<UYarnProject>(Obj);
	if (!YarnProject || !YarnProject->AssetImportData)
	{
		return EReimportResult::Failed;
	}

	FString SourcePath = YarnProject->AssetImportData->GetFirstFilename();
	if (SourcePath.IsEmpty() || !IFileManager::Get().FileExists(*SourcePath))
	{
		return EReimportResult::Failed;
	}

	// Compile the yarn project
	FString CompiledPath, LinesPath, MetadataPath, Error;
	if (!CompileYarnProject(SourcePath, CompiledPath, LinesPath, MetadataPath, Error))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Failed to compile Yarn project during reimport: %s"), *Error);
		return EReimportResult::Failed;
	}

	// Clear existing data
	YarnProject->Program = FYarnProgram();
	YarnProject->BaseStringTable.Empty();
	YarnProject->LineMetadata.Empty();
	YarnProject->NodeNames.Empty();
	YarnProject->Localizations.Empty();
	YarnProject->BaseLanguage.Empty();
#if WITH_EDITORONLY_DATA
	YarnProject->ResolvedSourceFiles.Empty();
#endif

	// Parse the compiled program
	if (!ParseCompiledProgram(CompiledPath, YarnProject->Program, Error))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Failed to parse compiled Yarn program during reimport: %s"), *Error);
		return EReimportResult::Failed;
	}

	// Parse the string table
	if (!ParseLinesCSV(LinesPath, YarnProject->BaseStringTable, Error))
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Failed to parse lines CSV during reimport: %s"), *Error);
	}

	// Parse the metadata
	if (!ParseMetadataCSV(MetadataPath, YarnProject->LineMetadata, Error))
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("Failed to parse metadata CSV during reimport: %s"), *Error);
	}

	// Parse the .yarnproject JSON for localization settings
	FString ProjectDir = FPaths::GetPath(SourcePath);
	ParseYarnProjectLocalization(SourcePath, ProjectDir, YarnProject, Error);

#if WITH_EDITORONLY_DATA
	// Store the source project path for file watching (absolute, normalized)
	YarnProject->SourceProjectPath = NormalizeToAbsolute(SourcePath);
#endif

	// Populate node names
	for (const auto& Pair : YarnProject->Program.Nodes)
	{
		YarnProject->NodeNames.Add(Pair.Key);
	}

	// Update the import data timestamp
	YarnProject->AssetImportData->Update(SourcePath);
	YarnProject->MarkPackageDirty();

	// Clean up temp files
	IFileManager::Get().Delete(*CompiledPath);
	IFileManager::Get().Delete(*LinesPath);
	IFileManager::Get().Delete(*MetadataPath);

	return EReimportResult::Succeeded;
}

bool UYarnProjectFactory::CompileYarnProject(const FString& ProjectPath, FString& OutCompiledPath, FString& OutLinesPath,
	FString& OutMetadataPath, FString& OutError)
{
	FString YscPath = GetYscPath();
	if (YscPath.IsEmpty())
	{
		OutError = TEXT("Could not find ysc executable");
		return false;
	}

	// Create temp directory for output
	FString TempDir = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(), TEXT("YarnCompile_"));
	IFileManager::Get().MakeDirectory(*TempDir, true);

	FString OutputName = FPaths::GetBaseFilename(ProjectPath);
	OutCompiledPath = FPaths::Combine(TempDir, OutputName + TEXT(".yarnc"));
	OutLinesPath = FPaths::Combine(TempDir, OutputName + TEXT("-Lines.csv"));
	OutMetadataPath = FPaths::Combine(TempDir, OutputName + TEXT("-Metadata.csv"));

	// Build the command line
	FString CommandLine = FString::Printf(TEXT("compile \"%s\" -o \"%s\" -n \"%s\""),
		*ProjectPath, *TempDir, *OutputName);

	// Run ysc
	int32 ReturnCode = 0;
	FString StdOut, StdErr;
	bool bLaunchSuccess = FPlatformProcess::ExecProcess(*YscPath, *CommandLine, &ReturnCode, &StdOut, &StdErr);

	if (!bLaunchSuccess)
	{
		OutError = FString::Printf(TEXT("Failed to launch ysc: %s"), *YscPath);
		return false;
	}

	if (ReturnCode != 0)
	{
		OutError = FString::Printf(TEXT("ysc returned error code %d: %s"), ReturnCode, *StdErr);
		return false;
	}

	// Check that the output files exist
	if (!IFileManager::Get().FileExists(*OutCompiledPath))
	{
		OutError = TEXT("Compiled file was not created");
		return false;
	}

	return true;
}

bool UYarnProjectFactory::ParseCompiledProgram(const FString& CompiledPath, FYarnProgram& OutProgram, FString& OutError)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *CompiledPath))
	{
		OutError = FString::Printf(TEXT("Failed to load compiled file: %s"), *CompiledPath);
		return false;
	}

	FYarnProtobufParser Parser(FileData);
	return Parser.ParseProgram(OutProgram, OutError);
}

bool UYarnProjectFactory::ParseLinesCSV(const FString& LinesPath, TMap<FString, FString>& OutStringTable, FString& OutError)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *LinesPath))
	{
		OutError = FString::Printf(TEXT("Failed to load lines file: %s"), *LinesPath);
		return false;
	}

	// Skip header
	for (int32 i = 1; i < Lines.Num(); i++)
	{
		const FString& Line = Lines[i];
		if (Line.IsEmpty()) continue;

		// Parse CSV: id,text,file,node,lineNumber
		// Use proper CSV parsing that handles escaped quotes ("")
		TArray<FString> Fields;
		ParseCSVLine(Line, Fields);

		if (Fields.Num() >= 2)
		{
			OutStringTable.Add(Fields[0], Fields[1]);
		}
	}

	return true;
}

bool UYarnProjectFactory::ParseMetadataCSV(const FString& MetadataPath, TMap<FString, FString>& OutMetadata, FString& OutError)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *MetadataPath))
	{
		OutError = FString::Printf(TEXT("Failed to load metadata file: %s"), *MetadataPath);
		return false;
	}

	// Skip header
	for (int32 i = 1; i < Lines.Num(); i++)
	{
		const FString& Line = Lines[i];
		if (Line.IsEmpty()) continue;

		// Parse CSV: id,node,lineNumber,tags
		// Use proper CSV parsing that handles escaped quotes ("")
		TArray<FString> Fields;
		ParseCSVLine(Line, Fields);

		if (Fields.Num() >= 4)
		{
			OutMetadata.Add(Fields[0], Fields[3]);
		}
	}

	return true;
}

FString UYarnProjectFactory::GetYscPath() const
{
	TArray<FString> PossiblePaths;

#if PLATFORM_WINDOWS
	// Windows paths
	FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (!UserProfile.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT(".dotnet/tools/ysc.exe")));
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT(".yarn-spinner/ysc.exe")));
	}
	FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
	if (!LocalAppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(LocalAppData, TEXT("Microsoft/dotnet/tools/ysc.exe")));
		PossiblePaths.Add(FPaths::Combine(LocalAppData, TEXT("YarnSpinner/ysc.exe")));
	}
	FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	if (!AppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(AppData, TEXT("YarnSpinner/ysc.exe")));
	}
	FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
	if (!ProgramFiles.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(ProgramFiles, TEXT("dotnet/tools/ysc.exe")));
		PossiblePaths.Add(FPaths::Combine(ProgramFiles, TEXT("YarnSpinner/ysc.exe")));
	}
	FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));
	if (!ProgramFilesX86.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(ProgramFilesX86, TEXT("dotnet/tools/ysc.exe")));
		PossiblePaths.Add(FPaths::Combine(ProgramFilesX86, TEXT("YarnSpinner/ysc.exe")));
	}
	// Common hardcoded paths
	PossiblePaths.Add(TEXT("C:/Program Files/dotnet/tools/ysc.exe"));
	PossiblePaths.Add(TEXT("C:/Program Files/YarnSpinner/ysc.exe"));
	PossiblePaths.Add(TEXT("ysc.exe")); // Rely on PATH
#else
	// macOS and Linux paths
	FString HomeDir = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	PossiblePaths.Add(TEXT("/usr/local/bin/ysc"));
	if (!HomeDir.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(HomeDir, TEXT(".dotnet/tools/ysc")));
	}
	PossiblePaths.Add(TEXT("/opt/homebrew/bin/ysc"));
	PossiblePaths.Add(TEXT("ysc")); // Rely on PATH
#endif

	for (const FString& Path : PossiblePaths)
	{
		if (IFileManager::Get().FileExists(*Path))
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("Found ysc at: %s"), *Path);
			return Path;
		}
	}

	// Fall back to assuming ysc is available on PATH
	UE_LOG(LogYarnSpinner, Warning, TEXT("Could not find ysc in common locations, trying PATH"));
	return TEXT("ysc");
}

bool UYarnProjectFactory::ParseYarnProjectLocalization(const FString& ProjectPath, const FString& ProjectDir,
	UYarnProject* OutYarnProject, FString& OutError)
{
	// Load the .yarnproject JSON file
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *ProjectPath))
	{
		OutError = FString::Printf(TEXT("Failed to load .yarnproject file: %s"), *ProjectPath);
		return false;
	}

	// Parse the JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutError = TEXT("Failed to parse .yarnproject JSON");
		return false;
	}

	// Get baseLanguage
	FString BaseLanguage;
	if (JsonObject->TryGetStringField(TEXT("baseLanguage"), BaseLanguage))
	{
		OutYarnProject->BaseLanguage = BaseLanguage;
		UE_LOG(LogYarnSpinner, Log, TEXT("Base language: %s"), *BaseLanguage);
	}

#if WITH_EDITORONLY_DATA
	// Parse sourceFiles and excludeFiles globs, resolve to actual file paths.
	// sourceFiles defaults to ["**/*.yarn"], excludeFiles defaults to [].
	{
		TArray<FString> IncludePatterns;
		TArray<FString> ExcludePatterns;

		const TArray<TSharedPtr<FJsonValue>>* SourceFilesArray;
		if (JsonObject->TryGetArrayField(TEXT("sourceFiles"), SourceFilesArray))
		{
			for (const TSharedPtr<FJsonValue>& Value : *SourceFilesArray)
			{
				FString Pattern;
				if (Value->TryGetString(Pattern))
				{
					IncludePatterns.Add(Pattern);
				}
			}
		}
		else
		{
			// Default to **/*.yarn if sourceFiles is not specified (matches compiler default)
			IncludePatterns.Add(TEXT("**/*.yarn"));
		}

		const TArray<TSharedPtr<FJsonValue>>* ExcludeFilesArray;
		if (JsonObject->TryGetArrayField(TEXT("excludeFiles"), ExcludeFilesArray))
		{
			for (const TSharedPtr<FJsonValue>& Value : *ExcludeFilesArray)
			{
				FString Pattern;
				if (Value->TryGetString(Pattern))
				{
					ExcludePatterns.Add(Pattern);
				}
			}
		}

		OutYarnProject->ResolvedSourceFiles = ResolveSourceFileGlobs(IncludePatterns, ExcludePatterns, ProjectDir);
		UE_LOG(LogYarnSpinner, Log, TEXT("Resolved %d source files from %d include patterns (%d exclude patterns)"),
			OutYarnProject->ResolvedSourceFiles.Num(), IncludePatterns.Num(), ExcludePatterns.Num());
	}
#endif

	// Get localisation object
	const TSharedPtr<FJsonObject>* LocalisationObject;
	if (JsonObject->TryGetObjectField(TEXT("localisation"), LocalisationObject))
	{
		// Iterate over each culture in the localisation object
		for (const auto& CulturePair : (*LocalisationObject)->Values)
		{
			FString CultureCode = CulturePair.Key;
			const TSharedPtr<FJsonObject>* CultureObject;

			if (CulturePair.Value->TryGetObject(CultureObject))
			{
				FYarnLocalization Localization;

				// Get strings CSV path
				FString StringsPath;
				if ((*CultureObject)->TryGetStringField(TEXT("strings"), StringsPath))
				{
					// Resolve relative path
					FString FullStringsPath = FPaths::Combine(ProjectDir, StringsPath);
					FPaths::NormalizeFilename(FullStringsPath);

					Localization.SourceCSVPath = FullStringsPath;

					// Load the CSV
					if (IFileManager::Get().FileExists(*FullStringsPath))
					{
						FString ParseError;
						if (ParseLocalizationCSV(FullStringsPath, Localization.Strings, ParseError))
						{
							UE_LOG(LogYarnSpinner, Log, TEXT("Loaded %d strings for culture '%s' from %s"),
								Localization.Strings.Num(), *CultureCode, *StringsPath);
						}
						else
						{
							UE_LOG(LogYarnSpinner, Warning, TEXT("Failed to parse localization CSV for culture '%s': %s"),
								*CultureCode, *ParseError);
						}
					}
					else
					{
						UE_LOG(LogYarnSpinner, Warning, TEXT("Localization CSV not found for culture '%s': %s"),
							*CultureCode, *FullStringsPath);
					}
				}

				// Get assets path
				FString AssetsPath;
				if ((*CultureObject)->TryGetStringField(TEXT("assets"), AssetsPath))
				{
					Localization.AssetsPath = FPaths::Combine(ProjectDir, AssetsPath);
					FPaths::NormalizeFilename(Localization.AssetsPath);
				}

				// Add to localizations map
				OutYarnProject->Localizations.Add(CultureCode, Localization);
			}
		}
	}

	return true;
}

bool UYarnProjectFactory::ParseLocalizationCSV(const FString& CSVPath, TMap<FString, FString>& OutStrings, FString& OutError)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *CSVPath))
	{
		OutError = FString::Printf(TEXT("Failed to load CSV file: %s"), *CSVPath);
		return false;
	}

	if (Lines.Num() == 0)
	{
		OutError = TEXT("CSV file is empty");
		return false;
	}

	// Parse header to find column indices
	// Expected columns: language,id,text,file,node,lineNumber,lock,comment
	// or: id,text,file,node,lineNumber (simpler format)
	TArray<FString> HeaderFields;
	ParseCSVLine(Lines[0], HeaderFields);

	int32 IdIndex = INDEX_NONE;
	int32 TextIndex = INDEX_NONE;

	for (int32 i = 0; i < HeaderFields.Num(); i++)
	{
		FString Field = HeaderFields[i].TrimStartAndEnd().ToLower();
		if (Field == TEXT("id"))
		{
			IdIndex = i;
		}
		else if (Field == TEXT("text"))
		{
			TextIndex = i;
		}
	}

	if (IdIndex == INDEX_NONE || TextIndex == INDEX_NONE)
	{
		OutError = TEXT("CSV missing required 'id' and 'text' columns");
		return false;
	}

	// Parse data rows
	for (int32 i = 1; i < Lines.Num(); i++)
	{
		const FString& Line = Lines[i];
		if (Line.IsEmpty()) continue;

		TArray<FString> Fields;
		ParseCSVLine(Line, Fields);

		if (Fields.Num() > FMath::Max(IdIndex, TextIndex))
		{
			FString Id = Fields[IdIndex].TrimStartAndEnd();
			FString Text = Fields[TextIndex].TrimStartAndEnd();

			if (!Id.IsEmpty())
			{
				OutStrings.Add(Id, Text);
			}
		}
	}

	return true;
}

void UYarnProjectFactory::ParseCSVLine(const FString& Line, TArray<FString>& OutFields)
{
	OutFields.Empty();
	FString Current;
	bool bInQuotes = false;

	for (int32 i = 0; i < Line.Len(); i++)
	{
		TCHAR Char = Line[i];

		if (Char == TEXT('"'))
		{
			// Check for escaped quote
			if (bInQuotes && i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
			{
				Current.AppendChar(TEXT('"'));
				i++; // Skip the next quote
			}
			else
			{
				bInQuotes = !bInQuotes;
			}
		}
		else if (Char == TEXT(',') && !bInQuotes)
		{
			OutFields.Add(Current);
			Current.Empty();
		}
		else
		{
			Current.AppendChar(Char);
		}
	}

	OutFields.Add(Current);
}

// Protobuf parser implementation

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

uint64 FYarnProtobufParser::ReadFixed64()
{
	if (Position + 8 > Data.Num())
	{
		return 0;
	}

	uint64 Result = 0;
	for (int32 i = 0; i < 8; i++)
	{
		Result |= static_cast<uint64>(Data[Position + i]) << (i * 8);
	}
	Position += 8;
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
				// Only add valid instructions (skip Invalid, e.g. from unsupported field tags like pushNull)
				if (Instruction.Type != EYarnInstructionType::Invalid)
				{
					OutNode.Instructions.Add(Instruction);
				}
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
						OutInstruction.FloatOperand = *reinterpret_cast<float*>(&Bits);
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

		case 24: // pushNull - not in proto v2 instruction set, compiler never generates this
			UE_LOG(LogYarnSpinner, Warning, TEXT("Encountered field tag 24 (pushNull) which is not in the v2 instruction set - ignoring"));
			ReadBytes();  // Consume bytes but don't create an instruction
			OutInstruction.Type = EYarnInstructionType::Invalid;
			break;

		default:
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
				OutValue = FYarnValue(*reinterpret_cast<float*>(&Bits));
			}
			break;

		default:
			SkipField(WireType);
			break;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

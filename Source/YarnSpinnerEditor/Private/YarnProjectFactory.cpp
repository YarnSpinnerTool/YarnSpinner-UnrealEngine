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
#include "YarnYSLSGenerator.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "EditorFramework/AssetImportData.h"
#include "YarnEditorPaths.h"
#include "YarnSpinnerEditorSettings.h"

#define LOCTEXT_NAMESPACE "YarnProjectFactory"

namespace
{
	// Resolve sourceFiles glob patterns from a .yarnproject into absolute file paths.
	// Matches the behaviour of Microsoft.Extensions.FileSystemGlobbing used by the
	// Yarn Spinner compiler:
	// - "**" + "/*.yarn" matches recursively
	// - "*.yarn" matches in project directory only
	// - "Dir" + "/*.yarn" matches in a specific subdirectory
	// - Absolute paths with .yarn extension are added directly
	// - excludeFiles patterns are subtracted from the results
	TArray<FString> ResolveSourceFileGlobs(const TArray<FString>& IncludePatterns,
		const TArray<FString>& ExcludePatterns, const FString& ProjectDir)
	{
		TArray<FString> Result;

		// Ensure ProjectDir is absolute and normalized
		FString AbsProjectDir = YarnEditorPaths::NormalizeToAbsolute(ProjectDir);

		for (const FString& RawPattern : IncludePatterns)
		{
			// Normalize separators in the pattern itself (Windows .yarnproject files
			// could have backslashes, though JSON typically uses forward slashes)
			FString Pattern = RawPattern;
			Pattern.ReplaceInline(TEXT("\\"), TEXT("/"));

			// Handle absolute paths as direct file references
			if (FPaths::IsRelative(Pattern) == false && Pattern.EndsWith(TEXT(".yarn")))
			{
				FString AbsPath = YarnEditorPaths::NormalizeToAbsolute(Pattern);
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
			SearchDir = YarnEditorPaths::NormalizeToAbsolute(SearchDir);

			FString WildcardFilter = TEXT("*") + Extension;

			if (bRecursive)
			{
				TArray<FString> FoundFiles;
				IFileManager::Get().FindFilesRecursive(FoundFiles, *SearchDir, *WildcardFilter, true, false);
				for (FString& FilePath : FoundFiles)
				{
					FilePath = YarnEditorPaths::NormalizeToAbsolute(FilePath);
					Result.AddUnique(FilePath);
				}
			}
			else
			{
				TArray<FString> FoundFiles;
				IFileManager::Get().FindFiles(FoundFiles, *FPaths::Combine(SearchDir, WildcardFilter), true, false);
				for (const FString& FileName : FoundFiles)
				{
					FString FullPath = YarnEditorPaths::NormalizeToAbsolute(FPaths::Combine(SearchDir, FileName));
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

bool UYarnProjectFactory::ImportProjectData(UYarnProject* YarnProject, const FString& SourcePath)
{
	FString CompiledPath, LinesPath, MetadataPath, Error;
	TArray<FYarnProjectDiagnostic> Diagnostics;
	if (!CompileYarnProject(SourcePath, CompiledPath, LinesPath, MetadataPath, Diagnostics, Error))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Failed to compile Yarn project: %s"), *Error);
		return false;
	}

	const FString CompileTempDir = FPaths::GetPath(CompiledPath);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().DeleteDirectory(*CompileTempDir, /*RequireExists=*/false, /*Tree=*/true);
	};

	// Retain compiler diagnostics on the asset so warnings stay visible
	YarnProject->ImportDiagnostics = Diagnostics;

	// Parse the compiled program
	if (!ParseCompiledProgram(CompiledPath, YarnProject->Program, Error))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("Failed to parse compiled Yarn program: %s"), *Error);
		return false;
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
	ParseYarnProjectLocalization(SourcePath, FPaths::GetPath(SourcePath), YarnProject, Error);

#if WITH_EDITORONLY_DATA
	// Store the source project path for file watching (absolute, normalized)
	YarnProject->SourceProjectPath = YarnEditorPaths::NormalizeToAbsolute(SourcePath);
#endif

	// Populate node names
	for (const auto& Pair : YarnProject->Program.Nodes)
	{
		YarnProject->NodeNames.Add(Pair.Key);
	}

	return true;
}

UObject* UYarnProjectFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	UYarnProject* YarnProject = NewObject<UYarnProject>(InParent, InClass, InName, Flags);

	if (!ImportProjectData(YarnProject, Filename))
	{
		return nullptr;
	}

	// Set up asset import data for reimport support
	if (!YarnProject->AssetImportData)
	{
		YarnProject->AssetImportData = NewObject<UAssetImportData>(YarnProject, TEXT("AssetImportData"));
	}
	YarnProject->AssetImportData->Update(Filename);
	YarnProject->MarkPackageDirty();

	FYarnYSLSGenerator::GenerateForProject(Filename);

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

	YarnProject->Program = FYarnProgram();
	YarnProject->BaseStringTable.Empty();
	YarnProject->LineMetadata.Empty();
	YarnProject->NodeNames.Empty();
	YarnProject->Localizations.Empty();
	YarnProject->BaseLanguage.Empty();
#if WITH_EDITORONLY_DATA
	YarnProject->ResolvedSourceFiles.Empty();
#endif

	if (!ImportProjectData(YarnProject, SourcePath))
	{
		return EReimportResult::Failed;
	}

	// Update the import data timestamp
	YarnProject->AssetImportData->Update(SourcePath);
	YarnProject->MarkPackageDirty();

	FYarnYSLSGenerator::GenerateForProject(SourcePath);

	return EReimportResult::Succeeded;
}

void UYarnProjectFactory::ParseCompilerDiagnostics(const FString& CompilerOutput, TArray<FYarnProjectDiagnostic>& OutDiagnostics)
{
	// ysc prints diagnostics as lines of the form:
	//   <emoji> WARNING: <file>: <line>:<column> <message>
	//   <emoji> ERROR: <file>: <line>:<column> <message>
	// The file/location prefix is omitted when the diagnostic has no source
	// location, so both shapes have to parse.

	TArray<FString> Lines;
	CompilerOutput.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		FString Severity;
		int32 MarkerIndex = Line.Find(TEXT("WARNING: "));
		if (MarkerIndex != INDEX_NONE)
		{
			Severity = TEXT("Warning");
			MarkerIndex += 9; // length of "WARNING: "
		}
		else
		{
			MarkerIndex = Line.Find(TEXT("ERROR: "));
			if (MarkerIndex == INDEX_NONE)
			{
				continue;
			}
			Severity = TEXT("Error");
			MarkerIndex += 7; // length of "ERROR: "
		}

		FYarnProjectDiagnostic Diagnostic;
		Diagnostic.Severity = Severity;

		FString Rest = Line.Mid(MarkerIndex);

		// Try to split off "<file>: <line>:<column> " from the front
		FRegexPattern LocationPattern(TEXT("^(.+?): ([0-9]+):([0-9]+) (.*)$"));
		FRegexMatcher LocationMatcher(LocationPattern, Rest);
		if (LocationMatcher.FindNext())
		{
			Diagnostic.FilePath = LocationMatcher.GetCaptureGroup(1);
			Diagnostic.Line = FCString::Atoi(*LocationMatcher.GetCaptureGroup(2));
			Diagnostic.Column = FCString::Atoi(*LocationMatcher.GetCaptureGroup(3));
			Diagnostic.Message = LocationMatcher.GetCaptureGroup(4);
		}
		else
		{
			Diagnostic.Message = Rest;
		}

		OutDiagnostics.Add(Diagnostic);
	}
}

bool UYarnProjectFactory::CompileYarnProject(const FString& ProjectPath, FString& OutCompiledPath, FString& OutLinesPath,
	FString& OutMetadataPath, TArray<FYarnProjectDiagnostic>& OutDiagnostics, FString& OutError)
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
	constexpr double YscTimeoutSeconds = 60.0;

	int32 ReturnCode = 0;
	FString StdOut, StdErr;

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	FProcHandle ProcHandle = FPlatformProcess::CreateProc(*YscPath, *CommandLine,
		/*bLaunchDetached=*/false, /*bLaunchHidden=*/true, /*bLaunchReallyHidden=*/true,
		nullptr, 0, nullptr, WritePipe, nullptr, WritePipe);

	if (!ProcHandle.IsValid())
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		OutError = FString::Printf(TEXT("Failed to launch ysc: %s"), *YscPath);
		return false;
	}

	const double StartTime = FPlatformTime::Seconds();
	while (FPlatformProcess::IsProcRunning(ProcHandle))
	{
		StdOut += FPlatformProcess::ReadPipe(ReadPipe);

		if (FPlatformTime::Seconds() - StartTime > YscTimeoutSeconds)
		{
			FPlatformProcess::TerminateProc(ProcHandle, /*KillTree=*/true);
			FPlatformProcess::CloseProc(ProcHandle);
			FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
			OutError = FString::Printf(TEXT("ysc did not finish within %.0f seconds and was terminated"), YscTimeoutSeconds);
			return false;
		}

		FPlatformProcess::Sleep(0.05f);
	}

	StdOut += FPlatformProcess::ReadPipe(ReadPipe);
	FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
	FPlatformProcess::CloseProc(ProcHandle);
	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	// Collect compiler diagnostics from the output. Warnings don't fail the
	// compile, but they're retained on the project asset and logged with
	// source locations so problems stay visible after import.
	ParseCompilerDiagnostics(StdOut + TEXT("\n") + StdErr, OutDiagnostics);
	for (const FYarnProjectDiagnostic& Diagnostic : OutDiagnostics)
	{
		if (Diagnostic.Severity == TEXT("Error"))
		{
			UE_LOG(LogYarnSpinner, Error, TEXT("%s(%d,%d): %s"), *Diagnostic.FilePath, Diagnostic.Line, Diagnostic.Column, *Diagnostic.Message);
		}
		else
		{
			UE_LOG(LogYarnSpinner, Warning, TEXT("%s(%d,%d): %s"), *Diagnostic.FilePath, Diagnostic.Line, Diagnostic.Column, *Diagnostic.Message);
		}
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
	if (!Parser.ParseProgram(OutProgram, OutError))
	{
		return false;
	}

	constexpr int32 MaxKnownLanguageVersion = 4;
	if (OutProgram.LanguageVersion > MaxKnownLanguageVersion)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("yarn project importer: program language version %d is newer than this plugin supports (%d) - update the plugin if dialogue misbehaves"),
			OutProgram.LanguageVersion, MaxKnownLanguageVersion);
	}
	return true;
}

bool UYarnProjectFactory::ParseLinesCSV(const FString& LinesPath, TMap<FString, FString>& OutStringTable, FString& OutError)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *LinesPath))
	{
		OutError = FString::Printf(TEXT("Failed to load lines file: %s"), *LinesPath);
		return false;
	}

	TArray<TArray<FString>> Records;
	ParseCSVRecords(Content, Records);
	for (int32 i = 1; i < Records.Num(); i++)
	{
		const TArray<FString>& Fields = Records[i];
		if (Fields.Num() >= 2)
		{
			OutStringTable.Add(Fields[0], Fields[1]);
		}
	}

	return true;
}

bool UYarnProjectFactory::ParseMetadataCSV(const FString& MetadataPath, TMap<FString, FString>& OutMetadata, FString& OutError)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *MetadataPath))
	{
		OutError = FString::Printf(TEXT("Failed to load metadata file: %s"), *MetadataPath);
		return false;
	}

	TArray<TArray<FString>> Records;
	ParseCSVRecords(Content, Records);
	for (int32 i = 1; i < Records.Num(); i++)
	{
		const TArray<FString>& Fields = Records[i];
		if (Fields.Num() >= 4)
		{
			OutMetadata.Add(Fields[0], Fields[3]);
		}
	}

	return true;
}

FString UYarnProjectFactory::GetYscPath() const
{
	const UYarnSpinnerEditorSettings* Settings = GetDefault<UYarnSpinnerEditorSettings>();
	if (!Settings->YscPath.FilePath.IsEmpty())
	{
		if (IFileManager::Get().FileExists(*Settings->YscPath.FilePath))
		{
			return Settings->YscPath.FilePath;
		}
		UE_LOG(LogYarnSpinner, Warning, TEXT("Configured ysc path '%s' does not exist - falling back to discovery"), *Settings->YscPath.FilePath);
	}

	static FString CachedPath;
	if (!CachedPath.IsEmpty() && IFileManager::Get().FileExists(*CachedPath))
	{
		return CachedPath;
	}

	TArray<FString> PossiblePaths;

#if PLATFORM_WINDOWS
	FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (!UserProfile.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT(".dotnet/tools/ysc.exe")));
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT(".yarn-spinner/ysc.exe")));
	}
	FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
	if (!LocalAppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(LocalAppData, TEXT("YarnSpinner/ysc.exe")));
	}
	FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	if (!AppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(AppData, TEXT("YarnSpinner/ysc.exe")));
	}
#else
	// macOS and Linux paths
	FString HomeDir = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	PossiblePaths.Add(TEXT("/usr/local/bin/ysc"));
	if (!HomeDir.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(HomeDir, TEXT(".dotnet/tools/ysc")));
	}
	PossiblePaths.Add(TEXT("/opt/homebrew/bin/ysc"));
#endif

	for (const FString& Path : PossiblePaths)
	{
		if (IFileManager::Get().FileExists(*Path))
		{
			UE_LOG(LogYarnSpinner, Log, TEXT("Found ysc at: %s"), *Path);
			CachedPath = Path;
			return Path;
		}
	}

	UE_LOG(LogYarnSpinner, Warning, TEXT("Could not find ysc in common locations, trying PATH. Set an explicit path in Project Settings > Plugins > Yarn Spinner."));
#if PLATFORM_WINDOWS
	return TEXT("ysc.exe");
#else
	return TEXT("ysc");
#endif
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
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *CSVPath))
	{
		OutError = FString::Printf(TEXT("Failed to load CSV file: %s"), *CSVPath);
		return false;
	}

	TArray<TArray<FString>> Records;
	ParseCSVRecords(Content, Records);

	if (Records.Num() == 0)
	{
		OutError = TEXT("CSV file is empty");
		return false;
	}

	// Parse header to find column indices
	// Expected columns: language,id,text,file,node,lineNumber,lock,comment
	// or: id,text,file,node,lineNumber (simpler format)
	const TArray<FString>& HeaderFields = Records[0];

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
	for (int32 i = 1; i < Records.Num(); i++)
	{
		const TArray<FString>& Fields = Records[i];

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

void UYarnProjectFactory::ParseCSVRecords(const FString& Content, TArray<TArray<FString>>& OutRecords)
{
	OutRecords.Empty();

	TArray<FString> Fields;
	FString Current;
	bool bInQuotes = false;
	bool bRecordHasContent = false;

	auto EndField = [&]()
	{
		Fields.Add(Current);
		Current.Reset();
	};
	auto EndRecord = [&]()
	{
		if (bRecordHasContent || !Current.IsEmpty() || Fields.Num() > 0)
		{
			EndField();
			OutRecords.Add(MoveTemp(Fields));
			Fields.Reset();
		}
		bRecordHasContent = false;
	};

	const int32 Len = Content.Len();
	for (int32 i = 0; i < Len; i++)
	{
		const TCHAR C = Content[i];

		if (bInQuotes)
		{
			if (C == TEXT('"'))
			{
				if (i + 1 < Len && Content[i + 1] == TEXT('"'))
				{
					Current.AppendChar(TEXT('"'));
					i++;
				}
				else
				{
					bInQuotes = false;
				}
			}
			else
			{
				Current.AppendChar(C);
			}
			bRecordHasContent = true;
			continue;
		}

		switch (C)
		{
		case TEXT('"'):
			bInQuotes = true;
			bRecordHasContent = true;
			break;
		case TEXT(','):
			EndField();
			bRecordHasContent = true;
			break;
		case TEXT('\r'):
			if (i + 1 < Len && Content[i + 1] == TEXT('\n'))
			{
				continue;
			}
			EndRecord();
			break;
		case TEXT('\n'):
			EndRecord();
			break;
		default:
			Current.AppendChar(C);
			bRecordHasContent = true;
			break;
		}
	}
	EndRecord();
}

// Protobuf parser implementation

#undef LOCTEXT_NAMESPACE

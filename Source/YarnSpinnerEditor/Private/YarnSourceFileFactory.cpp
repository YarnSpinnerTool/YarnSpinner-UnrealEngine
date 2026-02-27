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

#include "YarnSourceFileFactory.h"
#include "YarnSourceFile.h"
#include "YarnProgram.h"
#include "YarnSpinnerModule.h"
#include "YarnProjectFactory.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"

//
// UYarnSourceFileFactory - imports .yarn source files
//

UYarnSourceFileFactory::UYarnSourceFileFactory()
{
	bCreateNew = false;
	bEditAfterNew = false;
	bEditorImport = true;
	bText = true;
	ImportPriority = DefaultImportPriority + 1;

	SupportedClass = UYarnSourceFile::StaticClass();

	Formats.Add(TEXT("yarn;Yarn Spinner Source File"));
}

bool UYarnSourceFileFactory::FactoryCanImport(const FString& Filename)
{
	return Filename.EndsWith(TEXT(".yarn"));
}

UObject* UYarnSourceFileFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnSourceFileFactory: Failed to read file %s"), *Filename);
		bOutOperationCanceled = true;
		return nullptr;
	}

	UYarnSourceFile* SourceFile = NewObject<UYarnSourceFile>(InParent, InClass, InName, Flags);
	SourceFile->SourceText = FileContent;
	SourceFile->SourceFilename = Filename;
	SourceFile->ParseNodeTitles();

#if WITH_EDITORONLY_DATA
	if (SourceFile->AssetImportData)
	{
		SourceFile->AssetImportData->Update(Filename);
	}
#endif

	return SourceFile;
}

bool UYarnSourceFileFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UYarnSourceFile* SourceFile = Cast<UYarnSourceFile>(Obj);
	if (SourceFile)
	{
#if WITH_EDITORONLY_DATA
		if (SourceFile->AssetImportData)
		{
			SourceFile->AssetImportData->ExtractFilenames(OutFilenames);
		}
		else
#endif
		{
			OutFilenames.Add(SourceFile->SourceFilename);
		}
		return true;
	}
	return false;
}

void UYarnSourceFileFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UYarnSourceFile* SourceFile = Cast<UYarnSourceFile>(Obj);
	if (SourceFile && NewReimportPaths.Num() > 0)
	{
#if WITH_EDITORONLY_DATA
		if (SourceFile->AssetImportData)
		{
			SourceFile->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
		}
#endif
		SourceFile->SourceFilename = NewReimportPaths[0];
	}
}

EReimportResult::Type UYarnSourceFileFactory::Reimport(UObject* Obj)
{
	UYarnSourceFile* SourceFile = Cast<UYarnSourceFile>(Obj);
	if (!SourceFile)
	{
		return EReimportResult::Failed;
	}

	FString Filename = SourceFile->SourceFilename;
#if WITH_EDITORONLY_DATA
	if (SourceFile->AssetImportData)
	{
		Filename = SourceFile->AssetImportData->GetFirstFilename();
	}
#endif

	if (Filename.IsEmpty() || !FPaths::FileExists(Filename))
	{
		return EReimportResult::Failed;
	}

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		return EReimportResult::Failed;
	}

	SourceFile->SourceText = FileContent;
	SourceFile->SourceFilename = Filename;
	SourceFile->ParseNodeTitles();

#if WITH_EDITORONLY_DATA
	if (SourceFile->AssetImportData)
	{
		SourceFile->AssetImportData->Update(Filename);
	}
#endif

	return EReimportResult::Succeeded;
}

//
// UYarnCompiledFactory - imports .yarnc compiled files directly
//

UYarnCompiledFactory::UYarnCompiledFactory()
{
	bCreateNew = false;
	bEditAfterNew = false;
	bEditorImport = true;
	bText = false;
	ImportPriority = DefaultImportPriority + 1;

	SupportedClass = UYarnProject::StaticClass();

	Formats.Add(TEXT("yarnc;Yarn Spinner Compiled Program"));
}

bool UYarnCompiledFactory::FactoryCanImport(const FString& Filename)
{
	return Filename.EndsWith(TEXT(".yarnc"));
}

UObject* UYarnCompiledFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	// Load the compiled file
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *Filename))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnCompiledFactory: Failed to read file %s"), *Filename);
		bOutOperationCanceled = true;
		return nullptr;
	}

	// Parse the protobuf data
	FYarnProgram Program;
	FString ParseError;
	FYarnProtobufParser Parser(FileData);
	if (!Parser.ParseProgram(Program, ParseError))
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("YarnCompiledFactory: Failed to parse %s: %s"), *Filename, *ParseError);
		bOutOperationCanceled = true;
		return nullptr;
	}

	// Create the YarnProject
	UYarnProject* YarnProject = NewObject<UYarnProject>(InParent, InClass, InName, Flags);
	YarnProject->Program = Program;

#if WITH_EDITORONLY_DATA
	if (YarnProject->AssetImportData)
	{
		YarnProject->AssetImportData->Update(Filename);
	}
#endif

	return YarnProject;
}

bool UYarnCompiledFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UYarnProject* YarnProject = Cast<UYarnProject>(Obj);
	if (YarnProject)
	{
#if WITH_EDITORONLY_DATA
		if (YarnProject->AssetImportData)
		{
			YarnProject->AssetImportData->ExtractFilenames(OutFilenames);
		}
#endif
		return OutFilenames.Num() > 0 && OutFilenames[0].EndsWith(TEXT(".yarnc"));
	}
	return false;
}

void UYarnCompiledFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UYarnProject* YarnProject = Cast<UYarnProject>(Obj);
	if (YarnProject && NewReimportPaths.Num() > 0)
	{
#if WITH_EDITORONLY_DATA
		if (YarnProject->AssetImportData)
		{
			YarnProject->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
		}
#endif
	}
}

EReimportResult::Type UYarnCompiledFactory::Reimport(UObject* Obj)
{
	UYarnProject* YarnProject = Cast<UYarnProject>(Obj);
	if (!YarnProject)
	{
		return EReimportResult::Failed;
	}

	FString Filename;
#if WITH_EDITORONLY_DATA
	if (YarnProject->AssetImportData)
	{
		Filename = YarnProject->AssetImportData->GetFirstFilename();
	}
#endif

	if (Filename.IsEmpty() || !FPaths::FileExists(Filename))
	{
		return EReimportResult::Failed;
	}

	// Load and parse
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *Filename))
	{
		return EReimportResult::Failed;
	}

	FYarnProgram Program;
	FString ParseError;
	FYarnProtobufParser Parser(FileData);
	if (!Parser.ParseProgram(Program, ParseError))
	{
		return EReimportResult::Failed;
	}

	YarnProject->Program = Program;

#if WITH_EDITORONLY_DATA
	if (YarnProject->AssetImportData)
	{
		YarnProject->AssetImportData->Update(Filename);
	}
#endif

	return EReimportResult::Succeeded;
}

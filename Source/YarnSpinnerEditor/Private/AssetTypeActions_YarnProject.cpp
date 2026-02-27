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

#include "AssetTypeActions_YarnProject.h"
#include "YarnProgram.h"
#include "YarnSourceFile.h"
#include "YarnProjectFactory.h"
#include "YarnSourceFileFactory.h"
#include "ToolMenus.h"
#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_YarnProject"

FText FAssetTypeActions_YarnProject::GetName() const
{
	return LOCTEXT("AssetName", "Yarn Project");
}

FColor FAssetTypeActions_YarnProject::GetTypeColor() const
{
	// Yarn Spinner brand colour
	return FColor(138, 43, 226);
}

UClass* FAssetTypeActions_YarnProject::GetSupportedClass() const
{
	return UYarnProject::StaticClass();
}

uint32 FAssetTypeActions_YarnProject::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool FAssetTypeActions_YarnProject::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_YarnProject::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UYarnProject>> YarnProjects;
	for (UObject* Object : InObjects)
	{
		if (UYarnProject* YarnProject = Cast<UYarnProject>(Object))
		{
			YarnProjects.Add(YarnProject);
		}
	}

	// add reimport action
	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		"ReimportYarnProject",
		LOCTEXT("ReimportYarnProject", "Reimport"),
		LOCTEXT("ReimportYarnProjectTooltip", "Reimport the Yarn project from source files."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([YarnProjects]()
			{
				for (TWeakObjectPtr<UYarnProject> YarnProjectPtr : YarnProjects)
				{
					if (UYarnProject* YarnProject = YarnProjectPtr.Get())
					{
						// trigger reimport through the factory
						if (YarnProject->AssetImportData)
						{
							UYarnProjectFactory* Factory = NewObject<UYarnProjectFactory>();
							Factory->Reimport(YarnProject);
						}
					}
				}
			}),
			FCanExecuteAction::CreateLambda([YarnProjects]()
			{
				for (TWeakObjectPtr<UYarnProject> YarnProjectPtr : YarnProjects)
				{
					if (UYarnProject* YarnProject = YarnProjectPtr.Get())
					{
						if (YarnProject->AssetImportData && YarnProject->AssetImportData->GetFirstFilename().Len() > 0)
						{
							return true;
						}
					}
				}
				return false;
			})
		)
	));
}

void FAssetTypeActions_YarnProject::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// open the default property editor which shows all yarn project properties
	FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
}

//
// FAssetTypeActions_YarnSourceFile
//

FText FAssetTypeActions_YarnSourceFile::GetName() const
{
	return LOCTEXT("YarnSourceFileName", "Yarn Source File");
}

FColor FAssetTypeActions_YarnSourceFile::GetTypeColor() const
{
	// a lighter purple for source files
	return FColor(180, 100, 255);
}

UClass* FAssetTypeActions_YarnSourceFile::GetSupportedClass() const
{
	return UYarnSourceFile::StaticClass();
}

uint32 FAssetTypeActions_YarnSourceFile::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool FAssetTypeActions_YarnSourceFile::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_YarnSourceFile::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UYarnSourceFile>> SourceFiles;
	for (UObject* Object : InObjects)
	{
		if (UYarnSourceFile* SourceFile = Cast<UYarnSourceFile>(Object))
		{
			SourceFiles.Add(SourceFile);
		}
	}

	// add reimport action
	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		"ReimportYarnSourceFile",
		LOCTEXT("ReimportYarnSourceFile", "Reimport"),
		LOCTEXT("ReimportYarnSourceFileTooltip", "Reimport the Yarn source file."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([SourceFiles]()
			{
				for (TWeakObjectPtr<UYarnSourceFile> SourceFilePtr : SourceFiles)
				{
					if (UYarnSourceFile* SourceFile = SourceFilePtr.Get())
					{
#if WITH_EDITORONLY_DATA
						if (SourceFile->AssetImportData)
						{
							UYarnSourceFileFactory* Factory = NewObject<UYarnSourceFileFactory>();
							Factory->Reimport(SourceFile);
						}
#endif
					}
				}
			}),
			FCanExecuteAction::CreateLambda([SourceFiles]()
			{
				for (TWeakObjectPtr<UYarnSourceFile> SourceFilePtr : SourceFiles)
				{
					if (UYarnSourceFile* SourceFile = SourceFilePtr.Get())
					{
#if WITH_EDITORONLY_DATA
						if (SourceFile->AssetImportData && SourceFile->AssetImportData->GetFirstFilename().Len() > 0)
						{
							return true;
						}
#endif
					}
				}
				return false;
			})
		)
	));
}

void FAssetTypeActions_YarnSourceFile::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// open the default property editor which shows the source text
	FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
}

#undef LOCTEXT_NAMESPACE

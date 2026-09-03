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

#include "AssetDefinition_YarnProject.h"
#include "YarnProgram.h"
#include "YarnSourceFile.h"
#include "YarnYSLSGenerator.h"
#include "ContentBrowserMenuContexts.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_YarnProject"

// ============================================================================

FText UAssetDefinition_YarnProject::GetAssetDisplayName() const
{
	return LOCTEXT("YarnProjectName", "Yarn Project");
}

FLinearColor UAssetDefinition_YarnProject::GetAssetColor() const
{
	// Yarn Spinner brand colour
	return FLinearColor(FColor(138, 43, 226));
}

TSoftClassPtr<UObject> UAssetDefinition_YarnProject::GetAssetClass() const
{
	return UYarnProject::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_YarnProject::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Misc) };
	return Categories;
}

// ============================================================================

FText UAssetDefinition_YarnSourceFile::GetAssetDisplayName() const
{
	return LOCTEXT("YarnSourceFileName", "Yarn Source File");
}

FLinearColor UAssetDefinition_YarnSourceFile::GetAssetColor() const
{
	// a lighter purple for source files
	return FLinearColor(FColor(180, 100, 255));
}

TSoftClassPtr<UObject> UAssetDefinition_YarnSourceFile::GetAssetClass() const
{
	return UYarnSourceFile::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_YarnSourceFile::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Misc) };
	return Categories;
}

// ============================================================================

namespace
{
	void ReimportSelectedAssets(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			for (UObject* Asset : Context->LoadSelectedObjects<UObject>())
			{
				FReimportManager::Instance()->Reimport(Asset, /*bAskForNewFileIfMissing=*/false, /*bShowNotification=*/true);
			}
		}
	}

	void GenerateYSLSForSelectedProjects(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			TArray<FString> ProjectPaths;
			for (UYarnProject* YarnProject : Context->LoadSelectedObjects<UYarnProject>())
			{
				if (YarnProject->AssetImportData)
				{
					const FString SourcePath = YarnProject->AssetImportData->GetFirstFilename();
					if (!SourcePath.IsEmpty())
					{
						ProjectPaths.Add(SourcePath);
					}
				}
			}
			FYarnYSLSGenerator::GenerateForProjects(ProjectPaths);
		}
	}

	void RegisterYarnAssetMenus()
	{
		{
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UYarnProject::StaticClass());
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

			Section.AddMenuEntry(
				"ReimportYarnProject",
				LOCTEXT("ReimportYarnProject", "Reimport"),
				LOCTEXT("ReimportYarnProjectTooltip", "Reimport the Yarn project from source files."),
				FSlateIcon(),
				FToolMenuExecuteAction::CreateStatic(&ReimportSelectedAssets));

			Section.AddMenuEntry(
				"GenerateYarnYSLS",
				LOCTEXT("GenerateYarnYSLS", "Generate YSLS File"),
				LOCTEXT("GenerateYarnYSLSTooltip", "Scan for Yarn commands and functions and write a .ysls.json file next to the source .yarnproject, for autocomplete in the VS Code extension."),
				FSlateIcon(),
				FToolMenuExecuteAction::CreateStatic(&GenerateYSLSForSelectedProjects));
		}

		{
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UYarnSourceFile::StaticClass());
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

			Section.AddMenuEntry(
				"ReimportYarnSourceFile",
				LOCTEXT("ReimportYarnSourceFile", "Reimport"),
				LOCTEXT("ReimportYarnSourceFileTooltip", "Reimport the Yarn source file."),
				FSlateIcon(),
				FToolMenuExecuteAction::CreateStatic(&ReimportSelectedAssets));
		}
	}

	FDelayedAutoRegisterHelper GYarnAssetMenuRegistration(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterYarnAssetMenus));
	});
}

#undef LOCTEXT_NAMESPACE

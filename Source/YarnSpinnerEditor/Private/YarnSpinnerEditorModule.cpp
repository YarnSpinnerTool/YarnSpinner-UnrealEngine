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

#include "YarnSpinnerEditorModule.h"
#include "YarnSpinnerModule.h"
#include "AssetTypeActions_YarnProject.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Interfaces/IPluginManager.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "YarnSpinnerVersion.h"

#if YARNSPINNER_WITH_ASSET_REGISTRY_SUBDIR
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif
#include "YarnProgram.h"
#include "EditorReimportHandler.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "FYarnSpinnerEditorModule"

void FYarnSpinnerEditorModule::StartupModule()
{
	RegisterAssetTypes();
	RegisterStyleSet();
	SetupSourceFileWatchers();
}

void FYarnSpinnerEditorModule::ShutdownModule()
{
	TeardownSourceFileWatchers();
	UnregisterAssetTypes();
	UnregisterStyleSet();
}

FYarnSpinnerEditorModule& FYarnSpinnerEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FYarnSpinnerEditorModule>("YarnSpinnerEditor");
}

bool FYarnSpinnerEditorModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("YarnSpinnerEditor");
}

void FYarnSpinnerEditorModule::RegisterAssetTypes()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Register yarn project asset type
	TSharedPtr<IAssetTypeActions> YarnProjectActions = MakeShareable(new FAssetTypeActions_YarnProject());
	AssetTools.RegisterAssetTypeActions(YarnProjectActions.ToSharedRef());
	RegisteredAssetTypeActions.Add(YarnProjectActions);

	// Register yarn source file asset type
	TSharedPtr<IAssetTypeActions> YarnSourceFileActions = MakeShareable(new FAssetTypeActions_YarnSourceFile());
	AssetTools.RegisterAssetTypeActions(YarnSourceFileActions.ToSharedRef());
	RegisteredAssetTypeActions.Add(YarnSourceFileActions);
}

void FYarnSpinnerEditorModule::UnregisterAssetTypes()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			if (Action.IsValid())
			{
				AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
			}
		}
	}
	RegisteredAssetTypeActions.Empty();
}

void FYarnSpinnerEditorModule::RegisterStyleSet()
{
	// Find the plugin content directory
	FString PluginBaseDir = IPluginManager::Get().FindPlugin("YarnSpinner")->GetBaseDir();
	FString ResourcesDir = PluginBaseDir / TEXT("Resources");

	// Create the style set
	StyleSet = MakeShareable(new FSlateStyleSet("YarnSpinnerStyle"));
	StyleSet->SetContentRoot(ResourcesDir);

	// Register class icons for thumbnails
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon64x64(64.0f, 64.0f);

	// YarnProject icons
	StyleSet->Set("ClassIcon.YarnProject", new FSlateImageBrush(ResourcesDir / TEXT("yarn-project.png"), Icon16x16));
	StyleSet->Set("ClassThumbnail.YarnProject", new FSlateImageBrush(ResourcesDir / TEXT("yarn-project.png"), Icon64x64));

	// YarnSourceFile icons
	StyleSet->Set("ClassIcon.YarnSourceFile", new FSlateImageBrush(ResourcesDir / TEXT("yarn-file.png"), Icon16x16));
	StyleSet->Set("ClassThumbnail.YarnSourceFile", new FSlateImageBrush(ResourcesDir / TEXT("yarn-file.png"), Icon64x64));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void FYarnSpinnerEditorModule::UnregisterStyleSet()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		StyleSet.Reset();
	}
}

// ============================================================================
// Source file watching
// ============================================================================

void FYarnSpinnerEditorModule::SetupSourceFileWatchers()
{
	// Register asset registry callbacks so we rebuild watchers when YarnProject assets are added/removed
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetAddedHandle = AssetRegistry.OnAssetAdded().AddLambda([this](const FAssetData& AssetData)
	{
#if YARNSPINNER_WITH_ASSET_CLASS_PATH
		if (AssetData.AssetClassPath == UYarnProject::StaticClass()->GetClassPathName())
#else
		if (AssetData.AssetClass == UYarnProject::StaticClass()->GetFName())
#endif
		{
			// Delay rebuild slightly so the asset is fully loaded
			if (GEditor)
			{
				GEditor->GetTimerManager()->SetTimer(RebuildTimerHandle, FTimerDelegate::CreateLambda([this]()
				{
					RebuildWatcherState();
				}), 1.0f, false);
			}
		}
	});

	AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddLambda([this](const FAssetData& AssetData)
	{
#if YARNSPINNER_WITH_ASSET_CLASS_PATH
		if (AssetData.AssetClassPath == UYarnProject::StaticClass()->GetClassPathName())
#else
		if (AssetData.AssetClass == UYarnProject::StaticClass()->GetFName())
#endif
		{
			RebuildWatcherState();
		}
	});

	// Do initial build once the asset registry has finished loading
	if (AssetRegistry.IsLoadingAssets())
	{
		AssetRegistry.OnFilesLoaded().AddLambda([this]()
		{
			RebuildWatcherState();
		});
	}
	else
	{
		RebuildWatcherState();
	}
}

void FYarnSpinnerEditorModule::TeardownSourceFileWatchers()
{
	// Clear debounce timers
	if (GEditor)
	{
		if (ReimportTimerHandle.IsValid())
		{
			GEditor->GetTimerManager()->ClearTimer(ReimportTimerHandle);
		}
		if (RebuildTimerHandle.IsValid())
		{
			GEditor->GetTimerManager()->ClearTimer(RebuildTimerHandle);
		}
	}

	// Unregister directory watchers
	if (FModuleManager::Get().IsModuleLoaded("DirectoryWatcher"))
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
		if (DirectoryWatcher)
		{
			for (auto& Pair : WatchedDirectories)
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
			}
		}
	}
	WatchedDirectories.Empty();
	SourceFileToProjects.Empty();
	ProjectFileToAsset.Empty();
	PendingReimports.Empty();

	// Unregister asset registry callbacks
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		if (AssetAddedHandle.IsValid())
		{
			AssetRegistry.OnAssetAdded().Remove(AssetAddedHandle);
			AssetAddedHandle.Reset();
		}
		if (AssetRemovedHandle.IsValid())
		{
			AssetRegistry.OnAssetRemoved().Remove(AssetRemovedHandle);
			AssetRemovedHandle.Reset();
		}
	}
}

void FYarnSpinnerEditorModule::RebuildWatcherState()
{
	// Unregister existing watchers
	if (FModuleManager::Get().IsModuleLoaded("DirectoryWatcher"))
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
		if (DirectoryWatcher)
		{
			for (auto& Pair : WatchedDirectories)
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
			}
		}
	}
	WatchedDirectories.Empty();
	SourceFileToProjects.Empty();
	ProjectFileToAsset.Empty();

	// Find all UYarnProject assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> YarnProjectAssets;
#if YARNSPINNER_WITH_ASSET_CLASS_PATH
	AssetRegistry.GetAssetsByClass(UYarnProject::StaticClass()->GetClassPathName(), YarnProjectAssets);
#else
	AssetRegistry.GetAssetsByClass(UYarnProject::StaticClass()->GetFName(), YarnProjectAssets);
#endif

	TSet<FString> DirectoriesToWatch;

	for (const FAssetData& AssetData : YarnProjectAssets)
	{
		UYarnProject* Project = Cast<UYarnProject>(AssetData.GetAsset());
		if (!Project)
		{
			continue;
		}

#if WITH_EDITORONLY_DATA
		// Map source .yarn files to this project
		for (const FString& SourceFile : Project->ResolvedSourceFiles)
		{
			FString NormalizedPath = SourceFile;
			FPaths::NormalizeFilename(NormalizedPath);

			SourceFileToProjects.FindOrAdd(NormalizedPath).AddUnique(TWeakObjectPtr<UYarnProject>(Project));

			// Collect the directory for watching
			DirectoriesToWatch.Add(FPaths::GetPath(NormalizedPath));
		}

		// Map the .yarnproject file itself
		if (!Project->SourceProjectPath.IsEmpty())
		{
			FString NormalizedProjectPath = Project->SourceProjectPath;
			FPaths::NormalizeFilename(NormalizedProjectPath);
			ProjectFileToAsset.Add(NormalizedProjectPath, TWeakObjectPtr<UYarnProject>(Project));

			// Also watch the directory containing the .yarnproject
			DirectoriesToWatch.Add(FPaths::GetPath(NormalizedProjectPath));
		}
#endif
	}

	// Register directory watchers
	if (DirectoriesToWatch.Num() > 0)
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
		if (DirectoryWatcher)
		{
			for (const FString& Dir : DirectoriesToWatch)
			{
				FDelegateHandle Handle;
				DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
					Dir,
					IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FYarnSpinnerEditorModule::OnSourceDirectoryChanged),
					Handle
				);

				if (Handle.IsValid())
				{
					WatchedDirectories.Add(Dir, Handle);
				}
			}

			UE_LOG(LogYarnSpinner, Log, TEXT("YarnSpinner: Watching %d directories for .yarn file changes"), WatchedDirectories.Num());
		}
	}
}

void FYarnSpinnerEditorModule::OnSourceDirectoryChanged(const TArray<FFileChangeData>& Changes)
{
	bool bHasRelevantChanges = false;

	for (const FFileChangeData& Change : Changes)
	{
		FString ChangedPath = FPaths::ConvertRelativePathToFull(Change.Filename);
		FPaths::NormalizeFilename(ChangedPath);

		// Check .yarnproject FIRST (since ".yarnproject" also ends with ".yarn")
		if (ChangedPath.EndsWith(TEXT(".yarnproject")))
		{
			if (TWeakObjectPtr<UYarnProject>* WeakProject = ProjectFileToAsset.Find(ChangedPath))
			{
				if (WeakProject->IsValid())
				{
					PendingReimports.Add(*WeakProject);
					bHasRelevantChanges = true;
				}
			}
		}
		// Then check .yarn files
		else if (ChangedPath.EndsWith(TEXT(".yarn")))
		{
			// Look up which projects include this file
			if (TArray<TWeakObjectPtr<UYarnProject>>* Projects = SourceFileToProjects.Find(ChangedPath))
			{
				for (const TWeakObjectPtr<UYarnProject>& WeakProject : *Projects)
				{
					if (WeakProject.IsValid())
					{
						PendingReimports.Add(WeakProject);
						bHasRelevantChanges = true;
					}
				}
			}
			else if (Change.Action == FFileChangeData::FCA_Added)
			{
				// New .yarn file -- it might match a project's sourceFiles glob.
				// We don't know which project(s) without re-resolving globs, so
				// queue all projects for reimport (the compiler will sort it out).
				for (auto& Pair : ProjectFileToAsset)
				{
					if (Pair.Value.IsValid())
					{
						PendingReimports.Add(Pair.Value);
						bHasRelevantChanges = true;
					}
				}
			}
		}
	}

	if (bHasRelevantChanges && GEditor)
	{
		// Reset the reimport debounce timer -- 0.5s after the last change, we'll reimport
		GEditor->GetTimerManager()->SetTimer(
			ReimportTimerHandle,
			FTimerDelegate::CreateRaw(this, &FYarnSpinnerEditorModule::ExecutePendingReimports),
			0.5f,
			false
		);
	}
}

void FYarnSpinnerEditorModule::ExecutePendingReimports()
{
	if (PendingReimports.Num() == 0)
	{
		return;
	}

	UE_LOG(LogYarnSpinner, Log, TEXT("YarnSpinner: Auto-reimporting %d project(s) due to source file changes"), PendingReimports.Num());

	for (const TWeakObjectPtr<UYarnProject>& WeakProject : PendingReimports)
	{
		if (UYarnProject* Project = WeakProject.Get())
		{
			FReimportManager::Instance()->Reimport(Project, /*bAskForNewFileIfMissing=*/false, /*bShowNotification=*/true);
		}
	}

	PendingReimports.Empty();

	// Rebuild watcher state since reimport may have resolved new source files
	RebuildWatcherState();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYarnSpinnerEditorModule, YarnSpinnerEditor)

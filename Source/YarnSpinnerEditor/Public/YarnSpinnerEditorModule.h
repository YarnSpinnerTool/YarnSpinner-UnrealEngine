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
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "IDirectoryWatcher.h"

class FSlateStyleSet;
class UYarnProject;

/**
 * Yarn Spinner editor module.
 *
 * Provides editor functionality for importing and managing Yarn projects,
 * including asset factories, custom editors, and compilation support.
 */
class FYarnSpinnerEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Gets the singleton instance of the editor module.
	 * @return The Yarn Spinner editor module instance.
	 */
	static FYarnSpinnerEditorModule& Get();

	/**
	 * Checks if the module is loaded and ready.
	 * @return True if the module is loaded.
	 */
	static bool IsAvailable();

private:
	/** Registers custom asset types with the engine */
	void RegisterAssetTypes();

	/** Unregisters custom asset types */
	void UnregisterAssetTypes();

	/** Registers the Slate style set for icons */
	void RegisterStyleSet();

	/** Unregisters the Slate style set */
	void UnregisterStyleSet();

	/** Sets up directory watchers for all known .yarn source files */
	void SetupSourceFileWatchers();

	/** Tears down all active directory watchers */
	void TeardownSourceFileWatchers();

	/** Called when a watched directory changes */
	void OnSourceDirectoryChanged(const TArray<FFileChangeData>& Changes);

	/** Rebuilds the watcher state from all loaded UYarnProject assets */
	void RebuildWatcherState();

	/** Executes pending reimports after the debounce timer fires */
	void ExecutePendingReimports();

	/** Registered asset type actions */
	TArray<TSharedPtr<IAssetTypeActions>> RegisteredAssetTypeActions;

	/** Style set for custom icons */
	TSharedPtr<FSlateStyleSet> StyleSet;

	/** Map of watched directory path -> watcher delegate handle */
	TMap<FString, FDelegateHandle> WatchedDirectories;

	/** Map of normalized .yarn file path -> YarnProject assets that include it */
	TMap<FString, TArray<TWeakObjectPtr<UYarnProject>>> SourceFileToProjects;

	/** Map of normalized .yarnproject file path -> YarnProject asset */
	TMap<FString, TWeakObjectPtr<UYarnProject>> ProjectFileToAsset;

	/** Set of projects pending reimport (accumulated during debounce window) */
	TSet<TWeakObjectPtr<UYarnProject>> PendingReimports;

	/** Timer handle for debouncing rapid file change reimports */
	FTimerHandle ReimportTimerHandle;

	/** Timer handle for debouncing watcher rebuilds after asset changes */
	FTimerHandle RebuildTimerHandle;

	/** Asset registry callback handles */
	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRemovedHandle;
};

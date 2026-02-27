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
#include "AssetTypeActions_Base.h"

class UYarnProject;
class UYarnSourceFile;

/**
 * Asset type actions for UYarnProject.
 *
 * Defines how Yarn Project assets appear and behave in the editor,
 * including their category, colour, and available actions.
 */
class YARNSPINNEREDITOR_API FAssetTypeActions_YarnProject : public FAssetTypeActions_Base
{
public:
	// FAssetTypeActions_Base interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;
	// End of FAssetTypeActions_Base interface
};

/**
 * Asset type actions for UYarnSourceFile.
 *
 * Defines how Yarn Source File assets appear and behave in the editor.
 */
class YARNSPINNEREDITOR_API FAssetTypeActions_YarnSourceFile : public FAssetTypeActions_Base
{
public:
	// FAssetTypeActions_Base interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;
	// End of FAssetTypeActions_Base interface
};

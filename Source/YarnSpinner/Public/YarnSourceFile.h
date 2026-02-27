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
#include "UObject/NoExportTypes.h"
#include "YarnSourceFile.generated.h"

/**
 * A Yarn source file asset.
 *
 * This asset stores the raw text content of a .yarn file, allowing you to
 * view and reference yarn source files directly in the Content Browser.
 *
 * To use in dialogue, you'll still need to compile this to a YarnProject
 * using the Yarn Spinner compiler (ysc).
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnSourceFile : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * The raw source text of the yarn file.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner", meta = (MultiLine = true))
	FString SourceText;

	/**
	 * The original filename this was imported from.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FString SourceFilename;

	/**
	 * The node titles found in this file.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	TArray<FString> NodeTitles;

	/**
	 * Get the number of nodes in this source file.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	int32 GetNodeCount() const { return NodeTitles.Num(); }

	/**
	 * Check if a node with the given title exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	bool HasNode(const FString& NodeTitle) const { return NodeTitles.Contains(NodeTitle); }

	/**
	 * Parse the source text and extract node titles.
	 */
	void ParseNodeTitles();

#if WITH_EDITORONLY_DATA
	/** Import data for reimporting */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import Settings")
	class UAssetImportData* AssetImportData;

	virtual void PostInitProperties() override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#endif
};

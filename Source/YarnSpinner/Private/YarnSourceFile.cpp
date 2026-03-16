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

#include "YarnSourceFile.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

void UYarnSourceFile::ParseNodeTitles()
{
	NodeTitles.Empty();

	// Parse "title: NodeName" patterns from the source
	TArray<FString> Lines;
	SourceText.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();

		// Look for "title:" at the start of a line
		if (TrimmedLine.StartsWith(TEXT("title:"), ESearchCase::IgnoreCase))
		{
			FString NodeTitle = TrimmedLine.Mid(6).TrimStartAndEnd();
			if (!NodeTitle.IsEmpty())
			{
				NodeTitles.Add(NodeTitle);
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
void UYarnSourceFile::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
	Super::PostInitProperties();
}

#if YARNSPINNER_WITH_ASSET_REGISTRY_TAGS_CONTEXT
void UYarnSourceFile::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	if (AssetImportData)
	{
		Context.AddTag(FAssetRegistryTag(TEXT("SourceFile"), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(Context);
}
#else
void UYarnSourceFile::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(TEXT("SourceFile"), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}

	Super::GetAssetRegistryTags(OutTags);
}
#endif
#endif

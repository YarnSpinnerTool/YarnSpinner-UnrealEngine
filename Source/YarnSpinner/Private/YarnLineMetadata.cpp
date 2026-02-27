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

#include "YarnLineMetadata.h"

void UYarnLineMetadata::ParseMetadataString(const FString& MetadataString, TArray<FString>& OutTags)
{
	OutTags.Empty();
	MetadataString.ParseIntoArray(OutTags, TEXT(" "), true);
}

bool UYarnLineMetadata::GetMetadata(const FString& LineID, TArray<FString>& OutMetadata) const
{
	if (const FString* Found = LineMetadataMap.Find(LineID))
	{
		ParseMetadataString(*Found, OutMetadata);
		return OutMetadata.Num() > 0;
	}

	OutMetadata.Empty();
	return false;
}

bool UYarnLineMetadata::HasMetadataTag(const FString& LineID, const FString& Tag) const
{
	TArray<FString> Tags;
	if (GetMetadata(LineID, Tags))
	{
		for (const FString& T : Tags)
		{
			if (T.Equals(Tag, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}
	return false;
}

bool UYarnLineMetadata::GetShadowLineSource(const FString& LineID, FString& OutSourceLineID) const
{
	TArray<FString> Tags;
	if (GetMetadata(LineID, Tags))
	{
		for (const FString& Tag : Tags)
		{
			if (Tag.StartsWith(TEXT("shadow:"), ESearchCase::IgnoreCase))
			{
				OutSourceLineID = Tag.Mid(7); // length of "shadow:"
				return true;
			}
		}
	}

	OutSourceLineID = TEXT("");
	return false;
}

bool UYarnLineMetadata::IsShadowLine(const FString& LineID) const
{
	FString Unused;
	return GetShadowLineSource(LineID, Unused);
}

void UYarnLineMetadata::GetLineIDsWithMetadata(TArray<FString>& OutLineIDs) const
{
	OutLineIDs.Empty();
	for (const TPair<FString, FString>& Pair : LineMetadataMap)
	{
		OutLineIDs.Add(Pair.Key);
	}
}

void UYarnLineMetadata::GetLineIDsWithTag(const FString& Tag, TArray<FString>& OutLineIDs) const
{
	OutLineIDs.Empty();
	for (const TPair<FString, FString>& Pair : LineMetadataMap)
	{
		if (HasMetadataTag(Pair.Key, Tag))
		{
			OutLineIDs.Add(Pair.Key);
		}
	}
}

void UYarnLineMetadata::AddLineMetadata(const FString& LineID, const FString& Metadata)
{
	if (FString* Existing = LineMetadataMap.Find(LineID))
	{
		*Existing = *Existing + TEXT(" ") + Metadata;
	}
	else
	{
		LineMetadataMap.Add(LineID, Metadata);
	}
}

void UYarnLineMetadata::AddMetadataTag(const FString& LineID, const FString& Tag)
{
	AddLineMetadata(LineID, Tag);
}

void UYarnLineMetadata::Clear()
{
	LineMetadataMap.Empty();
}

// library functions
bool UYarnLineMetadataLibrary::IsLastLineBeforeOptions(const TArray<FString>& Metadata)
{
	return HasTag(Metadata, TEXT("lastline"));
}

bool UYarnLineMetadataLibrary::GetMetadataValue(const TArray<FString>& Metadata, const FString& Key, FString& OutValue)
{
	FString Prefix = Key + TEXT(":");
	for (const FString& Tag : Metadata)
	{
		if (Tag.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			OutValue = Tag.Mid(Prefix.Len());
			return true;
		}
	}

	OutValue = TEXT("");
	return false;
}

bool UYarnLineMetadataLibrary::HasTag(const TArray<FString>& Metadata, const FString& Tag)
{
	for (const FString& T : Metadata)
	{
		if (T.Equals(Tag, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

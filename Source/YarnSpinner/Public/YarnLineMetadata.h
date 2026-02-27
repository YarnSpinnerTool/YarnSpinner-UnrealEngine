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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YarnLineMetadata.generated.h"

/**
 * Stores metadata for lines in a Yarn project.
 *
 * Line metadata includes hashtags and other information associated with
 * individual lines of dialogue. This data comes from the compiled Yarn
 * project and can be used for things like:
 * - Associating audio files with lines
 * - Filtering lines by category
 * - Tracking shadow lines for localisation variants
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnLineMetadata : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get all metadata tags for a line.
	 *
	 * @param LineID The line ID to get metadata for.
	 * @param OutMetadata Array to fill with metadata tags.
	 * @return True if the line has metadata.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	bool GetMetadata(const FString& LineID, TArray<FString>& OutMetadata) const;

	/**
	 * Check if a line has a specific metadata tag.
	 *
	 * @param LineID The line ID to check.
	 * @param Tag The tag to look for.
	 * @return True if the line has this tag.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	bool HasMetadataTag(const FString& LineID, const FString& Tag) const;

	/**
	 * Get the shadow line source for a line.
	 *
	 * Shadow lines are used for localisation variants that reference
	 * another line's text. Returns the source line ID if this line
	 * is a shadow of another line.
	 *
	 * @param LineID The line ID to check.
	 * @param OutSourceLineID The source line ID if this is a shadow line.
	 * @return True if this is a shadow line.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	bool GetShadowLineSource(const FString& LineID, FString& OutSourceLineID) const;

	/**
	 * Check if a line is a shadow of another line.
	 *
	 * @param LineID The line ID to check.
	 * @return True if this is a shadow line.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	bool IsShadowLine(const FString& LineID) const;

	/**
	 * Get all line IDs that have metadata.
	 *
	 * @param OutLineIDs Array to fill with line IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	void GetLineIDsWithMetadata(TArray<FString>& OutLineIDs) const;

	/**
	 * Get all line IDs that have a specific metadata tag.
	 *
	 * @param Tag The tag to filter by.
	 * @param OutLineIDs Array to fill with matching line IDs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	void GetLineIDsWithTag(const FString& Tag, TArray<FString>& OutLineIDs) const;

	/**
	 * Add metadata for a line.
	 *
	 * @param LineID The line ID to add metadata for.
	 * @param Metadata Space-separated metadata tags.
	 */
	void AddLineMetadata(const FString& LineID, const FString& Metadata);

	/**
	 * Add a single metadata tag for a line.
	 *
	 * @param LineID The line ID to add the tag to.
	 * @param Tag The tag to add.
	 */
	void AddMetadataTag(const FString& LineID, const FString& Tag);

	/**
	 * Clear all metadata.
	 */
	void Clear();

	/**
	 * Get the number of lines with metadata.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	int32 GetLineCount() const { return LineMetadataMap.Num(); }

private:
	/** Internal storage: LineID -> space-separated metadata string */
	UPROPERTY()
	TMap<FString, FString> LineMetadataMap;

	/** Parse metadata string into array of tags */
	static void ParseMetadataString(const FString& MetadataString, TArray<FString>& OutTags);
};

/**
 * Information about a single line from the compiled Yarn project.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnLineInfo
{
	GENERATED_BODY()

	/** The unique identifier for this line */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Metadata")
	FString LineID;

	/** The original text of the line */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Metadata")
	FString Text;

	/** The name of the node containing this line */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Metadata")
	FString NodeName;

	/** The line number in the source file */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Metadata")
	int32 LineNumber = 0;

	/** The name of the source file */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Metadata")
	FString FileName;

	/** Whether the line ID was auto-generated */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Metadata")
	bool bIsImplicitTag = false;

	/** Metadata tags (hashtags from the line) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Metadata")
	TArray<FString> Metadata;

	/** The shadow line ID if this line shadows another (empty if not) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Metadata")
	FString ShadowLineID;

	/** Default constructor */
	FYarnLineInfo() = default;
};

/**
 * Static library for working with line metadata.
 */
UCLASS()
class YARNSPINNER_API UYarnLineMetadataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Check if a metadata tag indicates this is the last line before options.
	 *
	 * Lines immediately before dialogue options are automatically tagged
	 * with "lastline" by the compiler.
	 *
	 * @param Metadata The metadata array to check.
	 * @return True if this is the last line before options.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	static bool IsLastLineBeforeOptions(const TArray<FString>& Metadata);

	/**
	 * Get a metadata value by key.
	 *
	 * Searches for metadata in the format "key:value" and returns the value.
	 *
	 * @param Metadata The metadata array to search.
	 * @param Key The key to look for.
	 * @param OutValue The value if found.
	 * @return True if the key was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	static bool GetMetadataValue(const TArray<FString>& Metadata, const FString& Key, FString& OutValue);

	/**
	 * Check if metadata contains a specific tag.
	 *
	 * @param Metadata The metadata array to search.
	 * @param Tag The tag to look for.
	 * @return True if the tag is present.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Metadata")
	static bool HasTag(const TArray<FString>& Metadata, const FString& Tag);
};

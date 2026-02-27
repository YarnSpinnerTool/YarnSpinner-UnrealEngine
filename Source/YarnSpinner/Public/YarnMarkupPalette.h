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
#include "Engine/DataAsset.h"
#include "YarnMarkupPalette.generated.h"

/**
 * Defines a basic marker in the palette.
 *
 * Basic markers are simple tags that mark a position or range in the text.
 * They don't have any properties - just a name.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnBasicMarker
{
	GENERATED_BODY()

	/** The name of the marker (e.g., "wave", "shake") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	FString MarkerName;

	/** Description for documentation/editor purposes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	FString Description;
};

/**
 * Defines a property on a custom marker.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnMarkerProperty
{
	GENERATED_BODY()

	/** The name of the property */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	FString PropertyName;

	/** The default value for this property (as string) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	FString DefaultValue;

	/** Whether this property is required */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	bool bRequired = false;

	/** Description for documentation/editor purposes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	FString Description;
};

/**
 * Defines a custom marker with properties.
 *
 * Custom markers can have named properties with default values,
 * e.g., [shake intensity=2.0] or [color=#FF0000]
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnCustomMarker
{
	GENERATED_BODY()

	/** The name of the marker (e.g., "color", "size") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	FString MarkerName;

	/** Properties this marker accepts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	TArray<FYarnMarkerProperty> Properties;

	/** Whether this is a self-closing marker (e.g., [pause/] vs [color]...[/color]) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	bool bSelfClosing = false;

	/** Description for documentation/editor purposes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	FString Description;
};

/**
 * A markup palette defines the set of markers available for a project.
 *
 * Use this data asset to:
 * - Define which markers are valid in your project
 * - Provide default values for marker properties
 * - Enable editor validation and auto-complete
 *
 * Example markers:
 * - Basic: [wave], [shake], [bounce]
 * - Custom: [pause duration=1.0/], [color=#FF0000]text[/color]
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnMarkupPalette : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Basic markers (no properties) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	TArray<FYarnBasicMarker> BasicMarkers;

	/** Custom markers (with properties) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup")
	TArray<FYarnCustomMarker> CustomMarkers;

	/**
	 * Check if a marker name is defined in this palette.
	 * @param MarkerName The name to check.
	 * @return True if the marker is defined.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup")
	bool ContainsMarker(const FString& MarkerName) const;

	/**
	 * Get a basic marker by name.
	 * @param MarkerName The marker name.
	 * @param OutMarker The found marker.
	 * @return True if found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup")
	bool TryGetBasicMarker(const FString& MarkerName, FYarnBasicMarker& OutMarker) const;

	/**
	 * Get a custom marker by name.
	 * @param MarkerName The marker name.
	 * @param OutMarker The found marker.
	 * @return True if found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup")
	bool TryGetCustomMarker(const FString& MarkerName, FYarnCustomMarker& OutMarker) const;

	/**
	 * Get all marker names in this palette.
	 * @return Array of all marker names.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup")
	TArray<FString> GetAllMarkerNames() const;

	/**
	 * Get the default value for a property on a custom marker.
	 * @param MarkerName The marker name.
	 * @param PropertyName The property name.
	 * @param OutDefaultValue The default value.
	 * @return True if the property was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup")
	bool TryGetPropertyDefault(const FString& MarkerName, const FString& PropertyName, FString& OutDefaultValue) const;
};

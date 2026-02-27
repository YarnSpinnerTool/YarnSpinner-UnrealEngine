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
#include "UObject/Interface.h"
#include "Engine/DataAsset.h"
#include "YarnMarkup.generated.h"

/**
 * The type of a markup property value.
 */
UENUM(BlueprintType)
enum class EYarnMarkupValueType : uint8
{
	Integer,
	Float,
	String,
	Bool,
};

/**
 * A typed value for a markup property.
 * Properties can be integer, float, string, or bool.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnMarkupValue
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Markup")
	EYarnMarkupValueType Type = EYarnMarkupValueType::String;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Markup")
	int32 IntegerValue = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Markup")
	float FloatValue = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Markup")
	FString StringValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Markup")
	bool BoolValue = false;

	FYarnMarkupValue() = default;

	static FYarnMarkupValue MakeInteger(int32 InValue)
	{
		FYarnMarkupValue V;
		V.Type = EYarnMarkupValueType::Integer;
		V.IntegerValue = InValue;
		return V;
	}

	static FYarnMarkupValue MakeFloat(float InValue)
	{
		FYarnMarkupValue V;
		V.Type = EYarnMarkupValueType::Float;
		V.FloatValue = InValue;
		return V;
	}

	static FYarnMarkupValue MakeString(const FString& InValue)
	{
		FYarnMarkupValue V;
		V.Type = EYarnMarkupValueType::String;
		V.StringValue = InValue;
		return V;
	}

	static FYarnMarkupValue MakeBool(bool InValue)
	{
		FYarnMarkupValue V;
		V.Type = EYarnMarkupValueType::Bool;
		V.BoolValue = InValue;
		return V;
	}

	/** Convert to string representation */
	FString ToString() const
	{
		switch (Type)
		{
		case EYarnMarkupValueType::Integer:
			return FString::FromInt(IntegerValue);
		case EYarnMarkupValueType::Float:
			return FString::SanitizeFloat(FloatValue);
		case EYarnMarkupValueType::String:
			return StringValue;
		case EYarnMarkupValueType::Bool:
			return BoolValue ? TEXT("true") : TEXT("false");
		default:
			return FString();
		}
	}
};

/**
 * Represents a single markup attribute in parsed text.
 * For example, in "[bold]Hello[/bold]", the attribute would have:
 * - Name: "bold"
 * - Position: 0
 * - Length: 5 (the length of "Hello")
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnMarkupAttribute
{
	GENERATED_BODY()

	/** The name of the attribute (e.g., "bold", "color", "style") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	FString Name;

	/** The character position where this attribute starts in the plain text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	int32 Position = 0;

	/** The length in characters that this attribute covers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	int32 Length = 0;

	/** Position in the original source text (before markup stripping) where this
	 *  attribute's opening tag begins. -1 if not available. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Markup")
	int32 SourcePosition = -1;

	/** Additional typed properties for this attribute (e.g., color="red", count=5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	TMap<FString, FYarnMarkupValue> Properties;

	/** Get a property value as string by key (backward-compatible convenience method) */
	FString GetProperty(const FString& Key, const FString& DefaultValue = FString()) const
	{
		if (const FYarnMarkupValue* Found = Properties.Find(Key))
		{
			return Found->ToString();
		}
		return DefaultValue;
	}

	/** Check if a property exists */
	bool HasProperty(const FString& Key) const
	{
		return Properties.Contains(Key);
	}

	/** Get typed property value */
	bool TryGetProperty(const FString& Key, FYarnMarkupValue& OutValue) const
	{
		if (const FYarnMarkupValue* Found = Properties.Find(Key))
		{
			OutValue = *Found;
			return true;
		}
		return false;
	}
};

/**
 * Result of parsing markup in text.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnMarkupParseResult
{
	GENERATED_BODY()

	/** The text with all markup tags removed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	FString Text;

	/** All markup attributes found in the text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	TArray<FYarnMarkupAttribute> Attributes;

	/** The character name extracted from the line (if any).
	 * Populated from [character name="..."] attribute or implicit "Name: " prefix. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	FString CharacterName;

	/** The text without the character name prefix. If no character name, same as Text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	FString TextWithoutCharacterName;

	/** Check if parsing was successful */
	bool IsValid() const { return !Text.IsEmpty() || Attributes.Num() > 0; }

	/** Find all attributes with a given name */
	TArray<FYarnMarkupAttribute> FindAttributesByName(const FString& Name) const
	{
		TArray<FYarnMarkupAttribute> Result;
		for (const FYarnMarkupAttribute& Attr : Attributes)
		{
			if (Attr.Name == Name)
			{
				Result.Add(Attr);
			}
		}
		return Result;
	}

	/** Find an attribute by name. Returns nullptr if not found. */
	const FYarnMarkupAttribute* FindAttribute(const FString& Name) const
	{
		for (const FYarnMarkupAttribute& Attr : Attributes)
		{
			if (Attr.Name.Equals(Name, ESearchCase::CaseSensitive))
			{
				return &Attr;
			}
		}
		return nullptr;
	}
};

/**
 * Result from processing a markup replacement.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnMarkupReplacementResult
{
	GENERATED_BODY()

	/** Number of invisible characters added (e.g., rich text tags) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	int32 InvisibleCharactersAdded = 0;

	/** Any diagnostic messages or errors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	TArray<FString> Diagnostics;

	/** Whether processing was successful */
	bool IsSuccess() const { return Diagnostics.Num() == 0; }
};

/**
 * Interface for custom markup processors.
 *
 * Implement this interface to handle custom markup tags in dialogue text.
 * For example, you could create a processor that converts [shake] tags
 * to text effects in your game.
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class UYarnMarkupProcessor : public UInterface
{
	GENERATED_BODY()
};

class YARNSPINNER_API IYarnMarkupProcessor
{
	GENERATED_BODY()

public:
	/**
	 * Process a markup attribute and modify the text/attributes as needed.
	 *
	 * @param Attribute The markup attribute to process.
	 * @param Text The text content within the tags (can be modified).
	 * @param ChildAttributes Any nested attributes that may need position adjustment.
	 * @param LocaleCode The current locale code (e.g., "en", "fr").
	 * @return Result containing diagnostics and invisible character count.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Markup")
	FYarnMarkupReplacementResult ProcessMarkup(
		const FYarnMarkupAttribute& Attribute,
		FString& Text,
		TArray<FYarnMarkupAttribute>& ChildAttributes,
		const FString& LocaleCode
	);
};

/**
 * A single marker style definition in a markup palette.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnMarkupStyle
{
	GENERATED_BODY()

	/** The marker name (e.g., "emphasis", "warning") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FString MarkerName;

	/** Whether to apply a custom color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bUseColor = false;

	/** The color to apply */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (EditCondition = "bUseColor"))
	FLinearColor Color = FLinearColor::White;

	/** Whether to make the text bold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bBold = false;

	/** Whether to make the text italic */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bItalic = false;

	/** Whether to underline the text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bUnderline = false;

	/** Whether to strikethrough the text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	bool bStrikethrough = false;

	/**
	 * Generate rich text tags for this style.
	 * @param OutStartTag The opening tag(s).
	 * @param OutEndTag The closing tag(s).
	 */
	void GenerateRichTextTags(FString& OutStartTag, FString& OutEndTag) const;
};

/**
 * A rich text marker with arbitrary start/end tags for rendering.
 * Different from FYarnCustomMarker in YarnMarkupPalette.h which is for validation.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnRichTextMarker
{
	GENERATED_BODY()

	/** The marker name to match */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	FString MarkerName;

	/** The opening tag to insert (e.g., "<shake>" or "<color=#FF0000>") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	FString StartTag;

	/** The closing tag to insert (e.g., "</shake>" or "</color>") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	FString EndTag;

	/** Number of visible characters added at the start (for position adjustment) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	int32 VisibleCharactersAtStart = 0;
};

/**
 * A data asset containing rich text style definitions for markup rendering.
 *
 * Create instances of this in the Content Browser and assign to
 * UYarnPaletteMarkupProcessor to define how markup tags are rendered.
 *
 * This is different from UYarnMarkupPalette in YarnMarkupPalette.h,
 * which is for marker validation. This class is for rendering.
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnRichTextPalette : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Basic style markers with standard formatting options */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	TArray<FYarnMarkupStyle> BasicStyles;

	/** Custom markers with arbitrary tag injection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	TArray<FYarnRichTextMarker> CustomMarkers;

	/**
	 * Look up a basic style by marker name.
	 * @param MarkerName The name to look for.
	 * @return Pointer to the style, or nullptr if not found.
	 */
	const FYarnMarkupStyle* FindBasicStyle(const FString& MarkerName) const;

	/**
	 * Look up a custom marker by name.
	 * @param MarkerName The name to look for.
	 * @return Pointer to the marker, or nullptr if not found.
	 */
	const FYarnRichTextMarker* FindCustomMarker(const FString& MarkerName) const;
};

/**
 * A component that processes markup using a palette asset.
 *
 * Attach this to an actor and assign a MarkupPalette to define
 * how markup tags are converted to rich text.
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable)
class YARNSPINNER_API UYarnPaletteMarkupProcessor : public UActorComponent, public IYarnMarkupProcessor
{
	GENERATED_BODY()

public:
	UYarnPaletteMarkupProcessor();

	/** The rich text palette to use for style lookups */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markup")
	UYarnRichTextPalette* Palette;

	// IYarnMarkupProcessor interface
	virtual FYarnMarkupReplacementResult ProcessMarkup_Implementation(
		const FYarnMarkupAttribute& Attribute,
		FString& Text,
		TArray<FYarnMarkupAttribute>& ChildAttributes,
		const FString& LocaleCode
	) override;
};

/**
 * A component that processes [style=name] tags using rich text styles.
 *
 * This processor handles tags like [style=h1] and wraps content in
 * rich text style tags.
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable)
class YARNSPINNER_API UYarnStyleMarkupProcessor : public UActorComponent, public IYarnMarkupProcessor
{
	GENERATED_BODY()

public:
	UYarnStyleMarkupProcessor();

	// IYarnMarkupProcessor interface
	virtual FYarnMarkupReplacementResult ProcessMarkup_Implementation(
		const FYarnMarkupAttribute& Attribute,
		FString& Text,
		TArray<FYarnMarkupAttribute>& ChildAttributes,
		const FString& LocaleCode
	) override;
};

/**
 * Library of utility functions for working with Yarn markup.
 */
UCLASS()
class YARNSPINNER_API UYarnMarkupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Parse markup tags from text (simple version).
	 *
	 * @param Text The text containing markup tags.
	 * @return Parse result with plain text and attributes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup")
	static FYarnMarkupParseResult ParseMarkup(const FString& Text);

	/**
	 * Parse markup tags from text with full options.
	 * @param Text The text containing markup tags.
	 * @param LocaleCode The locale code for plural/ordinal rules (e.g., "en").
	 * @param bAddImplicitCharacterAttribute If true, detects "Name: text" pattern and creates a character attribute.
	 * @return Parse result with plain text, attributes, and character name.
	 */
	static FYarnMarkupParseResult ParseMarkupFull(const FString& Text, const FString& LocaleCode = TEXT("en"), bool bAddImplicitCharacterAttribute = true);

	/**
	 * Apply markup processors to text.
	 *
	 * @param ParseResult The parsed markup to process.
	 * @param Processors Array of markup processors to apply.
	 * @param LocaleCode The current locale code.
	 * @return Processed text with rich text tags.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup")
	static FString ApplyMarkupProcessors(
		const FYarnMarkupParseResult& ParseResult,
		const TArray<TScriptInterface<IYarnMarkupProcessor>>& Processors,
		const FString& LocaleCode
	);

	/**
	 * Convert a color to a rich text hex string.
	 * @param Color The color to convert.
	 * @return Hex string like "#FF0000FF".
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static FString ColorToHexString(const FLinearColor& Color);

	/**
	 * Escape special characters in text for use in rich text.
	 * @param Text The text to escape.
	 * @return Escaped text.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static FString EscapeRichText(const FString& Text);
};

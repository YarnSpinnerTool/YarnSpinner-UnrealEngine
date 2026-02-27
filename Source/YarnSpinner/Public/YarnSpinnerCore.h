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

// ============================================================================
// YarnSpinnerCore.h
// ============================================================================
//
// WHAT THIS IS:
// This file defines the fundamental data types used throughout Yarn Spinner.
// Everything that flows through the system - values, lines, options, commands -
// is defined here.
//
// BLUEPRINT VS C++ USAGE:
// All types here are BlueprintType, so they can be used in blueprints:
//   - as variables
//   - as function parameters and return values
//   - with break/make nodes
//
// C++ users can use these directly or access their internals.

// ----------------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------------

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "YarnMarkup.h"
#include "YarnSpinnerCore.generated.h"

struct FYarnMarkupParseResult;


// ============================================================================
// EYarnValueType
// ============================================================================

/**
 * The type of a Yarn value.
 *
 * Yarn has a simple type system with just three types: string, number, and bool.
 * This matches the Yarn language specification.
 */
UENUM(BlueprintType)
enum class EYarnValueType : uint8
{
	/** No value (uninitialised). Use IsValid() to check for this. */
	None UMETA(DisplayName = "None"),

	/** A string value. Access with GetStringValue(). */
	String UMETA(DisplayName = "String"),

	/** A numeric value (float). Access with GetNumberValue(). */
	Number UMETA(DisplayName = "Number"),

	/** A boolean value. Access with GetBoolValue(). */
	Bool UMETA(DisplayName = "Bool")
};

// ============================================================================
// FYarnValue
// ============================================================================
//
// This is the universal value type used throughout Yarn. All variables, function
// parameters, and expression results are FYarnValue instances.
//
// In blueprints, you can use the break node to access the underlying value,
// or use the conversion methods (ConvertToString, etc).

/**
 * A value in the Yarn Spinner system.
 *
 * Yarn values can be strings, numbers (floats), or booleans.
 * This matches Yarn's simple type system where all numbers are floats and
 * there are no null values.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnValue
{
	GENERATED_BODY()

	/** The type of this value. Use the Is* methods to check type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	EYarnValueType Type = EYarnValueType::None;

	// ------------------------------------------------------------------------
	// Constructors
	// ------------------------------------------------------------------------

	/** Default constructor - creates an uninitialised value (Type == None) */
	FYarnValue();

	/** Construct from a string */
	FYarnValue(const FString& InString);

	/** Construct from a number */
	FYarnValue(float InNumber);

	/** Construct from a boolean */
	FYarnValue(bool bInBool);

	// ------------------------------------------------------------------------
	// Type checking
	// ------------------------------------------------------------------------

	bool IsString() const { return Type == EYarnValueType::String; }
	bool IsNumber() const { return Type == EYarnValueType::Number; }
	bool IsBool() const { return Type == EYarnValueType::Bool; }
	bool IsValid() const { return Type != EYarnValueType::None; }

	// ------------------------------------------------------------------------
	// Type-safe accessors
	// ------------------------------------------------------------------------
	// These return the value if the type matches, or a default otherwise.
	// They don't convert between types - use the Convert methods for that.

	/**
	 * Get the string value.
	 * @return The string value, or empty string if not a string type.
	 */
	const FString& GetStringValue() const;

	/**
	 * Get the number value.
	 * @return The number value, or 0 if not a number type.
	 */
	float GetNumberValue() const;

	/**
	 * Get the boolean value.
	 * @return The boolean value, or false if not a bool type.
	 */
	bool GetBoolValue() const;

	// ------------------------------------------------------------------------
	// Type conversion
	// ------------------------------------------------------------------------
	// These convert to the target type regardless of the current type.

	/**
	 * Convert this value to a string representation.
	 * Numbers become "1.23", bools become "true"/"false".
	 * @return A string representation of this value.
	 */
	FString ConvertToString() const;

	/**
	 * Convert this value to a number.
	 * Strings are parsed as numbers, bools become 0 or 1.
	 * @return A numeric representation of this value.
	 */
	float ConvertToNumber() const;

	/**
	 * Convert this value to a boolean.
	 * Strings: "true"/"false", numbers: 0 is false, non-zero is true.
	 * @return A boolean representation of this value.
	 */
	bool ConvertToBool() const;

	// ------------------------------------------------------------------------
	// Comparison
	// ------------------------------------------------------------------------

	/** Equality comparison (checks type and value) */
	bool operator==(const FYarnValue& Other) const;
	bool operator!=(const FYarnValue& Other) const { return !(*this == Other); }

private:
	/** The string value (only valid if Type == String) */
	UPROPERTY()
	FString StringValue;

	/** The number value (only valid if Type == Number) */
	UPROPERTY()
	float NumberValue = 0.0f;

	/** The boolean value (only valid if Type == Bool) */
	UPROPERTY()
	bool BoolValue = false;
};

// ============================================================================
// FYarnLine
// ============================================================================
//
// This is the raw line data that comes from the VM. It contains just the line ID
// (used for localisation lookup) and any substitution values (for {0}, {1} etc).
//
// This is NOT what presenters receive - the dialogue runner converts this to
// FYarnLocalizedLine by looking up the text through the line provider.

/**
 * A raw line of dialogue before localisation.
 *
 * Contains the line ID (for text lookup) and substitution values. The dialogue
 * runner converts this to FYarnLocalizedLine before sending to presenters.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnLine
{
	GENERATED_BODY()

	/**
	 * The unique identifier for this line.
	 * Format is typically "line:nodename-linenumber" (e.g., "line:Start-0").
	 * Used by the line provider to look up the actual text.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FString LineID;

	/**
	 * Values to substitute into the line text.
	 * These replace {0}, {1}, etc. in the string table text.
	 * For example, if the line is "Hello, {0}!" and Substitutions[0] is "Player",
	 * the result is "Hello, Player!".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	TArray<FString> Substitutions;

	FYarnLine() = default;

	/** Construct with just a line ID (no substitutions) */
	FYarnLine(const FString& InLineID)
		: LineID(InLineID)
	{
	}

	/** Construct with a line ID and substitutions */
	FYarnLine(const FString& InLineID, const TArray<FString>& InSubstitutions)
		: LineID(InLineID)
		, Substitutions(InSubstitutions)
	{
	}
};

// ============================================================================
// FYarnLocalizedLine
// ============================================================================
//
// This is what presenters receive. The dialogue runner takes the raw FYarnLine,
// looks up the text via the line provider, applies substitutions, extracts the
// character name, and packages it all up here.

/**
 * A localised line ready for presentation.
 *
 * This is what presenters receive when RunLine is called. It contains the
 * final text with all substitutions applied, the character name extracted
 * (if the line uses "Character: " format), and any metadata tags.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnLocalizedLine
{
	GENERATED_BODY()

	/**
	 * The raw line data (contains LineID for audio lookup, etc).
	 * You typically don't need this in presenters - use the other fields.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FYarnLine RawLine;

	/**
	 * The localised text with substitutions applied.
	 * This is the full text including character name prefix if present.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FText Text;

	/**
	 * The text without the character name prefix.
	 * Use this when you're displaying the character name separately.
	 * For example, if the Yarn line is "Alice: Hello!" this would be "Hello!".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FText TextWithoutCharacterName;

	/**
	 * The name of the character speaking this line.
	 * Extracted from "Character: " prefix in the Yarn script.
	 * Empty if the line doesn't have a character prefix.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FString CharacterName;

	/**
	 * Metadata/hashtags associated with this line.
	 * These come from #tags at the end of lines in Yarn scripts.
	 * For example, "Hello! #angry #shout" would have ["angry", "shout"].
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	TArray<FString> Metadata;

	/**
	 * The parsed markup for this line.
	 * Contains attributes with positions and properties for text effects.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FYarnMarkupParseResult TextMarkup;

	FYarnLocalizedLine() = default;

	/** Check if this is a valid line (has a line ID) */
	bool IsValid() const { return !RawLine.LineID.IsEmpty(); }

	/** Check if this line has a specific metadata tag */
	bool HasMetadata(const FString& Tag) const
	{
		return Metadata.Contains(Tag);
	}

	/** Static invalid line for error cases */
	static FYarnLocalizedLine InvalidLine();
};

// ============================================================================
// FYarnOption
// ============================================================================
//
// Represents a single choice the player can make. Options come from shortcut
// options (-> Option text) or jump commands with conditions.
//
// The bIsAvailable flag is important - Yarn lets you show unavailable options
// (greyed out) so players know what they COULD have said if conditions were met.

/**
 * A single dialogue option that can be selected by the player.
 *
 * In Yarn scripts, options come from shortcut options (-> Option text) or
 * from jump commands. Each option has text to display and may be conditionally
 * disabled.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnOption
{
	GENERATED_BODY()

	/**
	 * The unique ID for this option within the current option set.
	 * Use this when calling OnOptionSelected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	int32 OptionID = INDEX_NONE;

	/**
	 * The localised line to display for this option.
	 * Contains the text, character name (if any), and metadata.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FYarnLocalizedLine Line;

	/**
	 * Whether this option is available for selection.
	 *
	 * If false, the option's condition evaluated to false. You might want to
	 * display it greyed out so players know it exists but can't be chosen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	bool bIsAvailable = true;

	FYarnOption() = default;

	/** Check if this is a valid option (has a valid ID) */
	bool IsValid() const { return OptionID != INDEX_NONE; }
};

// ============================================================================
// FYarnOptionSet
// ============================================================================
//
// Contains all options presented to the player at a choice point. Presenters
// receive this in RunOptions and should display all options, then call
// OnOptionSelected when the player makes a choice.

/**
 * A set of options to present to the player at a choice point.
 *
 * Presenters receive this in RunOptions and should display all options.
 * When the player selects one, call OnOptionSelected(index).
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnOptionSet
{
	GENERATED_BODY()

	/**
	 * All options in this set.
	 * Some may have bIsAvailable = false if their conditions weren't met.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	TArray<FYarnOption> Options;

	/** Get an option by its ID (returns nullptr if not found) */
	const FYarnOption* GetOptionByID(int32 OptionID) const;

	/** Check if at least one option is available for selection */
	bool HasAvailableOptions() const;
};

// ============================================================================
// FYarnCommand
// ============================================================================
//
// Commands are actions that Yarn tells your game to perform. They come from
// <<command arg1 arg2>> syntax in Yarn scripts.
//
// Yarn Spinner includes some built-in commands (like <<wait>>), but you can
// register your own via AddCommandHandler on the dialogue runner.

/**
 * A command to be executed by the game.
 *
 * Commands come from <<command arg1 arg2>> syntax in Yarn scripts.
 * The dialogue runner handles built-in commands (like <<wait>>) and fires
 * OnUnhandledCommand for custom ones.
 *
 * C++ users can register handlers with DialogueRunner->AddCommandHandler().
 * Blueprint users can bind to the OnUnhandledCommand event and parse there.
 *
 * Commands are parsed into name + parameters for easier handling.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnCommand
{
	GENERATED_BODY()

	/**
	 * The full text of the command as written in the Yarn script.
	 * For example, "wait 2.5" or "playSound explosion loud".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FString CommandText;

	/**
	 * The command name (first word).
	 * For "playSound explosion loud", this would be "playSound".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	FString CommandName;

	/**
	 * The command parameters (remaining words after the name).
	 * For "playSound explosion loud", this would be ["explosion", "loud"].
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	TArray<FString> Parameters;

	FYarnCommand() = default;

	/** Construct from command text, automatically parsing name and parameters */
	explicit FYarnCommand(const FString& InCommandText);

	/** Check if this is a valid command (has a name) */
	bool IsValid() const { return !CommandName.IsEmpty(); }

	/**
	 * Parse command text into name and parameters.
	 * @param InCommandText The full command text.
	 * @param OutName The command name (first word).
	 * @param OutParameters The command parameters (remaining words).
	 */
	static void ParseCommandText(const FString& InCommandText, FString& OutName, TArray<FString>& OutParameters);
};

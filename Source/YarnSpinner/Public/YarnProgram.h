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
// YarnProgram.h
// ============================================================================
//
// WHAT THIS IS:
// This file defines the compiled Yarn program structure. A UYarnProject asset
// contains one of these, which holds all the bytecode and data needed to run
// the dialogue.
//
// BLUEPRINT VS C++ USAGE:
// Most of this is internal - blueprint users interact with UYarnProject.
// C++ users might access FYarnProgram directly for advanced introspection.
//
// STRUCTURE:
// UYarnProject (the asset you create in editor)
//   +-- FYarnProgram (the compiled bytecode)
//        +-- TMap<FString, FYarnNode> Nodes
//             +-- TArray<FYarnInstruction> Instructions

#include "CoreMinimal.h"
#include "YarnSpinnerCore.h"
#include "YarnProgram.generated.h"

// ============================================================================
// EYarnInstructionType
// ============================================================================

/**
 * The type of a Yarn VM instruction.
 * These map directly to the protobuf instruction types from the compiler.
 * You shouldn't need to interact with these unless you're debugging the VM.
 */
UENUM()
enum class EYarnInstructionType : uint8
{
	Invalid,
	JumpTo,
	PeekAndJump,
	RunLine,
	RunCommand,
	AddOption,
	ShowOptions,
	PushString,
	PushFloat,
	PushBool,
	JumpIfFalse,
	Pop,
	CallFunction,
	PushVariable,
	StoreVariable,
	Stop,
	RunNode,
	PeekAndRunNode,
	DetourToNode,
	PeekAndDetourToNode,
	Return,
	AddSaliencyCandidate,
	AddSaliencyCandidateFromNode,
	SelectSaliencyCandidate
};

/**
 * A single Yarn Spinner instruction.
 */
USTRUCT()
struct YARNSPINNER_API FYarnInstruction
{
	GENERATED_BODY()

	/** The type of instruction */
	UPROPERTY()
	EYarnInstructionType Type = EYarnInstructionType::Invalid;

	/** String operand (used by various instructions) */
	UPROPERTY()
	FString StringOperand;

	/** Integer operand (destination for jumps, counts, etc.) */
	UPROPERTY()
	int32 IntOperand = 0;

	/** Float operand (for push float) */
	UPROPERTY()
	float FloatOperand = 0.0f;

	/** Bool operand (for push bool, hasCondition) */
	UPROPERTY()
	bool BoolOperand = false;

	/** Second integer operand (for saliency complexity score) */
	UPROPERTY()
	int32 IntOperand2 = 0;

	FYarnInstruction() = default;

	// Factory methods for creating specific instruction types

	static FYarnInstruction JumpTo(int32 Destination);
	static FYarnInstruction PeekAndJump();
	static FYarnInstruction RunLine(const FString& LineID, int32 SubstitutionCount);
	static FYarnInstruction RunCommand(const FString& CommandText, int32 SubstitutionCount);
	static FYarnInstruction AddOption(const FString& LineID, int32 Destination, int32 SubstitutionCount, bool bHasCondition);
	static FYarnInstruction ShowOptions();
	static FYarnInstruction PushString(const FString& Value);
	static FYarnInstruction PushFloat(float Value);
	static FYarnInstruction PushBool(bool bValue);
	static FYarnInstruction JumpIfFalse(int32 Destination);
	static FYarnInstruction Pop();
	static FYarnInstruction CallFunction(const FString& FunctionName);
	static FYarnInstruction PushVariable(const FString& VariableName);
	static FYarnInstruction StoreVariable(const FString& VariableName);
	static FYarnInstruction Stop();
	static FYarnInstruction RunNode(const FString& NodeName);
	static FYarnInstruction PeekAndRunNode();
	static FYarnInstruction DetourToNode(const FString& NodeName);
	static FYarnInstruction PeekAndDetourToNode();
	static FYarnInstruction Return();
	static FYarnInstruction AddSaliencyCandidate(const FString& ContentID, int32 ComplexityScore, int32 Destination);
	static FYarnInstruction AddSaliencyCandidateFromNode(const FString& NodeName, int32 Destination);
	static FYarnInstruction SelectSaliencyCandidate();
};

/**
 * A header on a Yarn node.
 */
USTRUCT()
struct YARNSPINNER_API FYarnHeader
{
	GENERATED_BODY()

	UPROPERTY()
	FString Key;

	UPROPERTY()
	FString Value;

	FYarnHeader() = default;
	FYarnHeader(const FString& InKey, const FString& InValue)
		: Key(InKey), Value(InValue) {}
};

/**
 * A node in a Yarn program.
 */
USTRUCT()
struct YARNSPINNER_API FYarnNode
{
	GENERATED_BODY()

	/** The name of this node */
	UPROPERTY()
	FString Name;

	/** The instructions in this node */
	UPROPERTY()
	TArray<FYarnInstruction> Instructions;

	/** The headers on this node */
	UPROPERTY()
	TArray<FYarnHeader> Headers;

	/** Whether this node is a node group hub (Yarn Spinner 3.1) */
	UPROPERTY()
	bool bIsNodeGroupHub = false;

	/** Member nodes if this is a node group hub */
	UPROPERTY()
	TArray<FString> NodeGroupMembers;

	/** Get a header value by key */
	FString GetHeaderValue(const FString& Key) const;

	/** Check if a header exists */
	bool HasHeader(const FString& Key) const;

	/**
	 * Get the tracking variable name for this node (used by visited()/visited_count()).
	 * Derived from the "tracking" header if present.
	 * @return The tracking variable name, or empty string if not set.
	 */
	FString GetTrackingVariableName() const;

	/**
	 * Get the saliency condition variables for this node.
	 * Used by AddSaliencyCandidateFromNode to evaluate smart variables.
	 * @return Array of variable names to evaluate for saliency conditions.
	 */
	TArray<FString> GetContentSaliencyConditionVariables() const;

	/**
	 * Get the saliency condition complexity score for this node.
	 * @return The complexity score (higher = more specific content).
	 */
	int32 GetContentSaliencyConditionComplexityScore() const;

	/**
	 * Get all line IDs referenced by this node.
	 * Used by PrepareForLinesHandler for pre-loading.
	 * @return Array of line IDs.
	 */
	TArray<FString> GetLineIDs() const;
};

/**
 * A compiled Yarn program.
 * Contains all nodes and initial variable values.
 */
USTRUCT()
struct YARNSPINNER_API FYarnProgram
{
	GENERATED_BODY()

	/** The name of this program */
	UPROPERTY()
	FString Name;

	/** The nodes in this program, keyed by name */
	UPROPERTY()
	TMap<FString, FYarnNode> Nodes;

	/** Initial values for variables */
	UPROPERTY()
	TMap<FString, FYarnValue> InitialValues;

	/** The language version this program was compiled with */
	UPROPERTY()
	int32 LanguageVersion = 0;

	/** Get a node by name */
	const FYarnNode* GetNode(const FString& NodeName) const;

	/** Check if a node exists */
	bool HasNode(const FString& NodeName) const;

	/** Get the initial value for a variable */
	bool TryGetInitialValue(const FString& VariableName, FYarnValue& OutValue) const;

	/**
	 * Get all line IDs for a specific node.
	 * Used by PrepareForLinesHandler for pre-loading localisation.
	 * @param NodeName The name of the node.
	 * @return Array of line IDs referenced by the node.
	 */
	TArray<FString> GetLineIDsForNode(const FString& NodeName) const;
};

class UAssetImportData;

// ============================================================================
// FYarnLocalization
// ============================================================================
//
// Stores the translated strings for a single locale/culture. Each culture
// (e.g., "de", "fr", "pt-BR") has its own FYarnLocalization in the project.
//
// The runtime strings table allows adding translations at runtime without
// modifying the asset - useful for user-generated content or dynamic text.

/**
 * Localisation data for a single culture/language.
 *
 * Contains the translated strings for one locale. The project stores a map
 * of these (culture code -> localisation data).
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnLocalization
{
	GENERATED_BODY()

	/** Translated strings for this culture (line ID -> translated text) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Localization")
	TMap<FString, FString> Strings;

	/**
	 * Runtime-added strings (not persisted to asset).
	 * These are checked first, allowing runtime overrides.
	 */
	TMap<FString, FString> RuntimeStrings;

	/** Path to localised assets directory (relative to .yarnproject) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Localization")
	FString AssetsPath;

	/** The source CSV file this was loaded from (for reimporting) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Localization")
	FString SourceCSVPath;

	/**
	 * Get localised string, checking runtime table first.
	 * Runtime strings override persistent strings.
	 */
	FString GetLocalizedString(const FString& LineID) const
	{
		// Check runtime strings first so they can override persistent strings
		if (const FString* RuntimeText = RuntimeStrings.Find(LineID))
		{
			return *RuntimeText;
		}
		// Then check persistent strings
		if (const FString* Text = Strings.Find(LineID))
		{
			return *Text;
		}
		return FString();
	}

	/** Check if a localised string exists (in either table) */
	bool ContainsLocalizedString(const FString& LineID) const
	{
		return RuntimeStrings.Contains(LineID) || Strings.Contains(LineID);
	}

	/**
	 * Add a runtime string (not persisted to asset).
	 * Useful for user-generated content or dynamic translations.
	 */
	void AddLocalizedString(const FString& LineID, const FString& Text)
	{
		RuntimeStrings.Add(LineID, Text);
	}

	/** Clear all data (both persistent and runtime) */
	void Clear()
	{
		Strings.Empty();
		RuntimeStrings.Empty();
	}
};

// ============================================================================
// UYarnProject
// ============================================================================
//
// WHAT THIS IS:
// This is the main asset type for Yarn Spinner. You create one by importing
// a .yarnproject file into Unreal. It contains:
//   - the compiled bytecode (FYarnProgram)
//   - base language strings
//   - localised strings for other languages
//   - line metadata
//
// BLUEPRINT VS C++ USAGE:
// Blueprint users:
//   - assign a UYarnProject to the dialogue runner in the editor
//   - use GetBaseText/GetLocalizedText for custom text lookup
//   - use HasNode to check if a node exists before starting
//
// C++ users:
//   - can access Program directly for bytecode introspection
//   - can access BaseStringTable and Localizations for text lookup

/**
 * A Yarn project asset.
 *
 * Contains a compiled program and associated localisation data.
 * Created by importing a .yarnproject file into the editor.
 *
 * @see UYarnDialogueRunner which uses this to run dialogue
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnProject : public UObject
{
	GENERATED_BODY()

public:
	// ========================================================================
	// Import settings
	// ========================================================================

	/** Import data for reimporting from source files (editor only) */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import Settings")
	UAssetImportData* AssetImportData;

	// ========================================================================
	// Compiled program data
	// ========================================================================

	/** The compiled program containing nodes and bytecode */
	UPROPERTY()
	FYarnProgram Program;

	// ========================================================================
	// Localisation data
	// ========================================================================

	/**
	 * Base language code (e.g., "en", "en-US").
	 * This is the language of the original Yarn scripts.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Localization")
	FString BaseLanguage;

	/**
	 * Base string table containing line text in the base language.
	 * Maps line ID -> text (e.g., "line:Start-0" -> "Hello, world!").
	 */
	UPROPERTY()
	TMap<FString, FString> BaseStringTable;

	/**
	 * Per-culture localisations.
	 * Maps culture code -> localisation data (e.g., "de" -> FYarnLocalization).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Localization")
	TMap<FString, FYarnLocalization> Localizations;

	/** Metadata for lines (line ID -> metadata string) */
	UPROPERTY()
	TMap<FString, FString> LineMetadata;

	// ========================================================================
	// Node information
	// ========================================================================

	/**
	 * List of all node names in the project.
	 * Useful for creating node selection dropdowns in editor UI.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner")
	TArray<FString> NodeNames;

#if WITH_EDITORONLY_DATA
	/** The source .yarnproject file path on disk */
	UPROPERTY()
	FString SourceProjectPath;

	/** Resolved .yarn source file paths from sourceFiles globs */
	UPROPERTY()
	TArray<FString> ResolvedSourceFiles;
#endif

	// ========================================================================
	// Blueprint-callable methods
	// ========================================================================

	/**
	 * Get the text for a line ID in the base language.
	 * @param LineID The line ID to look up (e.g., "line:Start-0").
	 * @return The line text, or empty string if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	FString GetBaseText(const FString& LineID) const;

	/**
	 * Get the localised text for a line ID in a specific culture.
	 * Falls back to base text if the culture or line is not found.
	 * @param LineID The line ID to look up.
	 * @param CultureCode The culture code (e.g., "fr", "de", "pt-BR").
	 * @return The localised text, or base text if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	FString GetLocalizedText(const FString& LineID, const FString& CultureCode) const;

	/**
	 * Check if localisation data exists for a specific culture.
	 * @param CultureCode The culture code to check.
	 * @return True if localisation exists for this culture.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	bool HasLocalization(const FString& CultureCode) const;

	/**
	 * Get all available culture codes that have localisation data.
	 * Includes the base language.
	 * @return Array of culture codes (e.g., ["en", "de", "fr"]).
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	TArray<FString> GetAvailableCultures() const;

	/**
	 * Check if the project contains a node with the given name.
	 * Useful for validating node names before starting dialogue.
	 * @param NodeName The node name to check (e.g., "Start", "Chapter1").
	 * @return True if the node exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	bool HasNode(const FString& NodeName) const;

	/**
	 * Get the initial value for a variable defined in the Yarn scripts.
	 * Variables declared with <<declare>> in Yarn have initial values.
	 * @param VariableName The variable name (must start with $).
	 * @param OutValue The initial value, if found.
	 * @return True if the variable has an initial value defined.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	bool TryGetInitialValue(const FString& VariableName, FYarnValue& OutValue) const;

	// ========================================================================
	// UObject interface
	// ========================================================================

	virtual void PostInitProperties() override;
#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

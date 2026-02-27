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
#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "YarnProjectFactory.generated.h"

class UYarnProject;

/**
 * Factory for importing .yarnproject files.
 *
 * This factory handles:
 * - Importing .yarnproject files
 * - Calling ysc to compile the yarn scripts
 * - Parsing the compiled output
 * - Creating UYarnProject assets
 */
UCLASS()
class YARNSPINNEREDITOR_API UYarnProjectFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UYarnProjectFactory();

	// UFactory interface
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

	// FReimportHandler interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;

private:
	/**
	 * Compile a yarn project using ysc.
	 * @param ProjectPath Path to the .yarnproject file.
	 * @param OutCompiledPath Output path to the .yarnc file.
	 * @param OutLinesPath Output path to the lines CSV.
	 * @param OutMetadataPath Output path to the metadata CSV.
	 * @param OutError Error message if compilation failed.
	 * @return True if compilation succeeded.
	 */
	bool CompileYarnProject(const FString& ProjectPath, FString& OutCompiledPath, FString& OutLinesPath,
		FString& OutMetadataPath, FString& OutError);

	/**
	 * Parse a compiled .yarnc file.
	 * @param CompiledPath Path to the .yarnc file.
	 * @param OutProgram Output program data.
	 * @param OutError Error message if parsing failed.
	 * @return True if parsing succeeded.
	 */
	bool ParseCompiledProgram(const FString& CompiledPath, struct FYarnProgram& OutProgram, FString& OutError);

	/**
	 * Parse a lines CSV file.
	 * @param LinesPath Path to the lines CSV.
	 * @param OutStringTable Output string table.
	 * @param OutError Error message if parsing failed.
	 * @return True if parsing succeeded.
	 */
	bool ParseLinesCSV(const FString& LinesPath, TMap<FString, FString>& OutStringTable, FString& OutError);

	/**
	 * Parse a metadata CSV file.
	 * @param MetadataPath Path to the metadata CSV.
	 * @param OutMetadata Output metadata table.
	 * @param OutError Error message if parsing failed.
	 * @return True if parsing succeeded.
	 */
	bool ParseMetadataCSV(const FString& MetadataPath, TMap<FString, FString>& OutMetadata, FString& OutError);

	/**
	 * Get the path to the ysc executable.
	 * @return The path to ysc.
	 */
	FString GetYscPath() const;

	/**
	 * Parse the .yarnproject JSON file for localization settings.
	 * Loads baseLanguage and localisation CSV files.
	 * @param ProjectPath Path to the .yarnproject file.
	 * @param ProjectDir Directory containing the .yarnproject file.
	 * @param OutYarnProject The project to populate with localization data.
	 * @param OutError Error message if parsing failed.
	 * @return True if parsing succeeded.
	 */
	bool ParseYarnProjectLocalization(const FString& ProjectPath, const FString& ProjectDir,
		UYarnProject* OutYarnProject, FString& OutError);

	/**
	 * Parse a localization CSV file into a string map.
	 * @param CSVPath Path to the CSV file.
	 * @param OutStrings Output string map (line ID -> text).
	 * @param OutError Error message if parsing failed.
	 * @return True if parsing succeeded.
	 */
	bool ParseLocalizationCSV(const FString& CSVPath, TMap<FString, FString>& OutStrings, FString& OutError);

	/**
	 * Parse a single CSV line, handling quoted fields.
	 * @param Line The CSV line to parse.
	 * @param OutFields Output array of field values.
	 */
	static void ParseCSVLine(const FString& Line, TArray<FString>& OutFields);
};

/**
 * Lightweight protobuf parser for Yarn Spinner compiled files.
 */
class YARNSPINNEREDITOR_API FYarnProtobufParser
{
public:
	FYarnProtobufParser(const TArray<uint8>& InData);

	/** Parse a Yarn Program from the data */
	bool ParseProgram(struct FYarnProgram& OutProgram, FString& OutError);

private:
	const TArray<uint8>& Data;
	int32 Position;

	/** Read a varint from the data */
	uint64 ReadVarint();

	/** Read a fixed 32-bit value */
	uint32 ReadFixed32();

	/** Read a fixed 64-bit value */
	uint64 ReadFixed64();

	/** Read a length-prefixed string */
	FString ReadString();

	/** Read a length-prefixed bytes */
	TArray<uint8> ReadBytes();

	/** Skip a field of the given wire type */
	void SkipField(int32 WireType);

	/** Parse a Node message */
	bool ParseNode(struct FYarnNode& OutNode);

	/** Parse an Instruction message */
	bool ParseInstruction(struct FYarnInstruction& OutInstruction);

	/** Parse an Operand message */
	bool ParseOperand(struct FYarnValue& OutValue);
};

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
	TArray<uint8> Data;
	int32 Position;

	/** Read a varint from the data */
	uint64 ReadVarint();

	/** Read a fixed 32-bit value */
	uint32 ReadFixed32();

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

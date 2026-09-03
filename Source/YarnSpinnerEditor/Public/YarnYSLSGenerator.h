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

class YARNSPINNEREDITOR_API FYarnYSLSGenerator
{
public:
	static bool GenerateForProject(const FString& YarnProjectPath);

	static bool GenerateForProjects(const TArray<FString>& YarnProjectPaths);

	static void RefreshCommandRegistry();

	static void WriteBakedRegistry(TArray<struct FYarnBakedActionEntry> Entries);
};

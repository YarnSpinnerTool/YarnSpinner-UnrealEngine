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
#include "Engine/DeveloperSettings.h"
#include "YarnSpinnerEditorSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Yarn Spinner"))
class YARNSPINNEREDITOR_API UYarnSpinnerEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UYarnSpinnerEditorSettings();

	UPROPERTY(config, EditAnywhere, Category = "Yarn Spinner|Compiler", meta = (FilePathFilter = "*"))
	FFilePath YscPath;
};

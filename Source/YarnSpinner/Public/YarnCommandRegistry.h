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
#include "YarnCommandRegistry.generated.h"

USTRUCT()
struct YARNSPINNER_API FYarnBakedActionEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Yarn Spinner")
	FString YarnName;

	UPROPERTY(VisibleAnywhere, Category = "Yarn Spinner")
	FSoftClassPath OwningClass;

	UPROPERTY(VisibleAnywhere, Category = "Yarn Spinner")
	FName FunctionName;

	UPROPERTY(VisibleAnywhere, Category = "Yarn Spinner")
	bool bIsFunction = false;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Yarn Spinner Command Registry"))
class YARNSPINNER_API UYarnCommandRegistrySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UYarnCommandRegistrySettings();

	UPROPERTY(config, VisibleAnywhere, Category = "Yarn Spinner")
	TArray<FYarnBakedActionEntry> BakedActions;

	void GetEntriesForClass(const UClass* Class, TArray<FYarnBakedActionEntry>& OutEntries) const;
};

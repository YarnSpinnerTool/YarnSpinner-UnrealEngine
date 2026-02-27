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
#include "Modules/ModuleManager.h"

YARNSPINNER_API DECLARE_LOG_CATEGORY_EXTERN(LogYarnSpinner, Log, All);

/**
 * Yarn Spinner runtime module.
 *
 * Provides the core dialogue system functionality including the virtual machine,
 * dialogue runner, variable storage, and dialogue presenters.
 */
class FYarnSpinnerModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Gets the singleton instance of the Yarn Spinner module.
	 * @return The Yarn Spinner module instance.
	 */
	static FYarnSpinnerModule& Get();

	/**
	 * Checks if the module is loaded and ready.
	 * @return True if the module is loaded.
	 */
	static bool IsAvailable();
};

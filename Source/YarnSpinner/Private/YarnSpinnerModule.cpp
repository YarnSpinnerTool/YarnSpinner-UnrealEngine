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

#include "YarnSpinnerModule.h"

DEFINE_LOG_CATEGORY(LogYarnSpinner);

#define LOCTEXT_NAMESPACE "FYarnSpinnerModule"

void FYarnSpinnerModule::StartupModule()
{
}

void FYarnSpinnerModule::ShutdownModule()
{
}

FYarnSpinnerModule& FYarnSpinnerModule::Get()
{
	return FModuleManager::LoadModuleChecked<FYarnSpinnerModule>("YarnSpinner");
}

bool FYarnSpinnerModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("YarnSpinner");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FYarnSpinnerModule, YarnSpinner)

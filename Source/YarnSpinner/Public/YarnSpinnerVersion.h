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
// YarnSpinnerVersion.h
// ============================================================================
//
// Centralized version detection and feature macros for cross-version
// compatibility between Unreal Engine 4.27 and 5.x.
//
// USAGE:
// Include this header in any file that needs version-specific behaviour.
// Use the YARNSPINNER_* macros to conditionally compile code.

// ENGINE_MAJOR_VERSION and ENGINE_MINOR_VERSION are defined by Unreal's build system
// They're available via CoreMinimal.h or can be accessed directly

// ============================================================================
// Engine Version Macros
// ============================================================================

// Check for UE5 or later
#define YARNSPINNER_ENGINE_VERSION_5_OR_LATER (ENGINE_MAJOR_VERSION >= 5)

// Check for specific UE5 minor versions
#define YARNSPINNER_ENGINE_VERSION_5_0_OR_LATER (ENGINE_MAJOR_VERSION >= 5)
#define YARNSPINNER_ENGINE_VERSION_5_1_OR_LATER (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1)
#define YARNSPINNER_ENGINE_VERSION_5_2_OR_LATER (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2)
#define YARNSPINNER_ENGINE_VERSION_5_3_OR_LATER (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3)
#define YARNSPINNER_ENGINE_VERSION_5_4_OR_LATER (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4)
#define YARNSPINNER_ENGINE_VERSION_5_5_OR_LATER (ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5)

// Check for UE 4.27 specifically
#define YARNSPINNER_ENGINE_VERSION_4_27 (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 27)

// ============================================================================
// Feature Availability Macros
// ============================================================================

/**
 * Enhanced Input System is available in UE5+.
 * In UE 4.27, we fall back to legacy input.
 */
#define YARNSPINNER_WITH_ENHANCED_INPUT YARNSPINNER_ENGINE_VERSION_5_OR_LATER

/**
 * TObjectPtr<> smart pointer is UE5+ only.
 * In UE 4.27, use raw pointers with UPROPERTY.
 */
#define YARNSPINNER_WITH_TOBJECTPTR YARNSPINNER_ENGINE_VERSION_5_OR_LATER

/**
 * FInputActionValue is part of Enhanced Input (UE5+).
 */
#define YARNSPINNER_WITH_INPUT_ACTION_VALUE YARNSPINNER_ENGINE_VERSION_5_OR_LATER

/**
 * UEnhancedInputComponent is UE5+ only.
 */
#define YARNSPINNER_WITH_ENHANCED_INPUT_COMPONENT YARNSPINNER_ENGINE_VERSION_5_OR_LATER

/**
 * UEnhancedInputLocalPlayerSubsystem is UE5+ only.
 */
#define YARNSPINNER_WITH_ENHANCED_INPUT_SUBSYSTEM YARNSPINNER_ENGINE_VERSION_5_OR_LATER

// ============================================================================
// API Compatibility Macros
// ============================================================================

#define YARNSPINNER_ADD_MOVEMENT_INPUT AddMovementInput
#define YARNSPINNER_ADD_CONTROLLER_YAW_INPUT AddControllerYawInput
#define YARNSPINNER_ADD_CONTROLLER_PITCH_INPUT AddControllerPitchInput

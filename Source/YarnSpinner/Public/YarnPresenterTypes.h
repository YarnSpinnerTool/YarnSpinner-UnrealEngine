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
// YarnPresenterTypes.h
// ============================================================================
//
// Small shared header that holds the types crossing the presenter <-> runner
// boundary: the completion-request enum and the two callback delegates. Lives
// here so both UYarnDialoguePresenter and UYarnDialogueRunner can see them
// without including each other.

#include "CoreMinimal.h"

#include "YarnPresenterTypes.generated.h"

// ============================================================================
// EYarnLineCompletionRequest
// ============================================================================
//
// What a presenter says about the *line* when it signals its own completion.
// Two cases that the framework genuinely needs to tell apart:
//
//   - None: "I'm done. No special request about the line." The everyday
//     case. A typewriter reaching the end of its text. A particle trigger
//     that fired and immediately returned. A subtitle waiting to be told
//     when to leave. The runner (or wrapper above) waits for the others.
//
//   - EndLine: "I'm done, and I'd like the line to end now." The
//     line-driving case. A voice-over presenter whose audio reached its
//     natural end. A cutscene presenter whose cinematic finished. The
//     runner cancels the current-content source on receipt, which cascades
//     to siblings via their tokens; they wrap up and signal in turn.
//
// The presenter chooses per call. A voice-over that finishes naturally
// emits EndLine; the same voice-over cancelled mid-play emits None because
// the line was already ending for some other reason.

/** What a presenter requests about the *line* (not just itself) at the
 *  moment it signals completion. */
UENUM(BlueprintType)
enum class EYarnLineCompletionRequest : uint8
{
	/** I'm done. No request about whether the line should end. */
	None UMETA(DisplayName = "None"),

	/** I'm done, and I'd like the line to end now. The line-driving case. */
	EndLine UMETA(DisplayName = "End Line"),
};

// ============================================================================
// FOnYarnLineFinished / FOnYarnOptionSelected
// ============================================================================
//
// Dynamic delegates the runner (or a wrapper) hands to a presenter when it
// calls Internal_RunLine / Internal_RunOptions. The presenter fires the
// delegate to signal completion.
//
// Dynamic so the bound side can be a UFUNCTION on a UObject (which the
// runner and wrappers are). Storage on the presenter happens to be fine
// because dynamic delegates carry a UObject target and a function name;
// no captured state on the presenter side.

/** Fired by a presenter when it's done presenting a line. The request
 *  argument tells the parent what to do about the line as a whole. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnYarnLineFinished,
	EYarnLineCompletionRequest, Request);

/** Fired by a presenter when the player picks an option. Carries the
 *  zero-based index of the chosen option. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnYarnOptionSelected,
	int32, OptionIndex);

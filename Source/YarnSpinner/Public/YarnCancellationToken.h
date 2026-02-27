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
// YarnCancellationToken.h
// ============================================================================
//
// The cancellation token system provides a two-tier interruption mechanism for
// dialogue presentation. This lets players control the pace of dialogue:
//   - First press: hurry up (skip typewriter, show full text)
//   - Second press: next line (advance to next content)
//
// THREAD SAFETY:
// The token uses std::atomic<bool> for both flags, with proper memory ordering.
// This means flags can be safely checked from any thread (e.g., audio thread)
// while the main thread marks them.
//
// HOW TO USE IN PRESENTERS:
//   1. Periodically poll IsHurryUpRequested() during presentation
//   2. If true, speed up (complete typewriter, skip delays) but keep presenting
//   3. Periodically poll IsNextContentRequested()
//   4. If true, finish immediately and call OnLinePresentationComplete()
//
// You can also use the dialogue runner's RequestHurryUp() and RequestNextLine()
// methods which update the current token.

#include "CoreMinimal.h"
#include <atomic>
#include "YarnCancellationToken.generated.h"

// ============================================================================
// FYarnLineCancellationToken
// ============================================================================

/**
 * A two-tier cancellation token for dialogue presentation.
 *
 * Provides two levels of interruption that presenters can respond to:
 *   - Hurry-up: speed up but keep presenting (first button press)
 *   - Next content: stop immediately and advance (second button press)
 *
 * The flags are hierarchical - if next content is requested, hurry-up is
 * also considered requested (you can't skip straight to next without hurrying).
 *
 * Thread safety: uses std::atomic for safe multi-threaded access.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnLineCancellationToken
{
	GENERATED_BODY()

	/** Default constructor - no cancellation requested */
	FYarnLineCancellationToken()
		: bHurryUpRequested(false)
		, bNextContentRequested(false)
	{
	}

	/** Copy constructor - required because std::atomic is not copyable */
	FYarnLineCancellationToken(const FYarnLineCancellationToken& Other)
		: bHurryUpRequested(Other.bHurryUpRequested.load(std::memory_order_acquire))
		, bNextContentRequested(Other.bNextContentRequested.load(std::memory_order_acquire))
	{
	}

	/** Copy assignment - required because std::atomic is not copyable */
	FYarnLineCancellationToken& operator=(const FYarnLineCancellationToken& Other)
	{
		if (this != &Other)
		{
			bHurryUpRequested.store(Other.bHurryUpRequested.load(std::memory_order_acquire), std::memory_order_release);
			bNextContentRequested.store(Other.bNextContentRequested.load(std::memory_order_acquire), std::memory_order_release);
		}
		return *this;
	}

	/**
	 * Check if hurry-up has been requested.
	 *
	 * When true, presenters should:
	 * - Skip or speed up animations
	 * - Show full text immediately instead of typewriter effect
	 * - Skip optional delays
	 *
	 * Returns true if either hurry-up OR next-content was requested.
	 */
	bool IsHurryUpRequested() const
	{
		return bHurryUpRequested.load(std::memory_order_acquire) ||
		       bNextContentRequested.load(std::memory_order_acquire);
	}

	/**
	 * Check if next content has been requested.
	 *
	 * When true, presenters should:
	 * - Stop presentation entirely
	 * - Clean up any ongoing effects
	 * - Signal completion as soon as possible
	 */
	bool IsNextContentRequested() const
	{
		return bNextContentRequested.load(std::memory_order_acquire);
	}

	/**
	 * Check if any cancellation has been requested.
	 */
	bool IsAnyCancellationRequested() const
	{
		return bHurryUpRequested.load(std::memory_order_acquire) ||
		       bNextContentRequested.load(std::memory_order_acquire);
	}

	/**
	 * Mark that hurry-up has been requested.
	 *
	 * Call this when the player wants to speed up presentation
	 * (e.g., pressed a button while text is appearing).
	 */
	void MarkHurryUpRequested()
	{
		bHurryUpRequested.store(true, std::memory_order_release);
	}

	/**
	 * Mark that next content has been requested.
	 *
	 * Call this when the player wants to skip to the next line entirely
	 * (e.g., pressed a button after text is fully displayed).
	 */
	void MarkNextContentRequested()
	{
		bNextContentRequested.store(true, std::memory_order_release);
	}

	/**
	 * Reset the token to initial state (no cancellation).
	 */
	void Reset()
	{
		bHurryUpRequested.store(false, std::memory_order_release);
		bNextContentRequested.store(false, std::memory_order_release);
	}

private:
	/** Whether hurry-up has been requested (thread-safe) */
	std::atomic<bool> bHurryUpRequested;

	/** Whether next content has been requested (thread-safe) */
	std::atomic<bool> bNextContentRequested;
};

// ============================================================================
// UYarnCancellationTokenSource
// ============================================================================
//
// A UObject wrapper that manages cancellation tokens for the dialogue runner.
// Each new line gets a fresh token (via GetToken), and the runner propagates
// player requests through here.
//
// You typically don't interact with this directly - use the dialogue runner's
// RequestHurryUp() and RequestNextLine() methods instead.

/**
 * Manages cancellation tokens for a dialogue runner.
 *
 * Creates and manages the cancellation tokens used during line presentation.
 * Each line gets a fresh token, and the manager handles propagating requests
 * from the dialogue runner to the current token.
 *
 * This is an internal implementation detail - blueprint users should use
 * the dialogue runner's RequestHurryUp() and RequestNextLine() methods.
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnCancellationTokenSource : public UObject
{
	GENERATED_BODY()

public:
	UYarnCancellationTokenSource();

	/**
	 * Get a new token for presenting a line.
	 *
	 * Resets any previous cancellation state and returns a fresh token.
	 * The returned reference is valid until the next call to GetToken().
	 * Called by the dialogue runner at the start of each line.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	FYarnLineCancellationToken& GetToken();

	/**
	 * Request hurry-up on the current token.
	 * Presenters checking IsHurryUpRequested() will now see true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	void RequestHurryUp();

	/**
	 * Request next content on the current token.
	 * Presenters checking IsNextContentRequested() will now see true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	void RequestNextContent();

	/**
	 * Cancel everything (equivalent to RequestNextContent).
	 * Call this when dialogue is being stopped entirely.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	void CancelAll();

	/** Check if hurry-up has been requested on the current token */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	bool IsHurryUpRequested() const { return CurrentToken.IsHurryUpRequested(); }

	/** Check if next content has been requested on the current token */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	bool IsNextContentRequested() const { return CurrentToken.IsNextContentRequested(); }

private:
	/** The current cancellation token used for the active line */
	FYarnLineCancellationToken CurrentToken;
};

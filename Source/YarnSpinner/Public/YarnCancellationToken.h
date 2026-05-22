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
// Cancellation in Yarn Spinner follows the .NET CancellationToken model.
//
// There are two types:
//   - UYarnCancellationTokenSource holds the actual cancellation state. It can
//     be cancelled (Cancel), it can hold a hurry-up flag (RequestHurryUp), and
//     it can produce any number of tokens that refer back to it (GetToken).
//   - FYarnLineCancellationToken is a small handle that holds a weak reference
//     to a source. Asking a token "are you cancelled?" forwards the question
//     to its source. Tokens never own state; they only observe.
//
// Tokens are the "read" side; sources are the "write" side. A presenter is
// given a token (so it can observe) but never the source (so it cannot affect
// cancellation for everyone else). Whoever owns the source decides when to
// cancel.
//
// LINKED SOURCES
//
// CreateLinkedTokenSource builds a new source whose state is inherited from
// one or more parent tokens. If any parent is cancelled, the linked child is
// cancelled. Cancelling the child has no effect on its parents.
//
// This is how multi-level cancellation works without anyone needing to write
// propagation by hand. Examples:
//
//   - Runner creates a "dialogue" source for the whole conversation.
//   - For each line, it creates a "current content" source linked to the
//     dialogue source. Presenters get tokens from the content source. Tearing
//     down the dialogue cancels everything; ending a single line cancels only
//     that line.
//
//   - A wrapper presenter (interruption, etc) takes the token it was given,
//     creates its own linked source from that token, and gives tokens from
//     the linked source to its subordinates. When the wrapper wants to stop
//     its subordinates without telling the parent, it cancels its own source.
//
// THE HURRY-UP AXIS
//
// Hurry-up is a separate signal from cancellation. "Cancel" means "stop";
// "hurry up" means "go faster but keep going". Both are propagated through
// linked sources, but per-axis: a caller can choose to link only one or both.
//
// THREAD SAFETY
//
// The source's flags are std::atomic, so any thread can read or set them
// safely. Callback registration uses a critical section. Callback invocation
// copies the list under the lock and then releases it before firing, so
// callbacks may freely re-enter the source.

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <atomic>

#include "YarnCancellationToken.generated.h"

class UYarnCancellationTokenSource;

// ============================================================================
// FYarnLineCancellationToken
// ============================================================================
//
// A handle that observes a UYarnCancellationTokenSource. Cheap to copy.
// Default-constructed tokens are never cancelled (matches .NET behaviour for
// CancellationToken.None).
//
// The historic name keeps the "Line" prefix even though the type is now used
// at multiple levels (dialogue, content, wrapper). Renaming has too much
// public-API blast radius; we live with the misnomer.

/**
 * A cancellation token. Lightweight handle holding a weak reference to a
 * UYarnCancellationTokenSource.
 *
 * Tokens observe state. They cannot change it. To cancel, call Cancel() on
 * the source that produced this token.
 *
 * A default-constructed token is permanently uncancelled.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnLineCancellationToken
{
	GENERATED_BODY()

	FYarnLineCancellationToken() = default;

	/** Returns true if the source has been cancelled. Default tokens return false. */
	bool IsCancellationRequested() const;

	/** Returns true if the source has hurry-up set. Default tokens return false. */
	bool IsHurryUpRequested() const;

	/**
	 * Returns true if this token observes a live source.
	 *
	 * A default-constructed token (no source) or a token whose source has
	 * been garbage-collected returns false. The same semantics as .NET's
	 * CancellationToken.CanBeCanceled property: not "is it currently
	 * cancelled", but "could it ever become so".
	 *
	 * Use this when you want to distinguish "the token is wired up but
	 * its source hasn't signalled" from "there is no token here at all".
	 */
	bool CanBeCancelled() const { return Source.IsValid(); }

	/**
	 * Returns true if any cancellation or hurry-up has been requested.
	 *
	 * Useful as a single check for "should I stop or speed up?". Most
	 * presenters that branch on either of those want this.
	 */
	bool IsAnyCancellationRequested() const;

	// ------------------------------------------------------------------------
	// Deprecated aliases
	// ------------------------------------------------------------------------
	// Kept around for one version while callers migrate. The signal didn't
	// change; only the name got more honest about what it actually means.

	UE_DEPRECATED(5.0, "Use IsCancellationRequested instead.")
	bool IsNextContentRequested() const { return IsCancellationRequested(); }

	UE_DEPRECATED(5.0, "Cancellation is now driven from the source. Call Cancel() on the UYarnCancellationTokenSource that produced this token.")
	void MarkNextContentRequested();

	UE_DEPRECATED(5.0, "Hurry-up is now driven from the source. Call RequestHurryUp() on the UYarnCancellationTokenSource that produced this token.")
	void MarkHurryUpRequested();

	UE_DEPRECATED(5.0, "Tokens no longer own state and so cannot be reset. Call Reset() on the UYarnCancellationTokenSource instead.")
	void Reset();

private:
	friend class UYarnCancellationTokenSource;

	/** Weak ref so tokens never keep sources alive. If the source is gone,
	 *  the token reports "not cancelled" (matches .NET CancellationToken.None). */
	TWeakObjectPtr<UYarnCancellationTokenSource> Source;
};

// ============================================================================
// UYarnCancellationTokenSource
// ============================================================================
//
// Owns the cancellation state. Hands out tokens that observe it. Hands out
// linked child sources whose state inherits from parent tokens.

/**
 * A cancellation token source.
 *
 * Holds the actual cancellation and hurry-up state. Produces tokens
 * (FYarnLineCancellationToken) that report on that state. Can be cancelled
 * directly (Cancel) or linked to a parent token so it cancels when the parent
 * does (CreateLinkedTokenSource).
 *
 * The dialogue runner creates one source for the whole conversation and a
 * second, linked one for each line. Wrappers around presenters create their
 * own linked sources to govern their subordinates.
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnCancellationTokenSource : public UObject
{
	GENERATED_BODY()

public:
	UYarnCancellationTokenSource();

	// ------------------------------------------------------------------------
	// Tokens
	// ------------------------------------------------------------------------

	/**
	 * Produce a token that observes this source.
	 *
	 * Any number of tokens may refer to the same source. They are cheap;
	 * they hold only a weak pointer back here. Cancelling this source affects
	 * all tokens produced from it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	FYarnLineCancellationToken GetToken();

	// ------------------------------------------------------------------------
	// State
	// ------------------------------------------------------------------------

	/**
	 * Cancel this source. Idempotent.
	 *
	 * Fires any callbacks registered via the OnCancelled delegate before
	 * returning. Linked child sources see this and cancel themselves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	void Cancel();

	/**
	 * Set the hurry-up flag. Idempotent.
	 *
	 * Hurry-up is advisory: presenters may ignore it. It does not imply
	 * cancellation. Useful when the player wants the line to speed up but
	 * not actually end yet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	void RequestHurryUp();

	/**
	 * Clear cancellation and hurry-up. Drops registered callbacks.
	 *
	 * Used by the dialogue runner when reusing a source for a new line.
	 * Callers that have already obtained tokens from this source can keep
	 * using them; the state they observe is reset to "not cancelled".
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	void Reset();

	/** True if Cancel has been called. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	bool IsCancellationRequested() const;

	/** True if either cancellation or hurry-up has been requested. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	bool IsHurryUpRequested() const;

	// ------------------------------------------------------------------------
	// Linked sources
	// ------------------------------------------------------------------------

	/**
	 * Build a new source whose state is inherited from the given tokens.
	 *
	 * The new source is cancelled when any of the parent sources are
	 * cancelled, and its hurry-up is set when any parent has hurry-up.
	 * Cancelling the new source does NOT propagate up to the parents.
	 *
	 * Each axis can be linked independently. The defaults link both, which
	 * is what you want unless you specifically need to filter (e.g. a
	 * wrapper that mirrors cancellation but not hurry-up).
	 *
	 * If a parent token's source is already gone, that parent contributes
	 * nothing. If a parent is already cancelled at the time of linking, the
	 * new source is created already-cancelled.
	 *
	 * @param Outer            UObject outer for the new source.
	 * @param LinkedTokens     Tokens whose sources to inherit state from.
	 * @param bLinkCancellation If true, parent cancellation propagates here.
	 * @param bLinkHurryUp     If true, parent hurry-up propagates here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation", meta = (AutoCreateRefTerm = "LinkedTokens"))
	static UYarnCancellationTokenSource* CreateLinkedTokenSource(
		UObject* Outer,
		const TArray<FYarnLineCancellationToken>& LinkedTokens,
		bool bLinkCancellation = true,
		bool bLinkHurryUp = true);

	/**
	 * Detach this source from any parents it was linked to.
	 *
	 * A linked source registers callbacks on each parent. Those callbacks
	 * are normally cleaned up in BeginDestroy, i.e. on garbage collection.
	 * Call this when you're finished with a linked source but the parent
	 * lives on (e.g. the runner retiring a per-line source while the
	 * dialogue source persists), so the parent's callback list doesn't grow
	 * with dead entries until the next GC. Idempotent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation")
	void UnlinkFromParents();

	// ------------------------------------------------------------------------
	// Callbacks
	// ------------------------------------------------------------------------
	// Internal hooks used by linked sources and by presenters that want to
	// react to cancellation without polling. Not BlueprintCallable: TFunction
	// isn't exposable to Blueprint.

	/**
	 * Register a callback fired once when this source is cancelled.
	 *
	 * If the source is already cancelled at the time of registration, the
	 * callback is invoked synchronously before Register returns.
	 *
	 * The returned handle can be passed to UnregisterOnCancelled. Reset
	 * drops all callbacks unconditionally.
	 */
	FDelegateHandle RegisterOnCancelled(TFunction<void()> Callback);
	void UnregisterOnCancelled(FDelegateHandle Handle);

	/** Same shape, but for the hurry-up axis. */
	FDelegateHandle RegisterOnHurryUp(TFunction<void()> Callback);
	void UnregisterOnHurryUp(FDelegateHandle Handle);

	// ------------------------------------------------------------------------
	// Deprecated aliases
	// ------------------------------------------------------------------------
	// One-version migration window. Existing call sites that say "next
	// content" or "cancel all" still compile.

	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation", meta = (DeprecatedFunction, DeprecationMessage = "Use Cancel instead."))
	void RequestNextContent() { Cancel(); }

	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation", meta = (DeprecatedFunction, DeprecationMessage = "Use Cancel instead."))
	void CancelAll() { Cancel(); }

	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Cancellation", meta = (DeprecatedFunction, DeprecationMessage = "Use IsCancellationRequested instead."))
	bool IsNextContentRequested() const { return IsCancellationRequested(); }

protected:
	virtual void BeginDestroy() override;

private:
	// State. Atomic so any thread can observe; cancellation is one-way (false
	// to true) except via Reset which is meant to be called on the owning
	// thread when no presentation is in flight.
	std::atomic<bool> bCancelled{false};
	std::atomic<bool> bHurryUp{false};

	// Callback list. Stored as TFunction<> rather than a multicast delegate
	// so that std::function-style captures work without needing UObject
	// targets. Re-entry safe: invocation copies under the lock.
	mutable FCriticalSection CallbackLock;
	TMap<FDelegateHandle, TFunction<void()>> CancelCallbacks;
	TMap<FDelegateHandle, TFunction<void()>> HurryUpCallbacks;

	// Linked-source bookkeeping. When this source is built via
	// CreateLinkedTokenSource we register callbacks on each parent; we keep
	// their handles so we can detach cleanly when destroyed.
	struct FLinkedSubscription
	{
		TWeakObjectPtr<UYarnCancellationTokenSource> Parent;
		FDelegateHandle Handle;
	};
	TArray<FLinkedSubscription> ParentCancelSubscriptions;
	TArray<FLinkedSubscription> ParentHurryUpSubscriptions;
};

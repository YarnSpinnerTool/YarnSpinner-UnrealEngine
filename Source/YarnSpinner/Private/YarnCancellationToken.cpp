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

#include "YarnCancellationToken.h"

#include "Misc/ScopeLock.h"
#include "UObject/UObjectGlobals.h" // GetTransientPackage()
#include "UObject/Package.h"        // full UPackage type (so UPackage* -> UObject* converts)

// ============================================================================
// FYarnLineCancellationToken
// ============================================================================

bool FYarnLineCancellationToken::IsCancellationRequested() const
{
	if (UYarnCancellationTokenSource* Src = Source.Get())
	{
		return Src->IsCancellationRequested();
	}
	// A default-constructed token (no source) is never cancelled. Matches
	// .NET CancellationToken.None behaviour.
	return false;
}

bool FYarnLineCancellationToken::IsHurryUpRequested() const
{
	if (UYarnCancellationTokenSource* Src = Source.Get())
	{
		return Src->IsHurryUpRequested();
	}
	return false;
}

bool FYarnLineCancellationToken::IsAnyCancellationRequested() const
{
	return IsCancellationRequested() || IsHurryUpRequested();
}

// These three are deprecated forwarders (tokens are observers only now).
// Defining a deprecated member triggers the deprecation warning at the
// definition itself, so scope the suppression to just this block.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

void FYarnLineCancellationToken::MarkNextContentRequested()
{
	// Forwarder so the handful of legacy call sites don't silently become
	// no-ops while they migrate to calling Cancel() on the source.
	if (UYarnCancellationTokenSource* Src = Source.Get())
	{
		Src->Cancel();
	}
}

void FYarnLineCancellationToken::MarkHurryUpRequested()
{
	if (UYarnCancellationTokenSource* Src = Source.Get())
	{
		Src->RequestHurryUp();
	}
}

void FYarnLineCancellationToken::Reset()
{
	if (UYarnCancellationTokenSource* Src = Source.Get())
	{
		Src->Reset();
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

// ============================================================================
// UYarnCancellationTokenSource
// ============================================================================

UYarnCancellationTokenSource::UYarnCancellationTokenSource() = default;

void UYarnCancellationTokenSource::UnlinkFromParents()
{
	// Detach from anything we were linked to. Without this, a parent keeps a
	// callback referencing us (via a weak ptr) until it's next walked or GC'd.
	for (const FLinkedSubscription& Sub : ParentCancelSubscriptions)
	{
		if (UYarnCancellationTokenSource* Parent = Sub.Parent.Get())
		{
			Parent->UnregisterOnCancelled(Sub.Handle);
		}
	}
	for (const FLinkedSubscription& Sub : ParentHurryUpSubscriptions)
	{
		if (UYarnCancellationTokenSource* Parent = Sub.Parent.Get())
		{
			Parent->UnregisterOnHurryUp(Sub.Handle);
		}
	}
	ParentCancelSubscriptions.Reset();
	ParentHurryUpSubscriptions.Reset();
}

void UYarnCancellationTokenSource::BeginDestroy()
{
	// Belt-and-braces: if the owner didn't already retire us via
	// UnlinkFromParents, do it now so a parent doesn't fire into a
	// half-destroyed object during GC.
	UnlinkFromParents();

	Super::BeginDestroy();
}

FYarnLineCancellationToken UYarnCancellationTokenSource::GetToken()
{
	FYarnLineCancellationToken Tok;
	Tok.Source = this;
	return Tok;
}

bool UYarnCancellationTokenSource::IsCancellationRequested() const
{
	return bCancelled.load(std::memory_order_acquire);
}

bool UYarnCancellationTokenSource::IsHurryUpRequested() const
{
	// Cancellation implies hurry-up. The reverse isn't true: you can ask
	// the line to speed up without ending it.
	return bHurryUp.load(std::memory_order_acquire) ||
	       bCancelled.load(std::memory_order_acquire);
}

void UYarnCancellationTokenSource::Cancel()
{
	// Writes are one-way (false to true). exchange returns the old value;
	// only fire callbacks on the first transition.
	const bool bWasCancelled = bCancelled.exchange(true, std::memory_order_acq_rel);
	if (bWasCancelled)
	{
		return;
	}

	// Copy callbacks under the lock, then release before invoking. This
	// lets a callback re-enter the source safely (e.g. a linked child
	// cancelling itself, which would otherwise deadlock on the lock).
	TArray<TFunction<void()>> ToFire;
	{
		FScopeLock Lock(&CallbackLock);
		ToFire.Reserve(CancelCallbacks.Num());
		for (const TPair<FDelegateHandle, TFunction<void()>>& Pair : CancelCallbacks)
		{
			ToFire.Add(Pair.Value);
		}
	}
	for (const TFunction<void()>& Cb : ToFire)
	{
		Cb();
	}
}

void UYarnCancellationTokenSource::RequestHurryUp()
{
	const bool bWasSet = bHurryUp.exchange(true, std::memory_order_acq_rel);
	if (bWasSet)
	{
		return;
	}

	TArray<TFunction<void()>> ToFire;
	{
		FScopeLock Lock(&CallbackLock);
		ToFire.Reserve(HurryUpCallbacks.Num());
		for (const TPair<FDelegateHandle, TFunction<void()>>& Pair : HurryUpCallbacks)
		{
			ToFire.Add(Pair.Value);
		}
	}
	for (const TFunction<void()>& Cb : ToFire)
	{
		Cb();
	}
}

void UYarnCancellationTokenSource::Reset()
{
	bCancelled.store(false, std::memory_order_release);
	bHurryUp.store(false, std::memory_order_release);

	// Reset is meant for reusing a source for a fresh line. Drop the
	// callbacks so the previous line's listeners don't see signals from
	// the new one.
	FScopeLock Lock(&CallbackLock);
	CancelCallbacks.Reset();
	HurryUpCallbacks.Reset();
}

FDelegateHandle UYarnCancellationTokenSource::RegisterOnCancelled(TFunction<void()> Callback)
{
	// Matches .NET CancellationToken.Register: registering against an
	// already-cancelled source fires the callback synchronously and
	// returns no handle.
	if (bCancelled.load(std::memory_order_acquire))
	{
		Callback();
		return FDelegateHandle();
	}

	FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	{
		FScopeLock Lock(&CallbackLock);
		CancelCallbacks.Add(Handle, MoveTemp(Callback));
	}

	// Race: cancellation could have happened between the check above and
	// the Add. Re-check; if so, pull our callback back out and fire it so
	// the contract holds.
	if (bCancelled.load(std::memory_order_acquire))
	{
		TFunction<void()> Stored;
		{
			FScopeLock Lock(&CallbackLock);
			if (TFunction<void()>* Found = CancelCallbacks.Find(Handle))
			{
				Stored = MoveTemp(*Found);
				CancelCallbacks.Remove(Handle);
			}
		}
		if (Stored)
		{
			Stored();
		}
		return FDelegateHandle();
	}

	return Handle;
}

void UYarnCancellationTokenSource::UnregisterOnCancelled(FDelegateHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}
	FScopeLock Lock(&CallbackLock);
	CancelCallbacks.Remove(Handle);
}

FDelegateHandle UYarnCancellationTokenSource::RegisterOnHurryUp(TFunction<void()> Callback)
{
	// Cancellation implies hurry-up, so an already-cancelled source counts
	// as already-hurried for the purposes of this contract.
	if (bHurryUp.load(std::memory_order_acquire) ||
	    bCancelled.load(std::memory_order_acquire))
	{
		Callback();
		return FDelegateHandle();
	}

	FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	{
		FScopeLock Lock(&CallbackLock);
		HurryUpCallbacks.Add(Handle, MoveTemp(Callback));
	}

	if (bHurryUp.load(std::memory_order_acquire) ||
	    bCancelled.load(std::memory_order_acquire))
	{
		TFunction<void()> Stored;
		{
			FScopeLock Lock(&CallbackLock);
			if (TFunction<void()>* Found = HurryUpCallbacks.Find(Handle))
			{
				Stored = MoveTemp(*Found);
				HurryUpCallbacks.Remove(Handle);
			}
		}
		if (Stored)
		{
			Stored();
		}
		return FDelegateHandle();
	}

	return Handle;
}

void UYarnCancellationTokenSource::UnregisterOnHurryUp(FDelegateHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}
	FScopeLock Lock(&CallbackLock);
	HurryUpCallbacks.Remove(Handle);
}

UYarnCancellationTokenSource* UYarnCancellationTokenSource::CreateLinkedTokenSource(
	UObject* Outer,
	const TArray<FYarnLineCancellationToken>& LinkedTokens,
	bool bLinkCancellation,
	bool bLinkHurryUp)
{
	// Fall back to the transient package if the caller didn't give us
	// somewhere to put this. Linked sources are typically short-lived and
	// owned by whichever component made them.
	if (!Outer)
	{
		Outer = GetTransientPackage();
	}

	UYarnCancellationTokenSource* Linked = NewObject<UYarnCancellationTokenSource>(Outer);

	// Weak ref so a parent firing into a dead child does nothing.
	TWeakObjectPtr<UYarnCancellationTokenSource> WeakLinked(Linked);

	for (const FYarnLineCancellationToken& Tok : LinkedTokens)
	{
		UYarnCancellationTokenSource* Parent = Tok.Source.Get();
		if (!Parent)
		{
			// Token's source has gone away. Per .NET, an unsourced token is
			// permanently uncancelled, so it contributes nothing.
			continue;
		}

		if (bLinkCancellation)
		{
			FDelegateHandle H = Parent->RegisterOnCancelled([WeakLinked]()
			{
				if (UYarnCancellationTokenSource* L = WeakLinked.Get())
				{
					L->Cancel();
				}
			});
			// An invalid handle means the parent was already cancelled and
			// the callback fired synchronously inside Register. Nothing to
			// track in that case.
			if (H.IsValid())
			{
				FLinkedSubscription Sub;
				Sub.Parent = Parent;
				Sub.Handle = H;
				Linked->ParentCancelSubscriptions.Add(Sub);
			}
		}

		if (bLinkHurryUp)
		{
			FDelegateHandle H = Parent->RegisterOnHurryUp([WeakLinked]()
			{
				if (UYarnCancellationTokenSource* L = WeakLinked.Get())
				{
					L->RequestHurryUp();
				}
			});
			if (H.IsValid())
			{
				FLinkedSubscription Sub;
				Sub.Parent = Parent;
				Sub.Handle = H;
				Linked->ParentHurryUpSubscriptions.Add(Sub);
			}
		}
	}

	return Linked;
}

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

UYarnCancellationTokenSource::UYarnCancellationTokenSource()
{
	CurrentToken.Reset();
}

FYarnLineCancellationToken& UYarnCancellationTokenSource::GetToken()
{
	CurrentToken.Reset();
	return CurrentToken;
}

void UYarnCancellationTokenSource::RequestHurryUp()
{
	CurrentToken.MarkHurryUpRequested();
}

void UYarnCancellationTokenSource::RequestNextContent()
{
	CurrentToken.MarkNextContentRequested();
}

void UYarnCancellationTokenSource::CancelAll()
{
	CurrentToken.MarkNextContentRequested();
}

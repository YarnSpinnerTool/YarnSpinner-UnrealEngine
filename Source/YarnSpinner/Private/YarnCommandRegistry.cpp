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

#include "YarnCommandRegistry.h"

UYarnCommandRegistrySettings::UYarnCommandRegistrySettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Yarn Spinner Command Registry");
}

void UYarnCommandRegistrySettings::GetEntriesForClass(const UClass* Class, TArray<FYarnBakedActionEntry>& OutEntries) const
{
	if (!Class)
	{
		return;
	}

	for (const FYarnBakedActionEntry& Entry : BakedActions)
	{
		const UClass* EntryClass = Entry.OwningClass.ResolveClass();
		if (EntryClass && Class->IsChildOf(EntryClass))
		{
			OutEntries.Add(Entry);
		}
	}
}

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

#include "YarnSaliency.h"
#include "YarnVariableStorage.h"
#include "YarnDialogueRunner.h"

// first strategy - just return first viable candidate
bool UYarnFirstSaliencyStrategy::QueryBestContent_Implementation(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate)
{
	for (const FYarnSaliencyCandidate& Candidate : Candidates)
	{
		if (Candidate.IsViable())
		{
			OutSelectedCandidate = Candidate;
			return true;
		}
	}
	return false;
}

void UYarnFirstSaliencyStrategy::ContentWasSelected_Implementation(const FYarnSaliencyCandidate& SelectedCandidate)
{
	// no state tracking needed
}

// best strategy - return highest complexity viable candidate
bool UYarnBestSaliencyStrategy::QueryBestContent_Implementation(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate)
{
	const FYarnSaliencyCandidate* Best = nullptr;

	for (const FYarnSaliencyCandidate& Candidate : Candidates)
	{
		if (!Candidate.IsViable())
		{
			continue;
		}

		if (Best == nullptr || Candidate.ComplexityScore > Best->ComplexityScore)
		{
			Best = &Candidate;
		}
	}

	if (Best != nullptr)
	{
		OutSelectedCandidate = *Best;
		return true;
	}

	return false;
}

void UYarnBestSaliencyStrategy::ContentWasSelected_Implementation(const FYarnSaliencyCandidate& SelectedCandidate)
{
	// no state tracking needed
}

// best least recently viewed strategy
void UYarnBestLeastRecentlyViewedSaliencyStrategy::Initialise(TScriptInterface<IYarnVariableStorage> InVariableStorage)
{
	VariableStorage = InVariableStorage;
}

int32 UYarnBestLeastRecentlyViewedSaliencyStrategy::GetViewCount(const FYarnSaliencyCandidate& Candidate) const
{
	if (!VariableStorage.GetInterface())
	{
		return 0;
	}

	FYarnValue Value;
	if (IYarnVariableStorage::Execute_TryGetValue(VariableStorage.GetObject(), Candidate.GetViewCountKey(), Value))
	{
		return FMath::RoundToInt(Value.ConvertToNumber());
	}

	return 0;
}

void UYarnBestLeastRecentlyViewedSaliencyStrategy::IncrementViewCount(const FYarnSaliencyCandidate& Candidate)
{
	if (!VariableStorage.GetInterface())
	{
		return;
	}

	int32 CurrentCount = GetViewCount(Candidate);
	IYarnVariableStorage::Execute_SetValue(
		VariableStorage.GetObject(),
		Candidate.GetViewCountKey(),
		FYarnValue(static_cast<float>(CurrentCount + 1))
	);
}

bool UYarnBestLeastRecentlyViewedSaliencyStrategy::QueryBestContent_Implementation(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate)
{
	// filter to viable candidates and get view counts
	struct FCandidateWithViewCount
	{
		FYarnSaliencyCandidate Candidate;
		int32 ViewCount;
	};

	TArray<FCandidateWithViewCount> ViableCandidates;
	for (const FYarnSaliencyCandidate& Candidate : Candidates)
	{
		if (Candidate.IsViable())
		{
			FCandidateWithViewCount Entry;
			Entry.Candidate = Candidate;
			Entry.ViewCount = GetViewCount(Candidate);
			ViableCandidates.Add(Entry);
		}
	}

	if (ViableCandidates.Num() == 0)
	{
		return false;
	}

	// Sort by view count (ascending), then by complexity (descending).
	// StableSort preserves original ordering among equal elements.
	ViableCandidates.StableSort([](const FCandidateWithViewCount& A, const FCandidateWithViewCount& B)
	{
		if (A.ViewCount != B.ViewCount)
		{
			return A.ViewCount < B.ViewCount; // least viewed first
		}
		return A.Candidate.ComplexityScore > B.Candidate.ComplexityScore; // highest complexity first
	});

	OutSelectedCandidate = ViableCandidates[0].Candidate;
	return true;
}

void UYarnBestLeastRecentlyViewedSaliencyStrategy::ContentWasSelected_Implementation(const FYarnSaliencyCandidate& SelectedCandidate)
{
	IncrementViewCount(SelectedCandidate);
}

// random best least recently viewed strategy
void UYarnRandomBestLeastRecentlyViewedSaliencyStrategy::Initialise(TScriptInterface<IYarnVariableStorage> InVariableStorage)
{
	VariableStorage = InVariableStorage;
}

int32 UYarnRandomBestLeastRecentlyViewedSaliencyStrategy::GetViewCount(const FYarnSaliencyCandidate& Candidate) const
{
	if (!VariableStorage.GetInterface())
	{
		return 0;
	}

	FYarnValue Value;
	if (IYarnVariableStorage::Execute_TryGetValue(VariableStorage.GetObject(), Candidate.GetViewCountKey(), Value))
	{
		return FMath::RoundToInt(Value.ConvertToNumber());
	}

	return 0;
}

void UYarnRandomBestLeastRecentlyViewedSaliencyStrategy::IncrementViewCount(const FYarnSaliencyCandidate& Candidate)
{
	if (!VariableStorage.GetInterface())
	{
		return;
	}

	int32 CurrentCount = GetViewCount(Candidate);
	IYarnVariableStorage::Execute_SetValue(
		VariableStorage.GetObject(),
		Candidate.GetViewCountKey(),
		FYarnValue(static_cast<float>(CurrentCount + 1))
	);
}

bool UYarnRandomBestLeastRecentlyViewedSaliencyStrategy::QueryBestContent_Implementation(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate)
{
	// filter to viable candidates and get view counts
	struct FCandidateWithViewCount
	{
		FYarnSaliencyCandidate Candidate;
		int32 ViewCount;
	};

	TArray<FCandidateWithViewCount> ViableCandidates;
	for (const FYarnSaliencyCandidate& Candidate : Candidates)
	{
		if (Candidate.IsViable())
		{
			FCandidateWithViewCount Entry;
			Entry.Candidate = Candidate;
			Entry.ViewCount = GetViewCount(Candidate);
			ViableCandidates.Add(Entry);
		}
	}

	if (ViableCandidates.Num() == 0)
	{
		return false;
	}

	// find minimum view count
	int32 MinViewCount = ViableCandidates[0].ViewCount;
	for (const FCandidateWithViewCount& Entry : ViableCandidates)
	{
		if (Entry.ViewCount < MinViewCount)
		{
			MinViewCount = Entry.ViewCount;
		}
	}

	// filter to only those with minimum view count
	TArray<FCandidateWithViewCount> LeastViewedCandidates;
	for (const FCandidateWithViewCount& Entry : ViableCandidates)
	{
		if (Entry.ViewCount == MinViewCount)
		{
			LeastViewedCandidates.Add(Entry);
		}
	}

	// find maximum complexity among least viewed
	int32 MaxComplexity = LeastViewedCandidates[0].Candidate.ComplexityScore;
	for (const FCandidateWithViewCount& Entry : LeastViewedCandidates)
	{
		if (Entry.Candidate.ComplexityScore > MaxComplexity)
		{
			MaxComplexity = Entry.Candidate.ComplexityScore;
		}
	}

	// filter to only those with maximum complexity
	TArray<FCandidateWithViewCount> BestCandidates;
	for (const FCandidateWithViewCount& Entry : LeastViewedCandidates)
	{
		if (Entry.Candidate.ComplexityScore == MaxComplexity)
		{
			BestCandidates.Add(Entry);
		}
	}

	// randomly select from the best candidates
	int32 RandomIndex = FMath::RandRange(0, BestCandidates.Num() - 1);
	OutSelectedCandidate = BestCandidates[RandomIndex].Candidate;
	return true;
}

void UYarnRandomBestLeastRecentlyViewedSaliencyStrategy::ContentWasSelected_Implementation(const FYarnSaliencyCandidate& SelectedCandidate)
{
	IncrementViewCount(SelectedCandidate);
}

// strategy factory
TScriptInterface<IYarnSaliencyStrategy> UYarnSaliencyStrategyFactory::CreateStrategy(
	EYarnSaliencyStrategy Strategy,
	TScriptInterface<IYarnVariableStorage> VariableStorage,
	UObject* Outer)
{
	switch (Strategy)
	{
	case EYarnSaliencyStrategy::First:
		return NewObject<UYarnFirstSaliencyStrategy>(Outer);

	case EYarnSaliencyStrategy::Best:
		return NewObject<UYarnBestSaliencyStrategy>(Outer);

	case EYarnSaliencyStrategy::BestLeastRecentlyViewed:
		{
			UYarnBestLeastRecentlyViewedSaliencyStrategy* Strat = NewObject<UYarnBestLeastRecentlyViewedSaliencyStrategy>(Outer);
			Strat->Initialise(VariableStorage);
			return Strat;
		}

	case EYarnSaliencyStrategy::RandomBestLeastRecentlyViewed:
	default:
		{
			UYarnRandomBestLeastRecentlyViewedSaliencyStrategy* Strat = NewObject<UYarnRandomBestLeastRecentlyViewedSaliencyStrategy>(Outer);
			Strat->Initialise(VariableStorage);
			return Strat;
		}
	}
}

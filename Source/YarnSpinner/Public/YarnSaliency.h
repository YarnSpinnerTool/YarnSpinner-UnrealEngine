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
// YarnSaliency.h
// ============================================================================
//
// The saliency system allows Yarn to intelligently select content from multiple
// candidates. This is used by node groups and line groups where you have several
// options and want Yarn to pick the "best" one based on conditions and history.
//
// For example, if you have multiple versions of a greeting (casual, formal, etc),
// saliency can pick the one that:
//   - matches the current context (conditions passed)
//   - hasn't been seen recently (for variety)
//   - has the highest priority/complexity score
//
// Blueprint users:
//   - Select a built-in strategy on the dialogue runner (SaliencyStrategy enum)
//   - Or create a Blueprint class implementing IYarnSaliencyStrategy for custom logic
//
// C++ users:
//   - Can implement IYarnSaliencyStrategy for custom selection logic
//   - Can use the factory to create built-in strategies

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "YarnVariableStorage.h"
#include "YarnSaliency.generated.h"

// ============================================================================
// EYarnSaliencyStrategy
// ============================================================================

/**
 * The type of saliency strategy to use for content selection.
 *
 * Set this on the dialogue runner to control how Yarn picks from multiple
 * content candidates (in node groups, line groups, etc).
 */
UENUM(BlueprintType)
enum class EYarnSaliencyStrategy : uint8
{
	/** Pick the first viable candidate (simplest, deterministic) */
	First UMETA(DisplayName = "First"),

	/** Pick the best (highest complexity score) viable candidate */
	Best UMETA(DisplayName = "Best"),

	/** Pick the best candidate that hasn't been seen recently (balances quality and variety) */
	BestLeastRecentlyViewed UMETA(DisplayName = "Best Least Recently Viewed"),

	/** Randomly pick from the best candidates not seen recently (recommended) */
	RandomBestLeastRecentlyViewed UMETA(DisplayName = "Random Best Least Recently Viewed")
};

// ============================================================================
// EYarnSaliencyContentType
// ============================================================================

/**
 * The type of content being considered for saliency selection.
 */
UENUM(BlueprintType)
enum class EYarnSaliencyContentType : uint8
{
	/** A single line of dialogue */
	Line UMETA(DisplayName = "Line"),

	/** A dialogue node */
	Node UMETA(DisplayName = "Node")
};

// ============================================================================
// FYarnSaliencyCandidate
// ============================================================================
//
// when yarn encounters a content group (node group, line group, etc), it
// creates a candidate for each option. the saliency strategy then examines
// all candidates and picks the best one.

/**
 * A candidate for saliency-based content selection.
 *
 * Contains information about one possible content choice, including how
 * many conditions passed/failed and its priority score.
 *
 * You typically don't create these yourself - they're created internally
 * by the VM when processing content groups.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnSaliencyCandidate
{
	GENERATED_BODY()

	/**
	 * Unique identifier for this content.
	 * For nodes, this is the node name. For lines, this is the line ID.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Saliency")
	FString ContentID;

	/**
	 * How many "when" conditions passed for this candidate.
	 * More passing conditions = better match for the current context.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Saliency")
	int32 PassingConditionCount = 0;

	/**
	 * How many "when" conditions failed for this candidate.
	 * If any fail, the candidate is not viable (IsViable() returns false).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Saliency")
	int32 FailingConditionCount = 0;

	/**
	 * The complexity/priority score (higher = better quality content).
	 * Typically based on how specific/detailed the content is.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Saliency")
	int32 ComplexityScore = 0;

	/** whether this is a line or a node */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yarn Spinner|Saliency")
	EYarnSaliencyContentType ContentType = EYarnSaliencyContentType::Line;

	/** the jump destination in the vm if this candidate is selected (internal) */
	int32 Destination = INDEX_NONE;

	/** whether this candidate's condition passed (for simple true/false conditions) */
	bool bConditionPassed = false;

	/** default constructor */
	FYarnSaliencyCandidate() = default;

	/** construct with a content id */
	explicit FYarnSaliencyCandidate(const FString& InContentID)
		: ContentID(InContentID)
	{
	}

	/**
	 * Get the variable key used to track view count for this content.
	 * Format: $Yarn.Internal.Content.ViewCount.{ContentID}
	 * Used by strategies that track history.
	 */
	FString GetViewCountKey() const
	{
		return FString::Printf(TEXT("$Yarn.Internal.Content.ViewCount.%s"), *ContentID);
	}

	/** check if this candidate is viable (no failing conditions) */
	bool IsViable() const { return FailingConditionCount == 0; }
};

// ============================================================================
// IYarnSaliencyStrategy
// ============================================================================
//
// interface for content selection strategies. implement this to create custom
// selection logic for content groups.
//
// the built-in strategies should cover most use cases - only implement this
// if you need very specific selection behaviour.
//
// Blueprint users can create a Blueprint class that implements this interface
// and override QueryBestContent and ContentWasSelected.

/**
 * Interface for content saliency strategies.
 *
 * Saliency strategies determine how to select the best content from
 * multiple candidates. Different strategies provide different behaviours:
 *   - First: simply picks the first viable candidate (deterministic)
 *   - Best: picks the highest complexity score (quality-focused)
 *   - BestLeastRecentlyViewed: balances quality and variety
 *   - RandomBestLeastRecentlyViewed: adds randomness (recommended)
 *
 * To create a custom strategy in Blueprint, create a new Blueprint class
 * that implements this interface and override the two methods.
 */
UINTERFACE(Blueprintable, BlueprintType)
class UYarnSaliencyStrategy : public UInterface
{
	GENERATED_BODY()
};

class YARNSPINNER_API IYarnSaliencyStrategy
{
	GENERATED_BODY()

public:
	/**
	 * Query for the best content from the given candidates.
	 *
	 * This is a read-only operation that should not modify any state.
	 * Call ContentWasSelected after the selection is confirmed.
	 *
	 * @param Candidates The available content candidates.
	 * @param OutSelectedCandidate The selected candidate (if any).
	 * @return True if a candidate was selected, false if none were viable.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Saliency")
	bool QueryBestContent(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate);

	/**
	 * Notify the strategy that content was selected.
	 *
	 * Called after QueryBestContent when the selection is confirmed.
	 * Strategies that track view history should update their state here
	 * (e.g., increment view count in variable storage).
	 *
	 * @param SelectedCandidate The candidate that was selected.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Saliency")
	void ContentWasSelected(const FYarnSaliencyCandidate& SelectedCandidate);
};

// ============================================================================
// built-in strategy implementations
// ============================================================================
//
// these are the four strategies you can select via the EYarnSaliencyStrategy enum.
// they're created automatically by the dialogue runner based on your selection.

/**
 * Saliency strategy that picks the first viable candidate.
 *
 * The simplest strategy - just returns the first candidate that has no
 * failing conditions. Deterministic and fast, but provides no variety.
 * Good for testing or when content order in the yarn file should determine selection.
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnFirstSaliencyStrategy : public UObject, public IYarnSaliencyStrategy
{
	GENERATED_BODY()

public:
	virtual bool QueryBestContent_Implementation(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate) override;
	virtual void ContentWasSelected_Implementation(const FYarnSaliencyCandidate& SelectedCandidate) override;
};

/**
 * Saliency strategy that picks the highest quality viable candidate.
 *
 * Selects the candidate with the highest complexity score from those
 * with no failing conditions. Deterministic, always picks "best" content.
 * Good when you want consistent quality but don't need variety.
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnBestSaliencyStrategy : public UObject, public IYarnSaliencyStrategy
{
	GENERATED_BODY()

public:
	virtual bool QueryBestContent_Implementation(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate) override;
	virtual void ContentWasSelected_Implementation(const FYarnSaliencyCandidate& SelectedCandidate) override;
};

/**
 * Saliency strategy that balances quality with view history.
 *
 * Prefers content that hasn't been seen recently, breaking ties by
 * complexity score. Tracks view counts in variable storage.
 * Good for making sure players see all your content over time.
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnBestLeastRecentlyViewedSaliencyStrategy : public UObject, public IYarnSaliencyStrategy
{
	GENERATED_BODY()

public:
	/** the variable storage used to track view counts */
	UPROPERTY()
	TScriptInterface<IYarnVariableStorage> VariableStorage;

	/** initialise with a variable storage (called by the factory) */
	void Initialise(TScriptInterface<IYarnVariableStorage> InVariableStorage);

	virtual bool QueryBestContent_Implementation(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate) override;
	virtual void ContentWasSelected_Implementation(const FYarnSaliencyCandidate& SelectedCandidate) override;

private:
	/** get the view count for a candidate from variable storage */
	int32 GetViewCount(const FYarnSaliencyCandidate& Candidate) const;

	/** increment the view count for a candidate */
	void IncrementViewCount(const FYarnSaliencyCandidate& Candidate);
};

/**
 * Saliency strategy that adds randomness to variety-based selection.
 *
 * Like BestLeastRecentlyViewed, but randomly selects from candidates
 * that tie for "best" position. Provides variety without repetition.
 *
 * This is the recommended strategy for most games - it provides variety
 * while still respecting content priority.
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnRandomBestLeastRecentlyViewedSaliencyStrategy : public UObject, public IYarnSaliencyStrategy
{
	GENERATED_BODY()

public:
	/** the variable storage used to track view counts */
	UPROPERTY()
	TScriptInterface<IYarnVariableStorage> VariableStorage;

	/** initialise with a variable storage (called by the factory) */
	void Initialise(TScriptInterface<IYarnVariableStorage> InVariableStorage);

	virtual bool QueryBestContent_Implementation(const TArray<FYarnSaliencyCandidate>& Candidates, FYarnSaliencyCandidate& OutSelectedCandidate) override;
	virtual void ContentWasSelected_Implementation(const FYarnSaliencyCandidate& SelectedCandidate) override;

private:
	/** get the view count for a candidate from variable storage */
	int32 GetViewCount(const FYarnSaliencyCandidate& Candidate) const;

	/** increment the view count for a candidate */
	void IncrementViewCount(const FYarnSaliencyCandidate& Candidate);
};

// ============================================================================
// UYarnSaliencyStrategyFactory
// ============================================================================
//
// internal factory used by the dialogue runner to create strategy instances.
// you typically don't need to use this directly.

/**
 * Factory for creating saliency strategies.
 * Used internally by the dialogue runner.
 */
UCLASS()
class YARNSPINNER_API UYarnSaliencyStrategyFactory : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create a saliency strategy based on the specified type.
	 *
	 * @param Strategy The strategy type to create.
	 * @param VariableStorage The variable storage for strategies that track history.
	 * @param Outer The outer object for the created strategy.
	 * @return The created strategy object.
	 */
	static TScriptInterface<IYarnSaliencyStrategy> CreateStrategy(
		EYarnSaliencyStrategy Strategy,
		TScriptInterface<IYarnVariableStorage> VariableStorage,
		UObject* Outer);
};

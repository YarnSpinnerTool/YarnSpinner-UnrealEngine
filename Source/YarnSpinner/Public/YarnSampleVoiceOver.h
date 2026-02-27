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

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarnDialogueRunner.h"
#include "YarnWidgetPresenter.h"
#include "YarnVoiceOverPresenter.h"
#include "Engine/DataTable.h"
#include "YarnSampleVoiceOver.generated.h"

class UYarnProject;

/**
 * Row structure for mapping line IDs to voice-over audio assets.
 * Use this in a DataTable to assign audio clips to dialogue lines.
 */
USTRUCT(BlueprintType)
struct FYarnVoiceOverEntry : public FTableRowBase
{
	GENERATED_BODY()

	/** The audio clip to play for this line */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over")
	USoundBase* AudioClip = nullptr;
};

/**
 * A ready-to-use Actor that demonstrates voice-over dialogue.
 *
 * This actor combines a DialogueRunner with both a WidgetPresenter (for text)
 * and a VoiceOverPresenter (for audio) to provide a complete voice-over
 * dialogue system out of the box.
 *
 * Voice-over audio is looked up from a DataTable using the line ID as the row name.
 *
 * Simply place this actor in your level, assign a YarnProject and a VoiceOver
 * DataTable, and call StartDialogue.
 *
 * @see AYarnDialogueActor for text-only dialogue
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API AYarnSampleVoiceOver : public AActor
{
	GENERATED_BODY()

public:
	AYarnSampleVoiceOver();

	virtual void BeginPlay() override;

	/**
	 * Start dialogue at the specified node.
	 * @param NodeName The name of the node to start at.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void StartDialogue(const FString& NodeName);

	/**
	 * Stop the current dialogue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void StopDialogue();

	/**
	 * Check if dialogue is currently running.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	bool IsDialogueRunning() const;

	// ========== Configuration ==========

	/** The Yarn Project asset to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	UYarnProject* YarnProject;

	/** The node to start dialogue at when using auto-start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FString StartNode = TEXT("Start");

	/** Whether to automatically start dialogue when the game begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	bool bAutoStart = false;

	/**
	 * DataTable mapping line IDs to voice-over audio clips.
	 * Each row name should match a line ID (e.g., "tutorial-tom-01"),
	 * and the row should contain an FYarnVoiceOverEntry with the audio clip.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over")
	UDataTable* VoiceOverTable;

	// ========== Text Appearance ==========

	/** Typewriter speed (characters per second). Set to 0 for instant text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Text Appearance")
	float TypewriterSpeed = 50.0f;

	/** Background color for the dialogue box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Text Appearance")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.9f);

	/** Text color for dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Text Appearance")
	FLinearColor TextColor = FLinearColor::White;

	/** Text color for character names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Text Appearance")
	FLinearColor CharacterNameColor = FLinearColor(0.8f, 0.6f, 0.2f, 1.0f);

	// ========== Voice Over Timing ==========

	/**
	 * Time in seconds to fade out audio when interrupted.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over|Timing", meta = (ClampMin = "0.0"))
	float FadeOutTimeOnInterrupt = 0.05f;

	/**
	 * Time in seconds to wait before starting audio playback.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over|Timing", meta = (ClampMin = "0.0"))
	float WaitTimeBeforeStart = 0.0f;

	/**
	 * Time in seconds to wait after audio completes before moving to next line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Over|Timing", meta = (ClampMin = "0.0"))
	float WaitTimeAfterComplete = 0.0f;

	// ========== Events ==========

	/** Called when dialogue starts. */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnDialogueStartBP OnDialogueStarted;

	/** Called when dialogue completes. */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnDialogueCompleteBP OnDialogueCompleted;

	// ========== Accessors ==========

	/** Get the dialogue runner component. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	UYarnDialogueRunner* GetDialogueRunner() const { return DialogueRunner; }

	/** Get the widget presenter component. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	UYarnWidgetPresenter* GetWidgetPresenter() const { return WidgetPresenter; }

	/** Get the voice-over presenter component. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	UYarnVoiceOverPresenter* GetVoiceOverPresenter() const { return VoiceOverPresenter; }

	/**
	 * Look up a voice-over audio clip for a line ID.
	 * @param LineID The line ID to look up.
	 * @return The audio clip, or nullptr if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voice Over")
	USoundBase* GetVoiceOverClipForLine(const FString& LineID) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	UYarnDialogueRunner* DialogueRunner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	UYarnWidgetPresenter* WidgetPresenter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	UYarnVoiceOverPresenter* VoiceOverPresenter;

	UFUNCTION()
	void HandleDialogueStarted();

	UFUNCTION()
	void HandleDialogueComplete();
};

/**
 * Custom voice-over presenter that uses a DataTable for audio lookup.
 * This is used internally by AYarnSampleVoiceOver.
 */
UCLASS()
class YARNSPINNER_API UYarnDataTableVoiceOverPresenter : public UYarnVoiceOverPresenter
{
	GENERATED_BODY()

public:
	/** The owning sample actor (for DataTable access) */
	UPROPERTY()
	AYarnSampleVoiceOver* OwnerActor;

	virtual USoundBase* GetVoiceOverClip_Implementation(const FYarnLocalizedLine& Line) override;
};

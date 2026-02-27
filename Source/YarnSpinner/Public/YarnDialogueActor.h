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
#include "YarnDialogueActor.generated.h"

class UYarnProject;

/**
 * A ready-to-use Actor that sets up a complete dialogue system.
 *
 * This actor combines a DialogueRunner with a WidgetPresenter to provide
 * a complete, working dialogue system out of the box. Simply place it in
 * your level, assign a YarnProject, and call StartDialogue.
 *
 * For more control, you can create your own setup using the individual
 * components (UYarnDialogueRunner, UYarnWidgetPresenter, etc).
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API AYarnDialogueActor : public AActor
{
	GENERATED_BODY()

public:
	AYarnDialogueActor();

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

	/** The Yarn Project asset to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	UYarnProject* YarnProject;

	/** The node to start dialogue at when using auto-start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FString StartNode = TEXT("Start");

	/** Whether to automatically start dialogue when the game begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	bool bAutoStart = false;

	/** Typewriter speed (characters per second). Set to 0 for instant text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Appearance")
	float TypewriterSpeed = 50.0f;

	/** Background color for the dialogue box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Appearance")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.9f);

	/** Text color for dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Appearance")
	FLinearColor TextColor = FLinearColor::White;

	/** Text color for character names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Appearance")
	FLinearColor CharacterNameColor = FLinearColor(0.8f, 0.6f, 0.2f, 1.0f);

	/** Called when dialogue starts. */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnDialogueStartBP OnDialogueStarted;

	/** Called when dialogue completes. */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Events")
	FOnYarnDialogueCompleteBP OnDialogueCompleted;

	/** Get the dialogue runner component. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	UYarnDialogueRunner* GetDialogueRunner() const { return DialogueRunner; }

	/** Get the widget presenter component. */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	UYarnWidgetPresenter* GetWidgetPresenter() const { return WidgetPresenter; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	UYarnDialogueRunner* DialogueRunner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Yarn Spinner")
	UYarnWidgetPresenter* WidgetPresenter;

	UFUNCTION()
	void HandleDialogueStarted();

	UFUNCTION()
	void HandleDialogueComplete();
};

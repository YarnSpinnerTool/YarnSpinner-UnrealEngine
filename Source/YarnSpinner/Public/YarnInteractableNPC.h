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
// YarnInteractableNPC.h
// ============================================================================
//
// WHAT THIS IS:
// an NPC actor that players can approach and interact with to trigger dialogue.
// each NPC has its own dialogue node and display name.
//
// HOW IT WORKS:
// 1. player enters the NPC's interaction radius (sphere collision)
// 2. a "Press E to Talk" prompt appears above the NPC
// 3. player presses the interact key
// 4. the NPC's dialogue node is started on the shared dialogue runner
// 5. dialogue plays through, then NPC returns to idle state
//
// BLUEPRINT VS C++ USAGE:
// blueprint users:
//   - place this actor in your level
//   - set DialogueNodeName to the yarn node this NPC should start
//   - set CharacterDisplayName for the name shown in dialogue UI
//   - optionally assign a custom skeletal mesh
//
// c++ users:
//   - can subclass for custom NPC behaviour
//   - override OnInteract() for custom interaction handling
//   - can spawn programmatically (see AYarnMultiNPCDemo for example)

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// unreal core - provides fundamental types
#include "CoreMinimal.h"

// AActor - base class for all actors in the world
#include "GameFramework/Actor.h"

// required by unreal's reflection system
#include "YarnInteractableNPC.generated.h"

class USphereComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;
class UWidgetComponent;
class UTextRenderComponent;
class UYarnDialogueRunner;

// ============================================================================
// AYarnInteractableNPC
// ============================================================================

/**
 * an NPC that players can interact with to trigger yarn dialogue.
 *
 * features:
 * - skeletal mesh for visual representation
 * - sphere collision for interaction range detection
 * - floating interaction prompt ("Press E to Talk")
 * - configurable dialogue node and display name
 *
 * @see AYarnFirstPersonPlayer for the player that interacts with these NPCs
 * @see AYarnMultiNPCDemo for a complete demo setup
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API AYarnInteractableNPC : public AActor
{
	GENERATED_BODY()

public:
	AYarnInteractableNPC();

	virtual void BeginPlay() override;

	// ========================================================================
	// components - the visual and collision elements
	// ========================================================================

	/**
	 * the capsule that defines the NPC's body collision.
	 * this is what line traces hit when the player looks at the NPC.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCapsuleComponent* BodyCapsule;

	/**
	 * the skeletal mesh that displays the NPC character.
	 * defaults to the UE5 mannequin if available.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* MeshComponent;

	/**
	 * the sphere collision that detects when the player is in range.
	 * its radius is controlled by InteractionRadius.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* InteractionSphere;

	/**
	 * text that floats above the NPC showing their name and interaction prompt.
	 * visibility is toggled based on player proximity.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* InteractionPrompt;

	// ========================================================================
	// configuration - set these in the editor
	// ========================================================================

	/**
	 * the name of the yarn node to start when this NPC is interacted with.
	 * for example: "Guard" or "Merchant_Greeting"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FString DialogueNodeName = TEXT("Start");

	/**
	 * the display name shown in dialogue UI when this NPC speaks.
	 * for example: "Town Guard" or "Friendly Merchant"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FString CharacterDisplayName = TEXT("NPC");

	/**
	 * how close the player must be to interact with this NPC (in units).
	 * this sets the radius of the interaction sphere.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner", meta = (ClampMin = "50.0"))
	float InteractionRadius = 200.0f;

	/**
	 * whether this NPC can currently be interacted with.
	 * set to false to disable interaction (e.g., during cutscenes).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	bool bCanBeInteractedWith = true;

	/**
	 * the text shown in the interaction prompt when player is in range.
	 * default: "Press E to Talk"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FString InteractionPromptText = TEXT("Press E to Talk");

	// ========================================================================
	// state - runtime information
	// ========================================================================

	/**
	 * whether the player is currently within interaction range.
	 * updated automatically by sphere overlap events.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|State")
	bool bPlayerInRange = false;

	/**
	 * whether this NPC is currently in a conversation.
	 * set when dialogue starts, cleared when dialogue ends.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|State")
	bool bIsInConversation = false;

	// ========================================================================
	// interaction - called when player interacts
	// ========================================================================

	/**
	 * attempt to interact with this NPC.
	 * if conditions are met, starts the dialogue node.
	 *
	 * @param DialogueRunner the dialogue runner to use for the conversation
	 * @return true if interaction was successful and dialogue started
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	bool Interact(UYarnDialogueRunner* DialogueRunner);

	/**
	 * check if this NPC can be interacted with right now.
	 * considers bCanBeInteractedWith, bPlayerInRange, and bIsInConversation.
	 *
	 * @return true if the NPC can be interacted with
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Yarn Spinner")
	bool CanInteract() const;

	/**
	 * called when dialogue with this NPC ends.
	 * override in blueprints or subclasses for custom behaviour.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner")
	void OnDialogueEnded();
	virtual void OnDialogueEnded_Implementation();

	/**
	 * show or hide the interaction prompt.
	 *
	 * @param bShow true to show the prompt, false to hide
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void SetPromptVisible(bool bShow);

protected:
	// ========================================================================
	// overlap events
	// ========================================================================

	/**
	 * called when something enters the interaction sphere.
	 */
	UFUNCTION()
	void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/**
	 * called when something exits the interaction sphere.
	 */
	UFUNCTION()
	void OnInteractionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/**
	 * internal handler for dialogue completion event.
	 */
	UFUNCTION()
	void HandleDialogueComplete();

	/** the dialogue runner we started dialogue on (for unbinding events) */
	UPROPERTY()
	UYarnDialogueRunner* ActiveDialogueRunner;
};

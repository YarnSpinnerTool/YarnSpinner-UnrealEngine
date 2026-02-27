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
// YarnMultiNPCDemo.h
// ============================================================================
//
// WHAT THIS IS:
// a self-contained demo actor that spawns a complete interactive dialogue scene.
// just drop this actor into any empty level and hit Play - it creates a room
// with multiple NPCs that the player can walk around and talk to.
//
// HOW IT WORKS:
// 1. on BeginPlay, spawns a floor, walls, and basic lighting
// 2. spawns a first-person player pawn and possesses it
// 3. spawns multiple NPCs at configured positions
// 4. player can walk around and interact with NPCs to trigger dialogue
//
// BLUEPRINT VS C++ USAGE:
// blueprint users:
//   - place this actor in an empty level
//   - assign the YarnProject containing NPC dialogue
//   - optionally customize NPCConfigs to add/remove/reposition NPCs
//   - hit Play!
//
// c++ users:
//   - can subclass for custom demo setups
//   - can modify NPCConfigs to spawn different characters
//   - can override SpawnRoom/SpawnNPCs for custom geometry

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// unreal core - provides fundamental types
#include "CoreMinimal.h"

// AActor - base class for all actors in the world
#include "GameFramework/Actor.h"

// required by unreal's reflection system
#include "YarnMultiNPCDemo.generated.h"

class UYarnProject;
class AYarnFirstPersonPlayer;
class AYarnInteractableNPC;

// ============================================================================
// FYarnNPCConfig
// ============================================================================

/**
 * configuration for an NPC to spawn in the demo.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnNPCConfig
{
	GENERATED_BODY()

	/** the name of the yarn node this NPC starts when interacted with */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Config")
	FString NodeName;

	/** the display name shown in dialogue UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Config")
	FString DisplayName;

	/** where to spawn this NPC in the room */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Config")
	FVector SpawnLocation;

	/** which direction the NPC faces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Config")
	FRotator SpawnRotation;

	FYarnNPCConfig()
		: NodeName(TEXT("Start"))
		, DisplayName(TEXT("NPC"))
		, SpawnLocation(FVector::ZeroVector)
		, SpawnRotation(FRotator::ZeroRotator)
	{
	}

	FYarnNPCConfig(const FString& InNodeName, const FString& InDisplayName, const FVector& InLocation, const FRotator& InRotation)
		: NodeName(InNodeName)
		, DisplayName(InDisplayName)
		, SpawnLocation(InLocation)
		, SpawnRotation(InRotation)
	{
	}
};

// ============================================================================
// AYarnMultiNPCDemo
// ============================================================================

/**
 * a complete, self-contained demo of multi-NPC dialogue.
 *
 * this actor spawns everything needed for a working dialogue demo:
 * - a room with floor and walls
 * - a first-person player pawn
 * - multiple NPCs with unique dialogue
 * - basic lighting
 *
 * just drop this in an empty level, assign a YarnProject, and hit Play.
 *
 * @see AYarnFirstPersonPlayer for the spawned player
 * @see AYarnInteractableNPC for the spawned NPCs
 */
UCLASS(Blueprintable, BlueprintType)
class YARNSPINNER_API AYarnMultiNPCDemo : public AActor
{
	GENERATED_BODY()

public:
	AYarnMultiNPCDemo();

	virtual void BeginPlay() override;

	// ========================================================================
	// configuration
	// ========================================================================

	/**
	 * the yarn project containing dialogue for all NPCs.
	 * this should include nodes matching each NPC's NodeName.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	UYarnProject* YarnProject;

	/**
	 * configurations for NPCs to spawn.
	 * each entry defines an NPC's dialogue node, name, position, and rotation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	TArray<FYarnNPCConfig> NPCConfigs;

	/**
	 * where to spawn the player in the room.
	 * default: center of the room.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner")
	FVector PlayerSpawnLocation = FVector(0, 0, 100);

	/**
	 * size of the room in units (width x depth).
	 * height is fixed at 300 units.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room", meta = (ClampMin = "500.0"))
	FVector2D RoomSize = FVector2D(1000, 1000);

	/**
	 * height of the room walls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room", meta = (ClampMin = "200.0"))
	float RoomHeight = 300.0f;

	/**
	 * whether to spawn room geometry (floor, walls).
	 * disable if you want to place this in an existing level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	bool bSpawnRoom = true;

	/**
	 * whether to spawn a light source.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
	bool bSpawnLight = true;

	// ========================================================================
	// appearance
	// ========================================================================

	/**
	 * typewriter speed for dialogue text (characters per second).
	 * 0 = instant display.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	float TypewriterSpeed = 50.0f;

	// ========================================================================
	// runtime references
	// ========================================================================

	/**
	 * the spawned player pawn.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Runtime")
	AYarnFirstPersonPlayer* SpawnedPlayer;

	/**
	 * all spawned NPCs.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Runtime")
	TArray<AYarnInteractableNPC*> SpawnedNPCs;

	// ========================================================================
	// methods
	// ========================================================================

	/**
	 * manually start dialogue at a specific node.
	 * useful for triggering conversation from blueprints/code.
	 *
	 * @param NodeName the yarn node to start
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner")
	void StartDialogue(const FString& NodeName);

protected:
	// ========================================================================
	// spawning methods (can be overridden)
	// ========================================================================

	/**
	 * spawn the room geometry (floor and walls).
	 * override in subclasses for custom room shapes.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner")
	void SpawnRoom();
	virtual void SpawnRoom_Implementation();

	/**
	 * spawn the player pawn and possess it.
	 * override in subclasses for custom player setup.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner")
	void SpawnPlayer();
	virtual void SpawnPlayer_Implementation();

	/**
	 * spawn all configured NPCs.
	 * override in subclasses for custom NPC spawning.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner")
	void SpawnNPCs();
	virtual void SpawnNPCs_Implementation();

	/**
	 * spawn lighting for the room.
	 * override in subclasses for custom lighting.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner")
	void SpawnLighting();
	virtual void SpawnLighting_Implementation();

	/**
	 * create default NPC configurations if none are set.
	 * called in constructor to provide sensible defaults.
	 */
	void CreateDefaultNPCConfigs();

	/** track spawned static meshes for cleanup */
	UPROPERTY()
	TArray<AActor*> SpawnedGeometry;
};

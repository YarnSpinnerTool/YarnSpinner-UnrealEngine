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

// ----------------------------------------------------------------------------
// includes
// ----------------------------------------------------------------------------

// our own header - must be included first for unreal header tool
#include "YarnMultiNPCDemo.h"

// yarn spinner components
#include "YarnFirstPersonPlayer.h"
#include "YarnInteractableNPC.h"
#include "YarnDialogueRunner.h"
#include "YarnWidgetPresenter.h"
#include "YarnProgram.h"

// logging
#include "YarnSpinnerModule.h"

// player controller for possession
#include "GameFramework/PlayerController.h"

// static mesh for room geometry
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

// lighting
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"

// materials
#include "Materials/MaterialInstanceDynamic.h"

// world spawning
#include "Engine/World.h"

// ============================================================================
// AYarnMultiNPCDemo
// ============================================================================
//
// this is a complete, self-contained demo that spawns a room with multiple
// NPCs for the player to interact with. just drop it in an empty level.

// ----------------------------------------------------------------------------
// constructor
// ----------------------------------------------------------------------------

AYarnMultiNPCDemo::AYarnMultiNPCDemo()
{
	PrimaryActorTick.bCanEverTick = false;

	// create default NPC configurations
	CreateDefaultNPCConfigs();
}

// ----------------------------------------------------------------------------
// default NPC setup
// ----------------------------------------------------------------------------

void AYarnMultiNPCDemo::CreateDefaultNPCConfigs()
{
	// clear existing configs
	NPCConfigs.Empty();

	// create 4 NPCs at different positions around the room
	// positions are relative to the demo actor's location

	// guard - near the entrance
	NPCConfigs.Add(FYarnNPCConfig(
		TEXT("Guard"),
		TEXT("Town Guard"),
		FVector(-300, 0, 0),
		FRotator(0, 0, 0)
	));

	// merchant - in one corner
	NPCConfigs.Add(FYarnNPCConfig(
		TEXT("Merchant"),
		TEXT("Friendly Merchant"),
		FVector(200, 300, 0),
		FRotator(0, -135, 0)
	));

	// scholar - at a desk (simulated by position)
	NPCConfigs.Add(FYarnNPCConfig(
		TEXT("Scholar"),
		TEXT("Distracted Scholar"),
		FVector(200, -300, 0),
		FRotator(0, 135, 0)
	));

	// child - wandering in the middle
	NPCConfigs.Add(FYarnNPCConfig(
		TEXT("Child"),
		TEXT("Curious Child"),
		FVector(100, 0, 0),
		FRotator(0, 180, 0)
	));
}

// ----------------------------------------------------------------------------
// lifecycle
// ----------------------------------------------------------------------------

void AYarnMultiNPCDemo::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogYarnSpinner, Log, TEXT("MultiNPCDemo: Setting up demo scene"));

	// spawn everything
	if (bSpawnRoom)
	{
		SpawnRoom();
	}

	if (bSpawnLight)
	{
		SpawnLighting();
	}

	SpawnPlayer();
	SpawnNPCs();

	UE_LOG(LogYarnSpinner, Log, TEXT("MultiNPCDemo: Demo scene ready! Walk around and press E to talk to NPCs."));
}

// ----------------------------------------------------------------------------
// room spawning
// ----------------------------------------------------------------------------

void AYarnMultiNPCDemo::SpawnRoom_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// find the basic cube mesh for building the room
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));
	if (!CubeMesh)
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("MultiNPCDemo: Could not find Cube mesh for room geometry"));
		return;
	}

	const FVector DemoLocation = GetActorLocation();
	const float HalfWidth = RoomSize.X / 2.0f;
	const float HalfDepth = RoomSize.Y / 2.0f;
	const float WallThickness = 20.0f;

	// create a simple material for the room
	UMaterial* BasicMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));

	// --- FLOOR ---
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(
			DemoLocation + FVector(0, 0, -10),
			FRotator::ZeroRotator,
			SpawnParams
		);
		if (Floor)
		{
			Floor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
			Floor->SetActorScale3D(FVector(RoomSize.X / 100.0f, RoomSize.Y / 100.0f, 0.2f));
			Floor->GetStaticMeshComponent()->SetMaterial(0, BasicMaterial);
			SpawnedGeometry.Add(Floor);
		}
	}

	// --- WALLS ---
	auto SpawnWall = [&](const FVector& Location, const FVector& Scale, const FString& Name)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(
			DemoLocation + Location,
			FRotator::ZeroRotator,
			SpawnParams
		);
		if (Wall)
		{
			Wall->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
			Wall->SetActorScale3D(Scale);
			Wall->GetStaticMeshComponent()->SetMaterial(0, BasicMaterial);
			SpawnedGeometry.Add(Wall);
		}
	};

	// front wall (negative X)
	SpawnWall(
		FVector(-HalfWidth - WallThickness / 2, 0, RoomHeight / 2),
		FVector(WallThickness / 100.0f, RoomSize.Y / 100.0f, RoomHeight / 100.0f),
		TEXT("FrontWall")
	);

	// back wall (positive X)
	SpawnWall(
		FVector(HalfWidth + WallThickness / 2, 0, RoomHeight / 2),
		FVector(WallThickness / 100.0f, RoomSize.Y / 100.0f, RoomHeight / 100.0f),
		TEXT("BackWall")
	);

	// left wall (negative Y)
	SpawnWall(
		FVector(0, -HalfDepth - WallThickness / 2, RoomHeight / 2),
		FVector(RoomSize.X / 100.0f, WallThickness / 100.0f, RoomHeight / 100.0f),
		TEXT("LeftWall")
	);

	// right wall (positive Y)
	SpawnWall(
		FVector(0, HalfDepth + WallThickness / 2, RoomHeight / 2),
		FVector(RoomSize.X / 100.0f, WallThickness / 100.0f, RoomHeight / 100.0f),
		TEXT("RightWall")
	);

	UE_LOG(LogYarnSpinner, Log, TEXT("MultiNPCDemo: Spawned room geometry"));
}

// ----------------------------------------------------------------------------
// lighting
// ----------------------------------------------------------------------------

void AYarnMultiNPCDemo::SpawnLighting_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// spawn a point light in the center of the room
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;

	APointLight* Light = World->SpawnActor<APointLight>(
		GetActorLocation() + FVector(0, 0, RoomHeight - 50),
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (Light)
	{
		UPointLightComponent* LightComp = Light->PointLightComponent;
		if (LightComp)
		{
			LightComp->SetIntensity(5000.0f);
			LightComp->SetAttenuationRadius(RoomSize.X * 1.5f);
			LightComp->SetLightColor(FLinearColor(1.0f, 0.95f, 0.9f)); // warm white
		}
		SpawnedGeometry.Add(Light);
	}

	UE_LOG(LogYarnSpinner, Log, TEXT("MultiNPCDemo: Spawned lighting"));
}

// ----------------------------------------------------------------------------
// player spawning
// ----------------------------------------------------------------------------

void AYarnMultiNPCDemo::SpawnPlayer_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("MultiNPCDemo: No player controller found!"));
		return;
	}

	// unpossess and destroy any existing pawn
	if (APawn* ExistingPawn = PC->GetPawn())
	{
		PC->UnPossess();
		ExistingPawn->Destroy();
		UE_LOG(LogYarnSpinner, Log, TEXT("MultiNPCDemo: Destroyed existing pawn"));
	}

	// spawn the first-person player pawn
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;

	SpawnedPlayer = World->SpawnActor<AYarnFirstPersonPlayer>(
		GetActorLocation() + PlayerSpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (SpawnedPlayer)
	{
		// configure the player
		SpawnedPlayer->YarnProject = YarnProject;
		SpawnedPlayer->AutoPossessPlayer = EAutoReceiveInput::Player0;

		// configure the widget presenter
		if (SpawnedPlayer->WidgetPresenter)
		{
			SpawnedPlayer->WidgetPresenter->TypewriterSpeed = TypewriterSpeed;
		}

		// possess the player
		PC->Possess(SpawnedPlayer);

		// ensure input is enabled
		SpawnedPlayer->EnableInput(PC);

		UE_LOG(LogYarnSpinner, Log, TEXT("MultiNPCDemo: Player spawned at %s and possessed"),
			*(GetActorLocation() + PlayerSpawnLocation).ToString());
	}
	else
	{
		UE_LOG(LogYarnSpinner, Error, TEXT("MultiNPCDemo: Failed to spawn player!"));
	}
}

// ----------------------------------------------------------------------------
// NPC spawning
// ----------------------------------------------------------------------------

void AYarnMultiNPCDemo::SpawnNPCs_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector DemoLocation = GetActorLocation();

	for (const FYarnNPCConfig& Config : NPCConfigs)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;

		AYarnInteractableNPC* NPC = World->SpawnActor<AYarnInteractableNPC>(
			DemoLocation + Config.SpawnLocation,
			Config.SpawnRotation,
			SpawnParams
		);

		if (NPC)
		{
			// configure the NPC
			NPC->DialogueNodeName = Config.NodeName;
			NPC->CharacterDisplayName = Config.DisplayName;

			SpawnedNPCs.Add(NPC);

			UE_LOG(LogYarnSpinner, Log, TEXT("MultiNPCDemo: Spawned NPC '%s' at %s"),
				*Config.DisplayName, *Config.SpawnLocation.ToString());
		}
		else
		{
			UE_LOG(LogYarnSpinner, Warning, TEXT("MultiNPCDemo: Failed to spawn NPC '%s'"),
				*Config.DisplayName);
		}
	}

	UE_LOG(LogYarnSpinner, Log, TEXT("MultiNPCDemo: Spawned %d NPCs"), SpawnedNPCs.Num());
}

// ----------------------------------------------------------------------------
// dialogue control
// ----------------------------------------------------------------------------

void AYarnMultiNPCDemo::StartDialogue(const FString& NodeName)
{
	if (SpawnedPlayer && SpawnedPlayer->DialogueRunner)
	{
		SpawnedPlayer->DialogueRunner->StartDialogue(NodeName);
	}
	else
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("MultiNPCDemo: Cannot start dialogue - no player or dialogue runner"));
	}
}

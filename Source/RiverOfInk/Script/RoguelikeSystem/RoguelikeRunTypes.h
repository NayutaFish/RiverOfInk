// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoguelikeRunTypes.generated.h"

class UWorld;

/** Persistent state for the whole roguelike run. Owned by the GameInstance-level run-flow subsystem. */
UENUM(BlueprintType)
enum class ERoguelikeRunState : uint8
{
	MainMenu,
	Preparation,
	LoadingRoom,
	InRoom,
	Result
};

/** Why the run state moved. This is diagnostic context; it is not the state itself. */
UENUM(BlueprintType)
enum class ERoguelikeRunTransitionReason : uint8
{
	None,
	EnterPreparation,
	StartRun,
	NextRoom,
	NextMajorStage,
	Restart,
	ReturnToMainMenu,
	RoomLoaded,
	RunCompleted,
	PlayerDefeated
};

/** Outcome data stays separate from Result so the result UI can distinguish victory from defeat. */
UENUM(BlueprintType)
enum class ERoguelikeRunOutcome : uint8
{
	None,
	Victory,
	Defeat
};

/** Map-local room state. A room GameMode will own this in the next implementation slice. */
UENUM(BlueprintType)
enum class ERoguelikeRoomState : uint8
{
	Initializing,
	Entering,
	Ready,
	Combat,
	Reward,
	Completed,
	Exiting
};

/** Gameplay role of a concrete room. More roles can be added without changing the run-flow code. */
UENUM(BlueprintType)
enum class ERoguelikeRoomType : uint8
{
	Combat
};

/** One concrete room candidate in a major-stage pool. RoomId is an identifier, not a string to parse. */
USTRUCT(BlueprintType)
struct FRoguelikeRoomDefinition
{
	GENERATED_BODY()

	/** Content identifier, for example M01_Combat_A. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Room")
	FName RoomId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Room")
	ERoguelikeRoomType RoomType = ERoguelikeRoomType::Combat;

	/** Relative probability for this room when drawing without replacement. Values below one are invalid. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Room", meta = (ClampMin = "1"))
	int32 SelectionWeight = 1;

	/** UE map asset which implements this room. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Room")
	TSoftObjectPtr<UWorld> RoomMap;
};

/** Immutable configuration for one abstract major stage. The active room sequence is generated at runtime. */
USTRUCT(BlueprintType)
struct FMajorStageDefinition
{
	GENERATED_BODY()

	/** Zero-based index used by run-flow code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Major Stage")
	int32 MajorStageIndex = INDEX_NONE;

	/** Number of unique rooms to draw from RoomPool for this stage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Major Stage", meta = (ClampMin = "1"))
	int32 RoomSequenceLength = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Major Stage")
	TArray<FRoguelikeRoomDefinition> RoomPool;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "PlayerRuntimeData.generated.h"

/**
 * Placeholder for a buff that belongs to the current run.
 *
 * The first runtime-data slice only reserves the storage boundary. Buff
 * identity, stacks, duration, and application rules will be defined together
 * with the buff system instead of being guessed here.
 */
USTRUCT(BlueprintType)
struct FRunBuffData
{
	GENERATED_BODY()
};

/** Effective player stats that must survive a level transition in one run. */
USTRUCT(BlueprintType)
struct FPlayerRuntimeStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats")
	int32 PhysicalResistance = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats")
	int32 MagicResistance = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats")
	float WalkSpeed = 600.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats")
	float SprintSpeed = 900.0f;
};

/**
 * The complete player-owned state for the current run.
 *
 * This is a value type on purpose: the GameInstance subsystem owns a copy,
 * while a PlayerCharacter and its components own the live values for the
 * current world.
 */
USTRUCT(BlueprintType)
struct FPlayerRuntimeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Skills")
	TArray<FPlayerSkillSlot> SkillSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Skills")
	TMap<EPlayerSkillID, FSkillUpgradeState> SkillUpgradeStates;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats")
	FPlayerRuntimeStats Stats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Buffs")
	TArray<FRunBuffData> RunBuffs;
};

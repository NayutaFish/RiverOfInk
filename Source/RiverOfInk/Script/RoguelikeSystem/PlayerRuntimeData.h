// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeSystem/RoguelikeEconomyTypes.h"
#include "PlayerRuntimeData.generated.h"

/**
 * Data-only state for a temporary buff that belongs to the current run.
 *
 * E0 defines the persistence contract only. Applying modifiers, decrementing
 * CombatRoomDuration, and removing expired entries belong to the Shop/Buff
 * slices that consume this data.
 */
USTRUCT(BlueprintType)
struct FRunBuffData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Runtime|Buffs")
	FName BuffId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Runtime|Buffs")
	EPlayerRuntimeStat StatType = EPlayerRuntimeStat::MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Runtime|Buffs")
	float AdditiveValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Runtime|Buffs")
	float MultiplierValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Runtime|Buffs", meta = (ClampMin = "0"))
	int32 RemainCombatCount = 0;
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

	/** Single defense value used by the current damage model. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats")
	int32 Defense = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats", meta = (DeprecatedProperty, DeprecationMessage = "Use Defense."))
	int32 PhysicalResistance = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Runtime|Stats", meta = (DeprecatedProperty, DeprecationMessage = "Use Defense."))
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

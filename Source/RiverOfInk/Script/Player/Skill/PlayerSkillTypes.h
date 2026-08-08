// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlayerSkillTypes.generated.h"

UENUM(BlueprintType)
enum class EPlayerSkillID : uint8
{
	None UMETA(DisplayName = "None"),
	TripleProjectile UMETA(DisplayName = "Triple Projectile"),
	CircularSlash UMETA(DisplayName = "Circular Slash")
};

/**
 * Runtime form of a skill. Forms change attack geometry/tempo while the
 * owning skill slot and its upgrade levels remain unchanged.
 */
UENUM(BlueprintType)
enum class EPlayerSkillForm : uint8
{
	Default UMETA(DisplayName = "Default"),
	ThrownGrenade UMETA(DisplayName = "Thrown Grenade")
};

UENUM(BlueprintType)
enum class ESkillUpgradeType : uint8
{
	None UMETA(DisplayName = "None"),
	Mechanic UMETA(DisplayName = "Mechanic"),
	Cooldown UMETA(DisplayName = "Cooldown"),
	Damage UMETA(DisplayName = "Damage")
};

UENUM(BlueprintType)
enum class ERoguelikeRewardType : uint8
{
	GainSkill UMETA(DisplayName = "Gain Skill"),
	UpgradeSkill UMETA(DisplayName = "Upgrade Skill")
};

USTRUCT(BlueprintType)
struct FPlayerSkillSlot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	EPlayerSkillID SkillID = EPlayerSkillID::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	int32 SkillLevel = 0;

	/**
	 * Form is part of the slot so a skill-form reward survives level travel in
	 * FPlayerRuntimeData. Older snapshots deserialize as Default.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	EPlayerSkillForm SkillForm = EPlayerSkillForm::Default;
};

USTRUCT(BlueprintType)
struct FSkillUpgradeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	int32 MechanicLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	int32 CooldownLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
	int32 DamageLevel = 0;
};

USTRUCT(BlueprintType)
struct FRoguelikeRewardOption
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward")
	ERoguelikeRewardType RewardType = ERoguelikeRewardType::UpgradeSkill;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward")
	EPlayerSkillID SkillID = EPlayerSkillID::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward")
	ESkillUpgradeType UpgradeType = ESkillUpgradeType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward")
	FText Title;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward")
	FText Description;
};

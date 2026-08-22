// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlayerSkillTypes.generated.h"

class UTexture2D;

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
	ThrownGrenade UMETA(DisplayName = "Thrown Grenade"),
	NullRing UMETA(DisplayName = "Null Ring"),
	TwinSlash UMETA(DisplayName = "Twin Slash")
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
	UpgradeSkill UMETA(DisplayName = "Upgrade Skill"),
	ChangeSkillForm UMETA(DisplayName = "Change Skill Form"),
	/** New build rewards mutate a skill slot through ESkillModifierID. */
	Modifier UMETA(DisplayName = "Skill Modifier"),
	/** Immediate Pure Ink reward; appended to preserve existing serialized enum values. */
	Currency UMETA(DisplayName = "Currency"),
	/** Immediate player health recovery reward; appended to preserve existing serialized enum values. */
	Health UMETA(DisplayName = "Health")
};

/**
 * Persistent modifiers granted by the in-run build system.
 *
 * SkillForm remains in the slot for backwards-compatible snapshots. New
 * rewards should write ModifierID/StackCount instead of replacing a form.
 */
UENUM(BlueprintType)
enum class ESkillModifierID : uint8
{
	None UMETA(DisplayName = "None"),
	AddProjectile UMETA(DisplayName = "Add Projectile"),
	InkGrenade UMETA(DisplayName = "Ink Grenade"),
	ExtraExplosion UMETA(DisplayName = "Extra Explosion"),
	TwinSlash UMETA(DisplayName = "Twin Slash"),
	NullRing UMETA(DisplayName = "Null Ring"),
	RadiusUp UMETA(DisplayName = "Radius Up"),
	CooldownDown UMETA(DisplayName = "Cooldown Down"),
	/** Enables marked-target selection for player-owned moving projectiles. */
	ProjectileHoming UMETA(DisplayName = "Projectile Homing")
};

UENUM(BlueprintType)
enum class ESkillPayloadType : uint8
{
	NormalProjectile UMETA(DisplayName = "Normal Projectile"),
	InkGrenade UMETA(DisplayName = "Ink Grenade")
};

USTRUCT(BlueprintType)
struct FSkillModifierState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Build")
	ESkillModifierID ModifierID = ESkillModifierID::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Build", meta = (ClampMin = "0"))
	int32 StackCount = 0;
};

/** Immutable output of the common skill resolver for one cast. */
USTRUCT(BlueprintType)
struct FResolvedSkillSpec
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved")
	EPlayerSkillID SkillID = EPlayerSkillID::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved")
	ESkillPayloadType PayloadType = ESkillPayloadType::NormalProjectile;

	// Q / Triple Projectile fields.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q", meta = (ClampMin = "0"))
	int32 ProjectileCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q")
	float ProjectileSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q")
	float ProjectileLifeTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q")
	float FuseTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q")
	float ExplosionRadius = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q")
	float ExplosionDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q")
	float CollisionRadius = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q", meta = (ClampMin = "1"))
	int32 ExplosionCount = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q")
	float ExplosionDelay = 0.12f;

	/** Whether this cast may select a player-owned homing mark. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q")
	bool bEnableHoming = false;

	/** Constant turn rate used by moving projectiles in degrees per second. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|Q", meta = (ClampMin = "0.0", Units = "deg/s"))
	float HomingTurnRate = 360.0f;

	// E / Circular Slash fields.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|E", meta = (ClampMin = "1"))
	int32 HitCount = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|E")
	float Radius = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|E")
	float Damage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|E")
	float SecondHitDelay = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|E")
	float SecondHitAngle = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|E")
	float SecondHitForwardOffset = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|E")
	float SecondHitDamageMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved|E")
	bool bNullifyEnemyProjectiles = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Resolved")
	float Cooldown = 0.0f;
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

	/** Additive/behavior modifiers persisted with the skill slot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Build")
	TArray<FSkillModifierState> Modifiers;
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

	/** Snapshot taken when this card is generated; used for accurate UI copy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Form")
	EPlayerSkillForm CurrentSkillForm = EPlayerSkillForm::Default;

	/** Destination form for a form-change reward; normal upgrades keep CurrentSkillForm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Form")
	EPlayerSkillForm TargetSkillForm = EPlayerSkillForm::Default;

	/** Modifier payload used by the new Isaac-style reward pool. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Modifier")
	ESkillModifierID ModifierID = ESkillModifierID::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Modifier", meta = (ClampMin = "0"))
	int32 StackDelta = 0;

	/** Optional preview values captured when the card is generated. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Preview")
	float BeforeValue = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Preview")
	float AfterValue = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward")
	FText Title;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward")
	FText Description;

	/** Optional presentation contract consumed by the generic Reward Option widget. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Presentation")
	TObjectPtr<UTexture2D> RewardIcon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Presentation")
	FText PrimaryValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Presentation")
	FText OldValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Presentation")
	FText NewValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Presentation")
	FText ShortDescription;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Presentation")
	FText TargetSkill;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Presentation")
	FText BuildType;

	/** Currency amount used by Currency rewards. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Currency", meta = (ClampMin = "0"))
	int32 CurrencyAmount = 0;

	/** Health amount used by Health rewards. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Health", meta = (ClampMin = "0.0"))
	float HealthRestoreAmount = 0.0f;
};

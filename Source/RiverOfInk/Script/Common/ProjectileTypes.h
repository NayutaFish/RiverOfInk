// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/CombatEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ProjectileTypes.generated.h"

class AActor;

/** Movement policy selected by the resolved skill payload. */
UENUM(BlueprintType)
enum class EProjectileGuidanceMode : uint8
{
	None UMETA(DisplayName = "None"),
	SoftProjectileHoming UMETA(DisplayName = "Soft Projectile Homing"),
	TargetedArcLanding UMETA(DisplayName = "Targeted Arc Landing")
};

/**
 * Runtime contract shared by player-owned moving projectiles.
 *
 * Damage remains owned by the projectile/skill class. This spec carries
 * movement and targeting data; each payload selects its own guidance policy.
 */
USTRUCT(BlueprintType)
struct FProjectileSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0"))
	float LifeTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 0.0f;

	/** Optional resolved damage preview; damage execution remains class-owned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Damage", meta = (ClampMin = "0.0"))
	float Damage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing")
	bool bEnableHoming = false;

	/** Payload-specific guidance policy. None preserves the original straight flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Guidance")
	EProjectileGuidanceMode GuidanceMode = EProjectileGuidanceMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0", Units = "deg/s"))
	float HomingTurnRate = 360.0f;

	/** Delay before a spawned projectile begins steering toward its locked target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0", Units = "s"))
	float HomingStartDelay = 0.06f;

	/** Maximum distance at which the projectile may keep correcting toward its locked target. Zero disables this limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0"))
	float HomingMaxDistance = 2500.0f;

	/** Once inside this radius, stop steering and let the projectile continue on its current heading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0"))
	float HomingAcceptanceRadius = 80.0f;

	/** Fixed horizontal offset used by targeted arc payloads to distribute landing points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Guidance")
	FVector GuidanceTargetOffset = FVector::ZeroVector;

	/** Target selected at spawn time. A projectile never retargets mid-flight. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Homing")
	TObjectPtr<AActor> HomingTarget;

	/** Exact player-owned mark identity captured together with HomingTarget. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Homing")
	FCombatEffectHandle HomingMarkHandle;

	/** Tags describing the projectile build that produced this projectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Tags")
	FGameplayTagContainer ProjectileTags;
};

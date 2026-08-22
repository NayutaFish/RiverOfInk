// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/CombatEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ProjectileTypes.generated.h"

class AActor;

/**
 * Runtime contract shared by player-owned moving projectiles.
 *
 * Damage remains owned by the projectile/skill class. This spec only carries
 * movement and targeting data so normal projectiles, Attack2, and the thrown
 * grenade can share the same homing rules.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0", Units = "deg/s"))
	float HomingTurnRate = 360.0f;

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

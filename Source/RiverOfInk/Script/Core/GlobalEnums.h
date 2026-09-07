// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalEnums.generated.h"

/**
 * 全局玩法枚举
 * 所有与 GamePlay 相关的跨模块枚举统一在此定义。
 */

// ====================
// 伤害类型
// ====================

UENUM(BlueprintType)
enum class EDamageType : uint8
{
	Physical UMETA(DisplayName = "Physical"),
	Magic UMETA(DisplayName = "Magic"),
	TrueDamage UMETA(DisplayName = "True Damage"),
	Must UMETA(DisplayName = "Must"),
	Unified UMETA(DisplayName = "Unified Damage")
};

// ====================
// 攻击类型
// ====================

/** Identifies one damage-event source for the BaseAttackPower model. */
UENUM(BlueprintType)
enum class EAttackType : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	PlayerBasicAttack UMETA(DisplayName = "Player Basic Attack"),
	PlayerRightClick UMETA(DisplayName = "Player Right Click"),
	PlayerQProjectile UMETA(DisplayName = "Player Q Projectile"),
	PlayerQGrenade UMETA(DisplayName = "Player Q Grenade"),
	PlayerESlash UMETA(DisplayName = "Player E Slash"),
	PlayerESlashStage UMETA(DisplayName = "Player E Slash Stage"),
	PlayerETwinSlashSecond UMETA(DisplayName = "Player E Twin Slash Second"),
	EnemyMelee UMETA(DisplayName = "Enemy Melee"),
	EnemyRanged UMETA(DisplayName = "Enemy Ranged"),
	EnemyCharge UMETA(DisplayName = "Enemy Charge"),
	EnemySuicide UMETA(DisplayName = "Enemy Suicide")
};

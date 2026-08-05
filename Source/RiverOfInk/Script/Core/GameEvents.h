// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/GlobalStructs.h"

class AActor;
class AEnemyBase;
class AAttackAreaBase;

/**
 * 游戏内事件定义
 * 所有通过 FEventBus 发布/订阅的事件结构体统一在此定义。
 */

// ====================
// 玩家血量属性改变事件
// ====================

struct FPlayerHealthChangedEvent
{
	/** 生命最大值 */
	int32 MaxHealth = 0;

	/** 当前生命值 */
	int32 CurrentHealth = 0;

	FPlayerHealthChangedEvent() = default;

	FPlayerHealthChangedEvent(int32 InMaxHealth, int32 InCurrentHealth)
		: MaxHealth(InMaxHealth)
		, CurrentHealth(InCurrentHealth)
	{
	}
};

// ====================
// 玩家生成完毕事件
// ====================

struct FPlayerSpawnedEvent
{
	/** 生成的玩家 */
	TObjectPtr<AActor> Player = nullptr;

	FPlayerSpawnedEvent() = default;

	FPlayerSpawnedEvent(AActor* InPlayer)
		: Player(InPlayer)
	{
	}
};

// ====================
// 玩家受到直接性攻击事件
// ====================

struct FPlayerTookDirectDamageEvent
{
	/** 玩家受击信息 */
	FTakeDamageInfo TakeDamageInfo;

	FPlayerTookDirectDamageEvent() = default;

	FPlayerTookDirectDamageEvent(const FTakeDamageInfo& InInfo)
		: TakeDamageInfo(InInfo)
	{
	}
};

// ====================
// 场景切换事件
// ====================

struct FSceneChangedEvent
{
	/** 新场景名称 */
	FName LevelName = NAME_None;

	FSceneChangedEvent() = default;

	FSceneChangedEvent(const FName& InLevelName)
		: LevelName(InLevelName)
	{
	}
};

// ====================
// 非玩家单位（敌人）死亡事件
// ====================

struct FNonPlayerDiedEvent
{
	/** 死亡的非玩家单位 */
	TObjectPtr<AEnemyBase> Victim = nullptr;

	/** 致死伤害信息 */
	FTakeDamageInfo DamageInfo;

	/** 造成伤害的攻击区域（可能为空，如非攻击区域导致的死亡） */
	TObjectPtr<AAttackAreaBase> AttackArea = nullptr;

	FNonPlayerDiedEvent() = default;

	FNonPlayerDiedEvent(AEnemyBase* InVictim, const FTakeDamageInfo& InDamageInfo, AAttackAreaBase* InAttackArea)
		: Victim(InVictim)
		, DamageInfo(InDamageInfo)
		, AttackArea(InAttackArea)
	{
	}
};

// ====================
// 玩家死亡事件
// ====================

struct FPlayerDiedEvent
{
	/** 击杀者 */
	TObjectPtr<AActor> Killer = nullptr;

	/** 死亡玩家 */
	TObjectPtr<AActor> Player = nullptr;

	FPlayerDiedEvent() = default;

	FPlayerDiedEvent(AActor* InKiller, AActor* InPlayer)
		: Killer(InKiller)
		, Player(InPlayer)
	{
	}
};

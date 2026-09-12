// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/GlobalStructs.h"
#include "Player/Skill/PlayerSkillTypes.h"

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

// ====================
// 玩家普攻（CommonAttack）命中事件
// ====================

struct FPlayerCommonAttackHitEvent
{
	/** 命中的敌人 */
	TObjectPtr<AEnemyBase> Enemy = nullptr;

	FPlayerCommonAttackHitEvent() = default;

	FPlayerCommonAttackHitEvent(AEnemyBase* InEnemy)
		: Enemy(InEnemy)
	{
	}
};

// ====================
// 玩家特攻（SpecialAttack）命中事件
// ====================

struct FPlayerSpecialAttackHitEvent
{
	/** 命中的敌人 */
	TObjectPtr<AEnemyBase> Enemy = nullptr;

	FPlayerSpecialAttackHitEvent() = default;

	FPlayerSpecialAttackHitEvent(AEnemyBase* InEnemy)
		: Enemy(InEnemy)
	{
	}
};

// ====================
// 玩家 Q 技能命中事件
// ====================

struct FPlayerSkillQHitEvent
{
	/** 命中的敌人 */
	TObjectPtr<AEnemyBase> Enemy = nullptr;

	FPlayerSkillQHitEvent() = default;

	FPlayerSkillQHitEvent(AEnemyBase* InEnemy)
		: Enemy(InEnemy)
	{
	}
};

// ====================
// 玩家 E 技能命中事件
// ====================

struct FPlayerSkillEHitEvent
{
	/** 命中的敌人 */
	TObjectPtr<AEnemyBase> Enemy = nullptr;

	FPlayerSkillEHitEvent() = default;

	FPlayerSkillEHitEvent(AEnemyBase* InEnemy)
		: Enemy(InEnemy)
	{
	}
};

// ====================
// 玩家 Q 技能施放事件
// ====================

struct FPlayerSkillQCastEvent
{
	FPlayerSkillQCastEvent() = default;
};

// ====================
// 玩家 E 技能施放事件
// ====================

struct FPlayerSkillECastEvent
{
	FPlayerSkillECastEvent() = default;
};

// ====================
// 玩家 Q 技能冷却完毕事件
// ====================

struct FPlayerSkillQCooldownReadyEvent
{
	FPlayerSkillQCooldownReadyEvent() = default;
};

// ====================
// 玩家 E 技能冷却完毕事件
// ====================

struct FPlayerSkillECooldownReadyEvent
{
	FPlayerSkillECooldownReadyEvent() = default;
};

// ====================
// 玩家进入冲刺事件
// ====================

struct FPlayerEnterDashEvent
{
	FPlayerEnterDashEvent() = default;
};

// ====================
// 玩家退出冲刺事件
// ====================

struct FPlayerExitDashEvent
{
	FPlayerExitDashEvent() = default;
};

// ====================
// 单局战斗开始事件
// ====================

struct FCombatRoomStartedEvent
{
	FCombatRoomStartedEvent() = default;
};

// ====================
// 单局战斗结束事件
// ====================

struct FCombatRoomClearedEvent
{
	FCombatRoomClearedEvent() = default;
};

// ====================
// 精英敌人死亡事件（仅当敌人 isElite == true 时通报）
// ====================

struct FOnEliteEnemyDiedEvent
{
	/** 死亡的精英敌人 */
	TObjectPtr<AEnemyBase> Victim = nullptr;

	FOnEliteEnemyDiedEvent() = default;

	FOnEliteEnemyDiedEvent(AEnemyBase* InVictim)
		: Victim(InVictim)
	{
	}
};

// ====================
// 非玩家单位受到玩家伤害事件
// ====================

struct FOnNonPlayerTakeDamageFromPlayer
{
	/** 本次受到的最终伤害（结算防御/倍率之后真正扣掉的血量） */
	float FinalDamage = 0.0f;

	FOnNonPlayerTakeDamageFromPlayer() = default;

	FOnNonPlayerTakeDamageFromPlayer(float InFinalDamage)
		: FinalDamage(InFinalDamage)
	{
	}
};

// ====================
// 获得商店货币事件（纯墨收入）
// ====================

struct FShopCurrencyGainedEvent
{
	/** 本次获得的货币数量（正数） */
	int32 Amount = 0;

	FShopCurrencyGainedEvent() = default;

	FShopCurrencyGainedEvent(int32 InAmount)
		: Amount(InAmount)
	{
	}
};

// ====================
// 完成一次商店购买事件
// ====================

struct FShopPurchaseCompletedEvent
{
	FShopPurchaseCompletedEvent() = default;
};

// ====================
// 玩家选定清场奖励（增益）事件
// ====================

struct FRewardSelectedEvent
{
	/**
	 * 奖励的稳定字符串标识，由枚举名拼成，例如：
	 *   "Modifier.ProjectileHoming" / "Modifier.AddProjectile"
	 *   "ChangeSkillForm.TripleProjectile.NullRing"
	 *   "GainSkill.CircularSlash" / "Currency" / "Health"
	 */
	FString RewardId;

	/** 卡面标题（本地化显示名，例如 "引墨"） */
	FText Title;

	/** 本次叠加层数：词条类奖励为实际叠加数，其它类型为 1。 */
	int32 StackCount = 1;

	/**
	 * Stable reward payload retained by settlement so result UI can resolve the
	 * same Build icon as the in-run HUD without parsing localized card text.
	 */
	ERoguelikeRewardType RewardType = ERoguelikeRewardType::Modifier;
	EPlayerSkillID SkillID = EPlayerSkillID::None;
	ESkillUpgradeType UpgradeType = ESkillUpgradeType::None;
	EPlayerSkillForm TargetSkillForm = EPlayerSkillForm::Default;
	ESkillModifierID ModifierID = ESkillModifierID::None;
	int32 CurrencyAmount = 0;
	float HealthRestoreAmount = 0.0f;

	FRewardSelectedEvent() = default;

	FRewardSelectedEvent(const FString& InRewardId, const FText& InTitle, int32 InStackCount)
		: RewardId(InRewardId)
		, Title(InTitle)
		, StackCount(InStackCount)
	{
	}

	FRewardSelectedEvent(const FString& InRewardId, const FRoguelikeRewardOption& InReward)
		: RewardId(InRewardId)
		, Title(InReward.Title)
		, StackCount(FMath::Max(1, InReward.StackDelta))
		, RewardType(InReward.RewardType)
		, SkillID(InReward.SkillID)
		, UpgradeType(InReward.UpgradeType)
		, TargetSkillForm(InReward.TargetSkillForm)
		, ModifierID(InReward.ModifierID)
		, CurrencyAmount(InReward.CurrencyAmount)
		, HealthRestoreAmount(InReward.HealthRestoreAmount)
	{
	}
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/GlobalEnums.h"
#include "GlobalStructs.generated.h"

class AActor;

/**
 * 全局玩法结构体
 * 所有与 GamePlay 相关的跨模块结构体统一在此定义。
 */

// ====================
// 受击信息
// ====================

USTRUCT(BlueprintType)
struct FTakeDamageInfo
{
	GENERATED_BODY()

	/** 攻击者（谁打的） */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TObjectPtr<AActor> Attacker;

	/**
	 * Legacy metadata retained so old Blueprint assets and callers still load.
	 * Health owners now use one damage calculation regardless of this value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (DeprecatedProperty, DeprecationMessage = "DamageType is metadata only; use the unified damage model."))
	EDamageType DamageType = EDamageType::Unified;

	/** 伤害值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DamageValue = 0.0f;

	/**
	 * 硬值伤害。为 0 时由受击者使用默认比例从 DamageValue 推导，
	 * 以保持旧版攻击蓝图兼容。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Hard Value", meta = (ClampMin = "0.0"))
	float HardDamageValue = 0.0f;

	/** 能否致死 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bCanCauseDeath = true;

	/** 是否为直接性攻击 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIsDirectDamage = true;

	/** 是否无视无敌 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIgnoreInvincible = false;
};

/** 直接性受击事件委托（玩家/敌人通用，状态类可订阅，如击退） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTakeDirectDamageSignature, const FTakeDamageInfo&, DamageInfo);

/**
 * Resolved enemy damage result used by hard-value reactions.
 * Health damage and hard-value damage are intentionally reported separately.
 */
USTRUCT(BlueprintType)
struct FEnemyDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FTakeDamageInfo DamageInfo;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	int32 FinalDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage|Hard Value")
	float HardValueBefore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage|Hard Value")
	float HardValueAfter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage|Hard Value")
	float HardDamageApplied = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage|Hard Value")
	bool bHardBreak = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bKilled = false;
};

/** Broadcast only when an enemy's hard value is broken. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyHardBreakSignature, const FEnemyDamageResult&, DamageResult);

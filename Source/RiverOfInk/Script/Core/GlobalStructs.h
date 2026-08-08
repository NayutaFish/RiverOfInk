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

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/GlobalEnums.h"
#include "GameplayTagContainer.h"
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
 * Unified damage request passed through the combat damage pipeline.
 *
 * FTakeDamageInfo remains the Blueprint and legacy compatibility shape. New
 * gameplay code should fill this context so source/target effects can modify
 * one request without adding another special-case damage path.
 */
USTRUCT(BlueprintType)
struct FDamageContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BaseDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Hard Value", meta = (ClampMin = "0.0"))
	float HardDamage = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(BlueprintReadWrite, Category = "Damage")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Tags")
	FGameplayTagContainer DamageTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	EDamageType DamageType = EDamageType::Unified;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bCanCauseDeath = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIsDirectDamage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIgnoreInvulnerability = false;

	FDamageContext() = default;

	explicit FDamageContext(const FTakeDamageInfo& InInfo)
		: BaseDamage(InInfo.DamageValue)
		, HardDamage(InInfo.HardDamageValue)
		, SourceActor(InInfo.Attacker)
		, DamageType(InInfo.DamageType)
		, bCanCauseDeath(InInfo.bCanCauseDeath)
		, bIsDirectDamage(InInfo.bIsDirectDamage)
		, bIgnoreInvulnerability(InInfo.bIgnoreInvincible)
	{
	}

	FTakeDamageInfo ToLegacyDamageInfo() const
	{
		FTakeDamageInfo LegacyInfo;
		LegacyInfo.Attacker = SourceActor;
		LegacyInfo.DamageType = DamageType;
		LegacyInfo.DamageValue = BaseDamage;
		LegacyInfo.HardDamageValue = HardDamage;
		LegacyInfo.bCanCauseDeath = bCanCauseDeath;
		LegacyInfo.bIsDirectDamage = bIsDirectDamage;
		LegacyInfo.bIgnoreInvincible = bIgnoreInvulnerability;
		return LegacyInfo;
	}
};

/** Result of one unified damage attempt, including blocked/no-damage reasons. */
USTRUCT(BlueprintType)
struct FDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FDamageContext Context;

	/** Damage after source and target effect modifiers, before Defense. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float ModifiedDamage = 0.0f;

	/** Final integer health damage after Defense and the global minimum rule. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	int32 FinalDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Damage|State")
	bool bBlockedByInvulnerability = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage|State")
	bool bNoDamage = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage|State")
	bool bDamageApplied = false;
};

/**
 * Resolved enemy damage result used by hard-value reactions.
 * Health damage and hard-value damage are intentionally reported separately.
 */
USTRUCT(BlueprintType)
struct FEnemyDamageResult
{
	GENERATED_BODY()

	/** Full unified result, including blocked/no-damage state. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FDamageResult ResolvedDamage;

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

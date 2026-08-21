// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/AttackAreaBase.h"
#include "AttackArea_PlayerAttack2.generated.h"

class ASpecialBuff_PlayerAttack2;

/**
 * 玩家攻击 2 的攻击区域：命中敌人时通报特攻命中事件，
 * 并生成 SpecialBuff 应用到命中的敌人
 */
UCLASS()
class RIVEROFINK_API AAttackArea_PlayerAttack2 : public AAttackAreaBase
{
	GENERATED_BODY()

public:
	AAttackArea_PlayerAttack2();
	/**
	 * Legacy configuration source. Existing Blueprint defaults may still point
	 * at BP_SpecialBuff_PlayerAttack2; its BonusDamageInfo is read as a
	 * migration fallback, but no Buff Actor is spawned anymore.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Buff", meta = (DeprecatedProperty, DeprecationMessage = "Configure NextHitBonusDamageInfo; the runtime now uses CombatEffectComponent."))
	TSubclassOf<ASpecialBuff_PlayerAttack2> SpecialBuffClass;

	/** Damage payload applied by the target's next valid hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Buff")
	FTakeDamageInfo NextHitBonusDamageInfo;

	/** Lifetime of the proc after Attack2 hits an enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Buff", meta = (ClampMin = "0.01", Units = "s"))
	float NextHitBonusDuration = 4.0f;

	/** Number of valid hits that can consume one application. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Buff", meta = (ClampMin = "1"))
	int32 NextHitBonusCharges = 1;

	/** Maximum stacked Attack2 proc applications on one enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Buff", meta = (ClampMin = "1"))
	int32 NextHitBonusMaxStacks = 3;

protected:
	virtual void ApplyDamage_Implementation(AActor* Target) override;
};

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
	/** 命中敌人时生成的特殊 Buff 类（蓝图赋值，可空） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Buff")
	TSubclassOf<ASpecialBuff_PlayerAttack2> SpecialBuffClass;

protected:
	virtual void ApplyDamage_Implementation(AActor* Target) override;
};

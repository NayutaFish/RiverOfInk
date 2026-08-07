// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/AttackAreaBase.h"
#include "AttackArea_PlayerAttack1.generated.h"

/**
 * 玩家攻击 1 的攻击区域：命中敌人时通报普攻命中事件
 */
UCLASS()
class RIVEROFINK_API AAttackArea_PlayerAttack1 : public AAttackAreaBase
{
	GENERATED_BODY()

protected:
	virtual void ApplyDamage_Implementation(AActor* Target) override;
};

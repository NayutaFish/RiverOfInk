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

public:
	/** 普攻命中时的短顿帧；二段攻击复用同一攻击范围类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback|HitStop", meta = (ClampMin = "0.0", Units = "s"))
	float HitStopDuration = 0.045f;

protected:
	virtual void ApplyDamage_Implementation(AActor* Target) override;
};

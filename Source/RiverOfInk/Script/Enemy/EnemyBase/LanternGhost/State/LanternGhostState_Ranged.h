// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "LanternGhostState_Ranged.generated.h"

/**
* 灯笼怪远程攻击状态。
* 当 EnemyBase 的 AttackAreaClass 是 AAttackAreaBase_Bezier 时，
* 生成两个贝塞尔曲线攻击区域进行远程攻击。
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API ULanternGhostState_Ranged : public UEnemyState_Attack
{
	GENERATED_BODY()

	public:
	ULanternGhostState_Ranged();

	protected:
	virtual void OnExit_Implementation() override;
	virtual void ExecuteAttack() override;

	private:
	FTimerHandle BezierReturnHandle;
};

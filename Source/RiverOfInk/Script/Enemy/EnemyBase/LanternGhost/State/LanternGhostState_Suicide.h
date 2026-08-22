// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "LanternGhostState_Suicide.generated.h"

/**
 * 灯笼怪自爆攻击状态。
 * 继承自 EnemyState_Attack；进入该状态 0.1 秒后触发死亡。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API ULanternGhostState_Suicide : public UEnemyState_Attack
{
	GENERATED_BODY()

public:
	ULanternGhostState_Suicide();

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;

private:
	/** 自爆：进入状态 0.1 秒后调用宿主死亡方法 */
	void Detonate();

	FTimerHandle DetonateTimerHandle;
};

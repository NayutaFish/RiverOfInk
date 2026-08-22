// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "LanternGhostState_Ranged.generated.h"

/**
 * 灯笼怪远程攻击状态。
 * 继承自 EnemyState_Attack，具体实现待补充。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API ULanternGhostState_Ranged : public UEnemyState_Attack
{
	GENERATED_BODY()

public:
	ULanternGhostState_Ranged();
};

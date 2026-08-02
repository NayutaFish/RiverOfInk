// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "EnemyState_Idle.generated.h"

/**
 * 待机状态：每 0.5s 检测与玩家的 XY 距离，小于阈值则切 Chase
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class TEST_GAMEPLAY_API UEnemyState_Idle : public UStateBase
{
	GENERATED_BODY()

public:
	UEnemyState_Idle();

protected:
	virtual void BeginPlay() override;

private:
	void CheckPlayerDistance();

	FTimerHandle DetectTimerHandle;
};

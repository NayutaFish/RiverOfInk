// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "Core/GlobalStructs.h"
#include "EnemyState_Idle.generated.h"

/**
 * 待机状态：每 0.5s 检测与玩家的 XY 距离，小于阈值则切 Chase；
 * 受直接性伤害时切 HitBack
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UEnemyState_Idle : public UStateBase
{
	GENERATED_BODY()

public:
	UEnemyState_Idle();

protected:
	virtual void BeginPlay() override;
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;

	/** 受直接性伤害：切入击退状态 */
	UFUNCTION()
	void OnTakeDirectDamage(const FTakeDamageInfo& DamageInfo);

private:
	void CheckPlayerDistance();

	FTimerHandle DetectTimerHandle;
};

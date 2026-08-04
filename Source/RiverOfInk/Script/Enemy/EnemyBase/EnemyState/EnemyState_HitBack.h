// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "EnemyState_HitBack.generated.h"

/**
 * 击退状态：受击后沿攻击者反方向位移，
 * 持续 HitBackDuration 后回到 Chase
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UEnemyState_HitBack : public UStateBase
{
	GENERATED_BODY()

public:
	UEnemyState_HitBack();

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

private:
	void OnHitBackEnd();

	/** 击退方向（单位向量） */
	FVector HitBackDirection = FVector::ZeroVector;

	FTimerHandle HitBackTimerHandle;
};

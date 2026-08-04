// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "Core/GlobalStructs.h"
#include "EnemyState_Chase.generated.h"

/**
 * 追击状态：面向玩家 + 距离判定决定移动；
 * 受直接性伤害时切 HitBack
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UEnemyState_Chase : public UStateBase
{
	GENERATED_BODY()

public:
	UEnemyState_Chase();

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

	/** 受直接性伤害：切入击退状态 */
	UFUNCTION()
	void OnTakeDirectDamage(const FTakeDamageInfo& DamageInfo);

private:
	void CheckChaseDistance();
	void CheckAttackTimer();

	FTimerHandle ChaseTimerHandle;
	bool bShouldMove = true;
};

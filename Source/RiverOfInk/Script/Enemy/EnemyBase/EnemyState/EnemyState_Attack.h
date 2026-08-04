// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "Core/GlobalStructs.h"
#include "EnemyState_Attack.generated.h"

/**
 * 攻击状态：进入后 0.2s 执行攻击逻辑，再过 0.3s 回到 Chase；
 * 受直接性伤害时切 HitBack
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UEnemyState_Attack : public UStateBase
{
	GENERATED_BODY()

public:
	UEnemyState_Attack();

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

	/** 受直接性伤害：切入击退状态 */
	UFUNCTION()
	void OnTakeDirectDamage(const FTakeDamageInfo& DamageInfo);

private:
	void ExecuteAttack();
	void ReturnToChase();

	FRotator LockedRotation = FRotator::ZeroRotator;
	FTimerHandle AttackDelayHandle;
	FTimerHandle ReturnHandle;
	bool bAttackExecuted = false;
};

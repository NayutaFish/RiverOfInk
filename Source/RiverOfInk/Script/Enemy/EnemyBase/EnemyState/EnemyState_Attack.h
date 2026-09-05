// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "Core/GlobalStructs.h"
#include "EnemyState_Attack.generated.h"

/**
 * 攻击状态：进入后 0.2s 执行攻击逻辑，再过 0.3s 回到 Chase；
 * 硬值被击破时切 HitBack
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

/** 硬值被击破：切入击退状态 */
UFUNCTION()
void OnHardBreak(const FEnemyDamageResult& DamageResult);

/** 子类可覆盖以自定义攻击生成逻辑。 */
virtual void ExecuteAttack();

/** 攻击结束后返回追击/丢失目标状态。 */
virtual void ReturnToChase();

private:
FRotator LockedRotation = FRotator::ZeroRotator;
FTimerHandle AttackDelayHandle;
FTimerHandle ReturnHandle;
bool bAttackExecuted = false;
};
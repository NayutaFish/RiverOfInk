// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "Core/GlobalStructs.h"
#include "EnemyState_Charge.generated.h"

class AAttackAreaBase;

/**
 * 冲撞型敌人的白盒状态：Windup → Active → Recovery。
 *
 * Slice 4：蓄力、锁定方向、冲撞位移和跟随型近战攻击区域。
 * Slice 5：硬值击破打断、障碍/目标碰撞收束和恢复阶段。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UEnemyState_Charge : public UStateBase
{
	GENERATED_BODY()

public:
	UEnemyState_Charge();

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

	/** 冲撞期间硬值被击破：立即结束冲撞并转入 HitBack。 */
	UFUNCTION()
	void OnHardBreak(const FEnemyDamageResult& DamageResult);

private:
	void BeginCharge();
	void EndActiveCharge(const TCHAR* EndReason);
	void EndActiveChargeByDuration();
	void FinishRecovery();
	void EnterTargetLost();
	void ClearChargeAttackArea();

	FVector ChargeDirection = FVector::ForwardVector;
	bool bChargeStarted = false;
	bool bRecoveryStarted = false;

	UPROPERTY()
	TObjectPtr<AAttackAreaBase> ChargeAttackArea;

	FTimerHandle ChargeStartTimerHandle;
	FTimerHandle ChargeEndTimerHandle;
	FTimerHandle ChargeRecoveryTimerHandle;
};

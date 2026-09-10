// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Dash.generated.h"

class UPlayerInputComponent;

/**
 * 闪避状态：朝当前朝向冲刺，持续 ms
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UPlayerState_Dash : public UStateBase
{
	GENERATED_BODY()

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

	/** 冲刺无敌窗口时长（秒）；0 表示冲刺期间不给无敌。默认与冲刺持续时间（0.3s，硬编码）一致。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Invulnerability", meta = (ClampMin = "0.0", Units = "s"))
	float DashInvulnerabilityDuration = 0.3f;

	/** 冲刺持续时间（秒）。哈迪斯参考值：0.3s（其无敌帧为前 0.2s）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Feel", meta = (ClampMin = "0.02", Units = "s"))
	float DashDuration = 0.17f;

	/** 冲刺“起手”速度（cm/s），只在进入冲刺那一帧使用，比持续速度更快以获得爆发感。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Feel", meta = (ClampMin = "1.0"))
	float DashInitialSpeed = 5500.0f;

	/** 冲刺持续阶段速度（cm/s），冲刺期间每帧维持该速度以抵消摩擦减速。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Feel", meta = (ClampMin = "1.0"))
	float DashSpeed = 2800.0f;

	/** 冲刺途中按左键是否立刻取消冲刺转入普攻（哈迪斯式 dash-attack）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash|Cancel")
	bool bAllowDashAttackCancel = true;

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);
	void OnLmb();

	/** 缓存的输入组件（OnEnter 取一次；dash-attack 需要清掉缓冲记录） */
	UPROPERTY(Transient)
	TObjectPtr<UPlayerInputComponent> CachedInput;

	bool bHadMoveInput = false;
	FTimerHandle DashTimerHandle;
};

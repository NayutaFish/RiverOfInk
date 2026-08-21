// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "EnemyState_Charging.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 通用蓄力状态：用于冲撞、射弹发射、自爆等需要前摇的攻击。
 *
 * 进入状态时生成蓄力 Niagara 特效并启动倒计时；
 * 蓄力期间每帧同步特效与敌人宿主的位置；
 * 倒计时结束跳转到 TargetStateClass（任意 EnemyState 子类）；
 * 退出状态时确保特效完全销毁。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UEnemyState_Charging : public UStateBase
{
	GENERATED_BODY()

public:
	UEnemyState_Charging();

	/** 蓄力持续时间（秒），可在编辑器直接修改 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charging", meta = (ClampMin = "0.0", Units = "s"))
	float DurationTime = 1.0f;

	/** 蓄力结束后的目标状态类（任意 EnemyState 子类，从内容浏览器拖状态蓝图类赋值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charging")
	TSubclassOf<UStateBase> TargetStateClass;

	/** 蓄力特效；进入状态时生成，退出时销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charging")
	TObjectPtr<UNiagaraSystem> ChargeNiagaraSystem;

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

private:
	/** 倒计时结束：跳转到目标状态类 */
	void FinishCharging();

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ChargeNiagaraComponent;

	FTimerHandle ChargeFinishTimerHandle;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Attack2.generated.h"

/**
 * Attack2 状态：播放攻击动画，0.5s 后根据 WASD 输入切回 Idle 或 Move
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class TEST_GAMEPLAY_API UPlayerState_Attack2 : public UStateBase
{
	GENERATED_BODY()

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);
	void OnAttackTimer();

	/** 攻击期间移动速度（沿当前 WASD 输入方向） */
	UPROPERTY(EditAnywhere, Category = "AttackState", meta = (ClampMin = "0.0"))
	float AttackMoveSpeed = 500.0f;

	bool bHadMoveInput = false;
	float MoveInputX = 0.0f;
	float MoveInputY = 0.0f;
	FTimerHandle AttackTimerHandle;
};

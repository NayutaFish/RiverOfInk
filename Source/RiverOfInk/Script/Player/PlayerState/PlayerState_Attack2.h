// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Attack2.generated.h"

/**
 * Attack2 状态：播放攻击动画并发射球形弹幕，0.3s 后根据 WASD 输入切回 Idle 或 Move
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UPlayerState_Attack2 : public UStateBase
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
	/** 右键弹幕的飞行时间。 */
	UPROPERTY(EditAnywhere, Category = "AttackState|Projectile", meta = (ClampMin = "0.01", Units = "s"))
	float ProjectileLifeTime = 1.5f;

	/** 右键弹幕的移动速度。 */
	UPROPERTY(EditAnywhere, Category = "AttackState|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 900.0f;

	/** 弹幕从玩家中心向前的生成偏移。 */
	UPROPERTY(EditAnywhere, Category = "AttackState|Projectile", meta = (ClampMin = "0.0"))
	float ProjectileSpawnForwardOffset = 120.0f;

	/** 球形弹幕 Hitbox 半径。 */
	UPROPERTY(EditAnywhere, Category = "AttackState|Projectile", meta = (ClampMin = "5.0"))
	float ProjectileHitboxRadius = 50.0f;

	bool bHadMoveInput = false;
	float MoveInputX = 0.0f;
	float MoveInputY = 0.0f;
	FTimerHandle AttackTimerHandle;
};

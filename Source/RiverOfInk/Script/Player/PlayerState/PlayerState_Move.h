// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "Core/GlobalStructs.h"
#include "PlayerState_Move.generated.h"

/**
 * 移动状态：处理 WASD 移动和 Shift 疾跑，鼠标左键切换到攻击；
 * 受直接性伤害时切 HitBack
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UPlayerState_Move : public UStateBase
{
	GENERATED_BODY()

public:
	UPlayerState_Move();

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

	/** 受直接性伤害：切入击退状态 */
	UFUNCTION()
	void OnTakeDirectDamage(const FTakeDamageInfo& DamageInfo);

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);
	void OnShift(float Value);
	void OnLmb();
	void OnRmb();
	void OnSpace();
	void OnQ();
	void OnE();

	/**
	 * 轴向速度归零：某个轴没有输入时，立刻抹掉速度里沿该轴的分量。
	 * 开启后松开方向键（或同轴正反键同时按下，例如 A+D）该轴向速度直接归零，
	 * 不会沿旧方向继续滑行，换向也更利落；关闭则恢复“靠摩擦力减速”的旧手感。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move|Feel", meta = (AllowPrivateAccess = "true"))
	bool bCancelVelocityOnNeutralAxis = true;

	float LastInputTime = 0.0f;
	float LastShiftTime = 0.0f;

	/** 当前移动输入轴值，每帧在 Update 中统一应用，保证移动平滑。 */
	float CurrentMoveX = 0.0f;
	float CurrentMoveY = 0.0f;
};

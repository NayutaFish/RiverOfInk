// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Move.generated.h"

/**
 * 移动状态：处理 WASD 移动和 Shift 疾跑，鼠标左键切换到攻击
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

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);
	void OnShift(float Value);
	void OnLmb();
	void OnRmb();
	void OnSpace();
	void OnQ();

	float LastInputTime = 0.0f;
	float LastShiftTime = 0.0f;
};

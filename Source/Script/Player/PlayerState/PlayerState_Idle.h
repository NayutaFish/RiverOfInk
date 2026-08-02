// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Idle.generated.h"

/**
 * 待机状态：检测移动输入后切换到移动状态，鼠标左键切换到攻击
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class TEST_GAMEPLAY_API UPlayerState_Idle : public UStateBase
{
	GENERATED_BODY()

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;

private:
	void OnMoveInput(float Value);
	void OnLmb();
	void OnRmb();
	void OnQ();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Skill1.generated.h"

/**
 * 技能 1 状态：播放攻击动画 + 释放 1 技能
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class TEST_GAMEPLAY_API UPlayerState_Skill1 : public UStateBase
{
	GENERATED_BODY()

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);
	void OnSkillTimer();

	bool bHadMoveInput = false;
	FVector SlideDirection = FVector::ZeroVector;
	FTimerHandle SkillTimerHandle;
};

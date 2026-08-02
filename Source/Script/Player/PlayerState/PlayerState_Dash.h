// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Dash.generated.h"

/**
 * 闪避状态：朝当前朝向冲刺，持续 ms
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class TEST_GAMEPLAY_API UPlayerState_Dash : public UStateBase
{
	GENERATED_BODY()

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);

	bool bHadMoveInput = false;
	FTimerHandle DashTimerHandle;
};

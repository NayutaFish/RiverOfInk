// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Dash.generated.h"

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

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);

	bool bHadMoveInput = false;
	FTimerHandle DashTimerHandle;
};

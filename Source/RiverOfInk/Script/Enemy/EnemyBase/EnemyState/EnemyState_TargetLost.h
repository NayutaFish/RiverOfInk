// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "EnemyState_TargetLost.generated.h"

/**
 * TargetLost 状态：停止攻击，周期性尝试重新获取玩家。
 *
 * 目标暂时不存在不是死亡；状态保留在敌人身上，玩家重新生成后可回到
 * Idle，再由 Idle 的普通感知流程进入 Chase。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UEnemyState_TargetLost : public UStateBase
{
	GENERATED_BODY()

public:
	UEnemyState_TargetLost();

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;

private:
	void CheckForTarget();

	FTimerHandle TargetCheckTimerHandle;
};

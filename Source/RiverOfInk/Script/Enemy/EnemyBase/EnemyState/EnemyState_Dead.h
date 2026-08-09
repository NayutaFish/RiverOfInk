// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "EnemyState_Dead.generated.h"

/**
 * Dead 状态：只负责触发一次死亡收尾。
 *
 * 敌人本体仍拥有延迟销毁计时器；状态组件只负责把死亡生命周期
 * 与其他状态统一接入，避免在 TakeDamage/Die 中重复发放掉落。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UEnemyState_Dead : public UStateBase
{
	GENERATED_BODY()

protected:
	virtual void OnEnter_Implementation() override;
};

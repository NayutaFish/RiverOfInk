// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "PlayerCharacter_CommonAttackManage.generated.h"

class APlayerCharacter;
class UPlayerState_Attack1;

/**
 * 普通攻击多段管理组件。
 *
 * 玩家蓝图里可以挂载多个 UPlayerState_Attack1 组件，每个组件通过 attackStage
 * 区分段数（1 / 2 / 3 ...），并各自配置不同的攻击蒙太奇和 Niagara 特效。
 * 本组件负责在两次普攻之间按 maxAttackInterval 时间窗切换 attackStage，
 * 超过间隔后自动重置回第 1 段。
 *
 * 说明：C++ 没有 C# 的 partial class，因此用独立 ActorComponent 来拆分
 * PlayerCharacter 的普攻段数路由逻辑，避免 PlayerCharacter 脚本过长。
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UPlayerCharacter_CommonAttackManage : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerCharacter_CommonAttackManage();

	/** 玩家按下普攻时调用；由 PlayerCharacter / Idle / Move 状态转发到这里。 */
	UFUNCTION(BlueprintCallable, Category = "Player|CommonAttack")
	void RequestNormalAttack();

protected:
	virtual void BeginPlay() override;

private:
	void OnAttackIntervalExpired();

	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> OwnerPlayer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPlayerState_Attack1>> AttackStages;

	int32 CurrentAttackStageIndex = 0;
	double LastAttackTime = -1.0;
	FTimerHandle ResetTimerHandle;
};

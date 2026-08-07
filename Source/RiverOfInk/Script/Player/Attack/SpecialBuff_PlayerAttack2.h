// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/GlobalStructs.h"
#include "GameFramework/Actor.h"
#include "SpecialBuff_PlayerAttack2.generated.h"

class AEnemyBase;

/**
 * 特攻（Attack2）命中的特殊 Buff
 *
 * 由 AttackArea_PlayerAttack2 在命中敌人时生成，并调用 ApplyToEnemy
 * 将命中的敌人传入。每帧跟随敌人位置，订阅敌人受伤/死亡事件：
 *   - 敌人受伤 → 额外对其施加一次 BonusDamageInfo 伤害
 *   - 敌人死亡 → 销毁自身
 */
UCLASS(Blueprintable)
class RIVEROFINK_API ASpecialBuff_PlayerAttack2 : public AActor
{
	GENERATED_BODY()

public:
	ASpecialBuff_PlayerAttack2();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 应用到命中的敌人
	 * @param InEnemy 被 Attack2 命中的敌人
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff")
	void ApplyToEnemy(AEnemyBase* InEnemy);

	/** 当前绑定的敌人（蓝图可读） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buff")
	TObjectPtr<AEnemyBase> TargetEnemy;

	/** 敌人每次受伤时额外追加的伤害（可配置） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	FTakeDamageInfo BonusDamageInfo;

private:
	/** 敌人受伤事件订阅回调：追加一次伤害 */
	UFUNCTION()
	void HandleEnemyDamaged(const FTakeDamageInfo& DamageInfo);

	/** 敌人死亡事件订阅回调：销毁自身 */
	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadEnemy);

	/** 防重入标志：避免追加伤害再次触发受伤事件造成无限循环 */
	bool bApplyingBonusDamage = false;
};


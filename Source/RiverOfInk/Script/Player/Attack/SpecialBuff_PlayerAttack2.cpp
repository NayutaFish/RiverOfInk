// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Attack/SpecialBuff_PlayerAttack2.h"
#include "Enemy/EnemyBase/EnemyBase.h"

ASpecialBuff_PlayerAttack2::ASpecialBuff_PlayerAttack2()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASpecialBuff_PlayerAttack2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 每帧跟随敌人位置（敌人销毁/无效时不再移动）
	if (IsValid(TargetEnemy))
	{
		SetActorLocation(TargetEnemy->GetActorLocation());
	}
}

void ASpecialBuff_PlayerAttack2::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 取消订阅，避免悬挂引用
	if (IsValid(TargetEnemy))
	{
		TargetEnemy->OnTakeDirectDamage.RemoveDynamic(this, &ASpecialBuff_PlayerAttack2::HandleEnemyDamaged);
		TargetEnemy->OnEnemyDeath.RemoveDynamic(this, &ASpecialBuff_PlayerAttack2::HandleEnemyDeath);
	}

	Super::EndPlay(EndPlayReason);
}

void ASpecialBuff_PlayerAttack2::ApplyToEnemy(AEnemyBase* InEnemy)
{
	// 记录命中的敌人
	TargetEnemy = InEnemy;

	// 立即同步一次位置，避免生成后短暂停留在生成点
	if (IsValid(InEnemy))
	{
		SetActorLocation(InEnemy->GetActorLocation());

		// 订阅所跟随敌人的受伤/死亡事件
		InEnemy->OnTakeDirectDamage.AddDynamic(this, &ASpecialBuff_PlayerAttack2::HandleEnemyDamaged);
		InEnemy->OnEnemyDeath.AddDynamic(this, &ASpecialBuff_PlayerAttack2::HandleEnemyDeath);
	}
}

void ASpecialBuff_PlayerAttack2::HandleEnemyDamaged(const FTakeDamageInfo& DamageInfo)
{
	// 防重入：追加伤害会再次触发受伤事件，递归调用会无限循环
	if (bApplyingBonusDamage || !IsValid(TargetEnemy))
	{
		return;
	}

	bApplyingBonusDamage = true;

	// 敌人受伤时，额外施加一次配置好的伤害
	TargetEnemy->TakeDamage(BonusDamageInfo);

	bApplyingBonusDamage = false;

	// 追加伤害结算完毕后销毁自身（此刻不再处于事件广播链中，销毁安全）
	Destroy();
}

void ASpecialBuff_PlayerAttack2::HandleEnemyDeath(AActor* DeadEnemy)
{
	// 敌人死亡：销毁自身
	Destroy();
}

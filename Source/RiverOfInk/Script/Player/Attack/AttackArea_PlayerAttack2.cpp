// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Attack/AttackArea_PlayerAttack2.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/Attack/SpecialBuff_PlayerAttack2.h"

void AAttackArea_PlayerAttack2::ApplyDamage_Implementation(AActor* Target)
{
	// 先记录目标是否为敌人（伤害结算可能直接击杀敌人，之后无法再 Cast）
	AEnemyBase* Enemy = Cast<AEnemyBase>(Target);

	Super::ApplyDamage_Implementation(Target);

	// 命中敌人时通报特攻命中事件（供音效/特效/计数器等订阅）
	if (Enemy)
	{
		FEventBus::Publish<FPlayerSpecialAttackHitEvent>(FPlayerSpecialAttackHitEvent(Enemy));

		// 若敌人已被本次伤害击杀（已销毁），不生成 Buff，避免幽灵 Buff 留在原地
		if (!IsValid(Enemy))
		{
			return;
		}

		// 生成特殊 Buff 并应用到命中的敌人（未配置 SpecialBuffClass 时跳过）
		if (SpecialBuffClass)
		{
			if (ASpecialBuff_PlayerAttack2* Buff = GetWorld()->SpawnActor<ASpecialBuff_PlayerAttack2>(
					SpecialBuffClass, GetActorLocation(), GetActorRotation()))
			{
				Buff->ApplyToEnemy(Enemy);
			}
		}
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Attack/AttackArea_PlayerAttack2.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/Attack/SpecialBuff_PlayerAttack2.h"

void AAttackArea_PlayerAttack2::ApplyDamage_Implementation(AActor* Target)
{
	Super::ApplyDamage_Implementation(Target);

	// 命中敌人时通报特攻命中事件（供音效/特效/计数器等订阅）
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Target))
	{
		FEventBus::Publish<FPlayerSpecialAttackHitEvent>(FPlayerSpecialAttackHitEvent(Enemy));

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

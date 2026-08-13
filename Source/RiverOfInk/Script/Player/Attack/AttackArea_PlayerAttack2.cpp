// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Attack/AttackArea_PlayerAttack2.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/Attack/SpecialBuff_PlayerAttack2.h"

AAttackArea_PlayerAttack2::AAttackArea_PlayerAttack2()
{
	// Attack2 is a moving projectile. Its hitbox is the inherited sphere, not a player-centered fan.
	bUseFanHitbox = false;
	bIsMeleeAttack = false;
	bFollowTargetRotation = false;
	bDetectObstacle = true;
	LifeTime = 1.5f;
	Speed = 900.0f;
	Radius = 50.0f;
	bDrawDebugHitbox = true;
	DebugHitboxColor = FColor(255, 90, 220, 220);
}

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

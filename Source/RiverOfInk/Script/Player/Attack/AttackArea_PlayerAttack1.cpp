// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Attack/AttackArea_PlayerAttack1.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"

void AAttackArea_PlayerAttack1::ApplyDamage_Implementation(AActor* Target)
{
	Super::ApplyDamage_Implementation(Target);

	// 命中敌人时通报普攻命中事件（供音效/特效/计数器等订阅）
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Target))
	{
		FEventBus::Publish<FPlayerCommonAttackHitEvent>(FPlayerCommonAttackHitEvent(Enemy));
	}
}

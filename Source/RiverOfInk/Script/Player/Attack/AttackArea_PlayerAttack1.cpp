// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Attack/AttackArea_PlayerAttack1.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "FreezeFrameManager/FreezeFrameManager.h"
#include "RiverOfInk.h"

void AAttackArea_PlayerAttack1::ApplyDamage_Implementation(AActor* Target)
{
	Super::ApplyDamage_Implementation(Target);

	// 命中敌人时通报普攻命中事件（供音效/特效/计数器等订阅）
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Target))
	{
		if (HitStopDuration > 0.0f)
		{
			FFreezeFrameManager::Trigger(GetWorld(), HitStopDuration);
			UE_LOG(LogRiverOfInk, Log,
				TEXT("Player Attack1 hit-stop: Enemy=%s Duration=%.3f."),
				*Enemy->GetName(),
				HitStopDuration);
		}

		FEventBus::Publish<FPlayerCommonAttackHitEvent>(FPlayerCommonAttackHitEvent(Enemy));
	}
}

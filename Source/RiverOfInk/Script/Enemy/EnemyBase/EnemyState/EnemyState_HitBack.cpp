// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/EnemyState/EnemyState_HitBack.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Idle.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/World.h"
#include "RiverOfInk.h"

UEnemyState_HitBack::UEnemyState_HitBack()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyState_HitBack::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	UE_LOG(LogRiverOfInk, Log, TEXT("Enemy %s State: HitBack"), *Enemy->GetName());

	// 击退方向：攻击者 → 自身（水平方向），无攻击者时朝自身后方
	FVector KnockbackDir = -Enemy->GetActorForwardVector();
	if (AActor* Attacker = Enemy->LastAttacker)
	{
		FVector Delta = Enemy->GetActorLocation() - Attacker->GetActorLocation();
		Delta.Z = 0.0f;
		if (!Delta.IsNearlyZero())
		{
			KnockbackDir = Delta.GetSafeNormal();
		}
	}
	HitBackDirection = KnockbackDir;

	// 击退结束后回到追击
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HitBackTimerHandle, this,
			&UEnemyState_HitBack::OnHitBackEnd, Enemy->HitBackDuration, false);
	}
}

void UEnemyState_HitBack::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitBackTimerHandle);
	}
}

void UEnemyState_HitBack::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	// 持续按击退方向位移（Sweep 检测碰撞，撞墙停下）
	Enemy->AddActorWorldOffset(HitBackDirection * Enemy->HitBackSpeed * DeltaTime, true);
}

void UEnemyState_HitBack::OnHitBackEnd()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead) return;

	Enemy->RefreshCombatTarget();
	Enemy->SwitchState(Enemy->HasValidCombatTarget()
		? UEnemyState_Chase::StaticClass()
		: UEnemyState_TargetLost::StaticClass());
}

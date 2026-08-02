// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyState/EnemyState_Attack.h"
#include "Enemy/EnemyBase.h"
#include "HikariPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UEnemyState_Chase::UEnemyState_Chase()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyState_Chase::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	bShouldMove = true;

	// 每 0.2s 检测一次距离，决定是否该移动
	GetWorld()->GetTimerManager().SetTimer(ChaseTimerHandle, this,
		&UEnemyState_Chase::CheckChaseDistance, 0.2f, true, 0.0f);

	// 攻击间隔检测
	if (Enemy->AttackInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(Enemy->AttackTimerHandle, this,
			&UEnemyState_Chase::CheckAttackTimer, Enemy->AttackInterval, true, Enemy->AttackInterval);
	}
}

void UEnemyState_Chase::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	GetWorld()->GetTimerManager().ClearTimer(ChaseTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(Enemy->AttackTimerHandle);
}

void UEnemyState_Chase::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || !Enemy->CachedPlayer) return;

	FVector ToPlayer = Enemy->CachedPlayer->GetActorLocation() - Enemy->GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (ToPlayer.IsNearlyZero()) return;

	// 平滑旋转面向玩家
	FRotator TargetRot = ToPlayer.Rotation();
	FRotator NewRot = FMath::RInterpConstantTo(
		Enemy->GetActorRotation(), TargetRot, DeltaTime, Enemy->ChaseRotationSpeed);
	Enemy->SetActorRotation(FRotator(0.0f, NewRot.Yaw, 0.0f));

	// 平滑移动（开启 Sweep 检测碰撞，撞到障碍物停下）
	if (bShouldMove)
	{
		FVector MoveDelta = ToPlayer.GetSafeNormal() * Enemy->ChaseSpeed * DeltaTime;
		Enemy->AddActorWorldOffset(MoveDelta, true);
	}
}

void UEnemyState_Chase::CheckChaseDistance()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || !Enemy->CachedPlayer) return;

	FVector Delta = Enemy->CachedPlayer->GetActorLocation() - Enemy->GetActorLocation();
	Delta.Z = 0.0f;
	float Distance = Delta.Size();

	if (bShouldMove)
	{
		// 正在移动中 → 直到距离 <= 停止阈值才停
		if (Distance <= Enemy->ChaseStopRange)
		{
			bShouldMove = false;
		}
	}
	else
	{
		// 当前停着 → 直到距离 > 继续阈值才追
		if (Distance > Enemy->ChaseContinueRange)
		{
			bShouldMove = true;
		}
	}

	// 攻击计时器没在跑（冷却已过），且距离够近 → 立即攻击
	if (!GetWorld()->GetTimerManager().IsTimerActive(Enemy->AttackTimerHandle) && Distance < Enemy->AttackRange)
	{
		Enemy->SwitchState(UEnemyState_Attack::StaticClass());
	}
}

void UEnemyState_Chase::CheckAttackTimer()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || !Enemy->CachedPlayer || Enemy->bIsDead) return;

	FVector Delta = Enemy->CachedPlayer->GetActorLocation() - Enemy->GetActorLocation();
	Delta.Z = 0.0f;
	float Distance = Delta.Size();

	if (Distance < Enemy->AttackRange)
	{
		Enemy->SwitchState(UEnemyState_Attack::StaticClass());
	}
	else
	{
		// 距离太远，停止攻击计时，等距离合适时由 Chase 检查直接触发
		GetWorld()->GetTimerManager().ClearTimer(Enemy->AttackTimerHandle);
	}
}
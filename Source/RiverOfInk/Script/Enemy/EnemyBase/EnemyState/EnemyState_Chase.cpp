// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Charge.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Charging.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_HitBack.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Idle.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

namespace
{
	bool IsRangedEnemy(const AEnemyBase* Enemy)
	{
		return Enemy
			&& !Enemy->bAttackAreaIsMelee
			&& Enemy->AttackAreaSpeed > KINDA_SMALL_NUMBER;
	}

	float GetMaximumAttackRange(const AEnemyBase* Enemy)
	{
		if (!Enemy)
		{
			return 0.0f;
		}

		return Enemy->MaximumAttackRange > KINDA_SMALL_NUMBER
			? Enemy->MaximumAttackRange
			: Enemy->AttackRange;
	}

	bool IsWithinAttackRange(const AEnemyBase* Enemy, float Distance)
	{
		if (!Enemy)
		{
			return false;
		}

		if (!IsRangedEnemy(Enemy))
		{
			return Distance < Enemy->AttackRange;
		}

		const float MaximumRange = GetMaximumAttackRange(Enemy);
		const float MinimumRange = FMath::Min(Enemy->MinimumAttackRange, MaximumRange);
		return Distance >= MinimumRange && Distance <= MaximumRange;
	}
}

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
	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	// 仅在硬值被击破时切 HitBack；普通受击不会改变状态。
	Enemy->OnHardBreak.AddDynamic(this, &UEnemyState_Chase::OnHardBreak);

	// 每 0.2s 检测一次距离，决定是否该移动
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ChaseTimerHandle, this,
			&UEnemyState_Chase::CheckChaseDistance, 0.2f, true, 0.0f);
	}

	// 攻击间隔检测
	if (Enemy->AttackInterval > 0.0f && GetWorld())
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

	// 取消订阅
	Enemy->OnHardBreak.RemoveDynamic(this, &UEnemyState_Chase::OnHardBreak);

	GetWorld()->GetTimerManager().ClearTimer(ChaseTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(Enemy->AttackTimerHandle);
}

void UEnemyState_Chase::OnHardBreak(const FEnemyDamageResult& DamageResult)
{
	(void)DamageResult;
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead) return;

	Enemy->SwitchState(UEnemyState_HitBack::StaticClass());
}

void UEnemyState_Chase::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	FVector ToPlayer = Enemy->GetCombatTarget()->GetActorLocation() - Enemy->GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (ToPlayer.IsNearlyZero()) return;
	const float Distance = ToPlayer.Size();

	// 平滑旋转面向玩家
	FRotator TargetRot = ToPlayer.Rotation();
	FRotator NewRot = FMath::RInterpConstantTo(
		Enemy->GetActorRotation(), TargetRot, DeltaTime, Enemy->ChaseRotationSpeed);
	Enemy->SetActorRotation(FRotator(0.0f, NewRot.Yaw, 0.0f));

	// 远程敌人在最小/最大攻击距离之间保持位置：超出外圈靠近，
	// 进入内圈后后退；近战敌人继续使用原有的 ChaseStop/Continue 滞回。
	if (IsRangedEnemy(Enemy))
	{
		const float MaximumRange = GetMaximumAttackRange(Enemy);
		const float MinimumRange = FMath::Min(Enemy->MinimumAttackRange, MaximumRange);
		bShouldMove = Distance < MinimumRange || Distance > MaximumRange;
		if (bShouldMove)
		{
			const FVector MoveDirection = Distance < MinimumRange
				? -ToPlayer.GetSafeNormal()
				: ToPlayer.GetSafeNormal();
			Enemy->AddActorWorldOffset(MoveDirection * Enemy->ChaseSpeed * DeltaTime, true);
		}
	}
	else if (bShouldMove)
	{
		// 平滑移动（开启 Sweep 检测碰撞，撞到障碍物停下）
		FVector MoveDelta = ToPlayer.GetSafeNormal() * Enemy->ChaseSpeed * DeltaTime;
		Enemy->AddActorWorldOffset(MoveDelta, true);
	}
}

void UEnemyState_Chase::CheckChaseDistance()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	FVector Delta = Enemy->GetCombatTarget()->GetActorLocation() - Enemy->GetActorLocation();
	Delta.Z = 0.0f;
	float Distance = Delta.Size();

	if (IsRangedEnemy(Enemy))
	{
		const float MaximumRange = GetMaximumAttackRange(Enemy);
		const float MinimumRange = FMath::Min(Enemy->MinimumAttackRange, MaximumRange);
		bShouldMove = Distance < MinimumRange || Distance > MaximumRange;

		if (UWorld* World = GetWorld())
		{
			if (!World->GetTimerManager().IsTimerActive(Enemy->AttackTimerHandle)
				&& IsWithinAttackRange(Enemy, Distance))
			{
				Enemy->SwitchState(UEnemyState_Charging::StaticClass());
			}
		}
		return;
	}

	if (Enemy->IsChargeAttackInRange(Distance))
	{
		Enemy->SwitchState(UEnemyState_Charge::StaticClass());
		return;
	}

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
	if (UWorld* World = GetWorld(); World
		&& !World->GetTimerManager().IsTimerActive(Enemy->AttackTimerHandle))
	{
		if (Enemy->IsChargeAttackInRange(Distance))
		{
			Enemy->SwitchState(UEnemyState_Charge::StaticClass());
		}
		else if (IsWithinAttackRange(Enemy, Distance))
		{
			Enemy->SwitchState(UEnemyState_Charging::StaticClass());
		}
	}
}

void UEnemyState_Chase::CheckAttackTimer()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead) return;

	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	FVector Delta = Enemy->GetCombatTarget()->GetActorLocation() - Enemy->GetActorLocation();
	Delta.Z = 0.0f;
	float Distance = Delta.Size();

	if (Enemy->IsChargeAttackInRange(Distance))
	{
		Enemy->SwitchState(UEnemyState_Charge::StaticClass());
	}
	else if (IsWithinAttackRange(Enemy, Distance))
	{
		Enemy->SwitchState(UEnemyState_Charging::StaticClass());
	}
	else
	{
		// 距离太远，停止攻击计时，等距离合适时由 Chase 检查直接触发
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Enemy->AttackTimerHandle);
		}
	}
}

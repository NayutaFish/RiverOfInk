// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_HitBack.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Idle.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"
#include "Common/AttackAreaBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UEnemyState_Attack::UEnemyState_Attack()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyState_Attack::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	// Attack is re-entered after every recovery. Reset this phase flag so a
	// later attack cannot move during its wind-up before ExecuteAttack runs.
	bAttackExecuted = false;
	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	// 仅在硬值被击破时切 HitBack；普通受击不会取消攻击。
	Enemy->OnHardBreak.AddDynamic(this, &UEnemyState_Attack::OnHardBreak);

	// 锁定面向目标的朝向，攻击期间不旋转；远程攻击因此会沿目标方向发射。
	LockedRotation = Enemy->GetActorRotation();
	if (APlayerCharacter* Target = Enemy->GetCombatTarget())
	{
		FVector ToTarget = Target->GetActorLocation() - Enemy->GetActorLocation();
		ToTarget.Z = 0.0f;
		if (!ToTarget.IsNearlyZero())
		{
			LockedRotation = ToTarget.Rotation();
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AttackDelayHandle, this,
			&UEnemyState_Attack::ExecuteAttack, Enemy->AttackWindupTime, false);
	}
}

void UEnemyState_Attack::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (Enemy)
	{
		Enemy->OnHardBreak.RemoveDynamic(this, &UEnemyState_Attack::OnHardBreak);
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AttackDelayHandle);
		GetWorld()->GetTimerManager().ClearTimer(ReturnHandle);
	}
}

void UEnemyState_Attack::OnHardBreak(const FEnemyDamageResult& DamageResult)
{
	(void)DamageResult;
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead) return;

	Enemy->SwitchState(UEnemyState_HitBack::StaticClass());
}
void UEnemyState_Attack::ExecuteAttack()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead) return;

	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	if (!Enemy->AttackAreaClass)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s attack skipped: AttackAreaClass is missing; returning to Chase."),
			*Enemy->GetName());
		ReturnToChase();
		return;
	}

	bAttackExecuted = true;

	const FVector AttackDirection = LockedRotation.Vector();
	const FVector SpawnLoc = Enemy->GetActorLocation() + AttackDirection * Enemy->AttackAreaSpawnOffset;
	AActor* FollowTarget = Enemy->bAttackAreaFollowOwner ? Enemy : nullptr;
	const FTransform SpawnTransform(LockedRotation, SpawnLoc);

	AAttackAreaBase* SpawnedAttackArea = nullptr;
	if (UWorld* World = GetWorld())
	{
		SpawnedAttackArea = World->SpawnActorDeferred<AAttackAreaBase>(
			Enemy->AttackAreaClass,
			SpawnTransform,
			Enemy,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}

	if (AAttackAreaBase* AttackArea = SpawnedAttackArea)
	{
		AttackArea->Initialize(Enemy->AttackAreaLifeTime, Enemy->AttackAreaSpeed,
			Enemy->bAttackAreaIsMelee, FollowTarget);
		AttackArea->bDamageOpponentOnly = true;
		AttackArea->bDetectObstacle = Enemy->bAttackAreaDetectObstacle;
		AttackArea->bIsEnemyProjectile = !Enemy->bAttackAreaIsMelee
			&& Enemy->AttackAreaSpeed > KINDA_SMALL_NUMBER;
		UGameplayStatics::FinishSpawningActor(AttackArea, SpawnTransform);
		if (AttackArea->bIsEnemyProjectile)
		{
			UE_LOG(LogRiverOfInk, Log, TEXT("Enemy projectile tagged for Null Ring: %s."), *AttackArea->GetName());
		}

		UE_LOG(LogRiverOfInk, Log,
			TEXT("Enemy %s attack executed: Style=%s Area=%s."),
			*Enemy->GetName(),
			Enemy->bAttackAreaIsMelee ? TEXT("Melee") : TEXT("Ranged"),
			*AttackArea->GetName());
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s attack spawn failed; returning to Chase."), *Enemy->GetName());
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReturnHandle, this,
			&UEnemyState_Attack::ReturnToChase, Enemy->AttackRecoveryTime, false);
	}
}

void UEnemyState_Attack::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	// 强制维持锁定朝向
	Enemy->SetActorRotation(LockedRotation);

	if (bAttackExecuted && Enemy->AttackMoveSpeed > 0.0f)
	{
		FVector Direction = LockedRotation.Vector();
		Direction.Z = 0.0f;
		if (!Direction.IsNearlyZero())
		{
			Enemy->AddActorWorldOffset(
				Direction.GetSafeNormal()
				* Enemy->GetEffectiveMoveSpeed(Enemy->AttackMoveSpeed)
				* DeltaTime,
				true);
		}
	}
}

void UEnemyState_Attack::ReturnToChase()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (Enemy)
	{
		Enemy->RefreshCombatTarget();
		Enemy->SwitchState(Enemy->HasValidCombatTarget()
			? UEnemyState_Chase::StaticClass()
			: UEnemyState_TargetLost::StaticClass());
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_HitBack.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "RiverOfInk.h"
#include "Common/AttackAreaBase.h"
#include "Engine/World.h"

UEnemyState_Attack::UEnemyState_Attack()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyState_Attack::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	// 订阅直接性受击事件，受击时切 HitBack
	Enemy->OnTakeDirectDamage.AddDynamic(this, &UEnemyState_Attack::OnTakeDirectDamage);

	// 锁定当前朝向，攻击期间不旋转
	LockedRotation = Enemy->GetActorRotation();

	GetWorld()->GetTimerManager().SetTimer(AttackDelayHandle, this,
		&UEnemyState_Attack::ExecuteAttack, Enemy->AttackWindupTime, false);
}

void UEnemyState_Attack::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (Enemy)
	{
		Enemy->OnTakeDirectDamage.RemoveDynamic(this, &UEnemyState_Attack::OnTakeDirectDamage);
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AttackDelayHandle);
		GetWorld()->GetTimerManager().ClearTimer(ReturnHandle);
	}
}

void UEnemyState_Attack::OnTakeDirectDamage(const FTakeDamageInfo& DamageInfo)
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead) return;

	Enemy->SwitchState(UEnemyState_HitBack::StaticClass());
}
void UEnemyState_Attack::ExecuteAttack()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead || !Enemy->AttackAreaClass) return;

	bAttackExecuted = true;

	FActorSpawnParameters Params;
	Params.Owner = Enemy;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FVector SpawnLoc = Enemy->GetActorLocation() + Enemy->GetActorForwardVector() * Enemy->AttackAreaSpawnOffset;
	AActor* FollowTarget = Enemy->bAttackAreaFollowOwner ? Enemy : nullptr;

	if (AAttackAreaBase* AttackArea = GetWorld()->SpawnActor<AAttackAreaBase>(
			Enemy->AttackAreaClass,
			SpawnLoc,
			Enemy->GetActorRotation(),
			Params))
	{
		AttackArea->Initialize(Enemy->AttackAreaLifeTime, Enemy->AttackAreaSpeed,
			Enemy->bAttackAreaIsMelee, FollowTarget);
		AttackArea->bDamageOpponentOnly = true;
		AttackArea->bDetectObstacle = Enemy->bAttackAreaDetectObstacle;
		AttackArea->bIsEnemyProjectile = !Enemy->bAttackAreaIsMelee
			&& Enemy->AttackAreaSpeed > KINDA_SMALL_NUMBER;
		if (AttackArea->bIsEnemyProjectile)
		{
			UE_LOG(LogRiverOfInk, Log, TEXT("Enemy projectile tagged for Null Ring: %s."), *AttackArea->GetName());
		}
	}

	GetWorld()->GetTimerManager().SetTimer(ReturnHandle, this,
		&UEnemyState_Attack::ReturnToChase, Enemy->AttackRecoveryTime, false);
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
			Enemy->AddActorWorldOffset(Direction.GetSafeNormal() * Enemy->AttackMoveSpeed * DeltaTime, true);
		}
	}
}

void UEnemyState_Attack::ReturnToChase()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (Enemy)
	{
		Enemy->SwitchState(UEnemyState_Chase::StaticClass());
	}
}

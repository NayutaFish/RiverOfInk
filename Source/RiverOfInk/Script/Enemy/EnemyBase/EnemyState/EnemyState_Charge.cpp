// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Charge.h"

#include "Common/AttackAreaBase.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_HitBack.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "Engine/World.h"
#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"
#include "TimerManager.h"

UEnemyState_Charge::UEnemyState_Charge()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyState_Charge::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy)
	{
		return;
	}

	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		EnterTargetLost();
		return;
	}

	Enemy->OnHardBreak.AddDynamic(this, &UEnemyState_Charge::OnHardBreak);
	Enemy->GetWorldTimerManager().ClearTimer(Enemy->AttackTimerHandle);
	ClearChargeAttackArea();
	bChargeStarted = false;
	bRecoveryStarted = false;

	FVector ToTarget = Enemy->GetCombatTarget()->GetActorLocation() - Enemy->GetActorLocation();
	ToTarget.Z = 0.0f;
	ChargeDirection = ToTarget.IsNearlyZero()
		? Enemy->GetActorForwardVector().GetSafeNormal2D()
		: ToTarget.GetSafeNormal();
	if (ChargeDirection.IsNearlyZero())
	{
		ChargeDirection = FVector::ForwardVector;
	}

	Enemy->SetActorRotation(ChargeDirection.Rotation());

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Enemy %s charge windup: Duration=%.2f Speed=%.1f Direction=(%.2f,%.2f)."),
		*Enemy->GetName(),
		Enemy->ChargeWindupTime,
		Enemy->ChargeSpeed,
		ChargeDirection.X,
		ChargeDirection.Y);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChargeStartTimerHandle,
			this,
			&UEnemyState_Charge::BeginCharge,
			FMath::Max(0.0f, Enemy->ChargeWindupTime),
			false);
	}
}

void UEnemyState_Charge::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (Enemy)
	{
		Enemy->OnHardBreak.RemoveDynamic(this, &UEnemyState_Charge::OnHardBreak);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeStartTimerHandle);
		World->GetTimerManager().ClearTimer(ChargeEndTimerHandle);
		World->GetTimerManager().ClearTimer(ChargeRecoveryTimerHandle);
	}

	ClearChargeAttackArea();
	bChargeStarted = false;
	bRecoveryStarted = false;
}

void UEnemyState_Charge::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || !bChargeStarted || bRecoveryStarted)
	{
		return;
	}

	FHitResult Hit;
	Enemy->AddActorWorldOffset(
		ChargeDirection * Enemy->ChargeSpeed * DeltaTime,
		true,
		&Hit);

	if (Hit.bBlockingHit)
	{
		EndActiveCharge(TEXT("Blocked"));
	}
}

void UEnemyState_Charge::OnHardBreak(const FEnemyDamageResult& DamageResult)
{
	(void)DamageResult;

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead)
	{
		return;
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Enemy %s charge interrupted by hard break."),
		*Enemy->GetName());

	Enemy->SwitchState(UEnemyState_HitBack::StaticClass());
}

void UEnemyState_Charge::BeginCharge()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead)
	{
		return;
	}

	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		EnterTargetLost();
		return;
	}

	bChargeStarted = true;
	UE_LOG(LogRiverOfInk, Log,
		TEXT("Enemy %s charge started: Speed=%.1f Duration=%.2f."),
		*Enemy->GetName(),
		Enemy->ChargeSpeed,
		Enemy->ChargeDuration);

	if (Enemy->AttackAreaClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = Enemy;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FVector SpawnLocation = Enemy->GetActorLocation()
			+ ChargeDirection * Enemy->AttackAreaSpawnOffset;
		if (UWorld* World = GetWorld())
		{
			ChargeAttackArea = World->SpawnActor<AAttackAreaBase>(
				Enemy->AttackAreaClass,
				SpawnLocation,
				ChargeDirection.Rotation(),
				Params);
		}

		if (ChargeAttackArea)
		{
			ChargeAttackArea->Initialize(
				FMath::Max(0.01f, Enemy->ChargeDuration),
				0.0f,
				true,
				Enemy);
			ChargeAttackArea->bDamageOpponentOnly = true;
			ChargeAttackArea->bDetectObstacle = false;
			ChargeAttackArea->bIsEnemyProjectile = false;
			UE_LOG(LogRiverOfInk, Log,
				TEXT("Enemy %s charge hitbox spawned: %s."),
				*Enemy->GetName(),
				*ChargeAttackArea->GetName());
		}
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s charge has no AttackAreaClass; movement will be non-damaging."),
			*Enemy->GetName());
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChargeEndTimerHandle,
			this,
			&UEnemyState_Charge::EndActiveChargeByDuration,
			FMath::Max(0.0f, Enemy->ChargeDuration),
			false);
	}
}

void UEnemyState_Charge::EndActiveCharge(const TCHAR* EndReason)
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || !bChargeStarted || bRecoveryStarted)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeEndTimerHandle);
	}

	bChargeStarted = false;
	bRecoveryStarted = true;
	ClearChargeAttackArea();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Enemy %s charge ended: Reason=%s; recovery=%.2f."),
		*Enemy->GetName(),
		EndReason ? EndReason : TEXT("Unknown"),
		Enemy->ChargeRecoveryTime);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChargeRecoveryTimerHandle,
			this,
			&UEnemyState_Charge::FinishRecovery,
			FMath::Max(0.0f, Enemy->ChargeRecoveryTime),
			false);
	}
}

void UEnemyState_Charge::EndActiveChargeByDuration()
{
	EndActiveCharge(TEXT("Duration"));
}

void UEnemyState_Charge::FinishRecovery()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead)
	{
		return;
	}

	Enemy->RefreshCombatTarget();
	UE_LOG(LogRiverOfInk, Log,
		TEXT("Enemy %s charge recovery complete."),
		*Enemy->GetName());

	Enemy->SwitchState(Enemy->HasValidCombatTarget()
		? UEnemyState_Chase::StaticClass()
		: UEnemyState_TargetLost::StaticClass());
}

void UEnemyState_Charge::EnterTargetLost()
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
	}
}

void UEnemyState_Charge::ClearChargeAttackArea()
{
	if (IsValid(ChargeAttackArea.Get()))
	{
		ChargeAttackArea->Destroy();
	}
	ChargeAttackArea = nullptr;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"

#include "Enemy/EnemyBase/EnemyBase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Idle.h"
#include "Engine/World.h"
#include "RiverOfInk.h"
#include "TimerManager.h"

UEnemyState_TargetLost::UEnemyState_TargetLost()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyState_TargetLost::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Enemy->AttackTimerHandle);
		World->GetTimerManager().SetTimer(
			TargetCheckTimerHandle,
			this,
			&UEnemyState_TargetLost::CheckForTarget,
			0.25f,
			true,
			0.0f);
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Enemy %s entered TargetLost; waiting for a valid combat target."),
		*Enemy->GetName());
}

void UEnemyState_TargetLost::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TargetCheckTimerHandle);
	}
}

void UEnemyState_TargetLost::CheckForTarget()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead)
	{
		return;
	}

	Enemy->RefreshCombatTarget();
	if (Enemy->HasValidCombatTarget())
	{
		UE_LOG(LogRiverOfInk, Log,
			TEXT("Enemy %s reacquired combat target; returning to Idle."),
			*Enemy->GetName());
		Enemy->SwitchState(UEnemyState_Idle::StaticClass());
	}
}

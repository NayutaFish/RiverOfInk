// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/LanternGhost/State/LanternGhostState_Suicide.h"

#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

ULanternGhostState_Suicide::ULanternGhostState_Suicide()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULanternGhostState_Suicide::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DetonateTimerHandle,
			this,
			&ULanternGhostState_Suicide::Detonate,
			0.1f,
			false);
	}
}

void ULanternGhostState_Suicide::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetonateTimerHandle);
	}
}

void ULanternGhostState_Suicide::Detonate()
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		Enemy->Die();
	}
}

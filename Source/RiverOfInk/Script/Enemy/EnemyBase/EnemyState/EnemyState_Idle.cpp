// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Idle.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_HitBack.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"
#include "Engine/World.h"

UEnemyState_Idle::UEnemyState_Idle()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyState_Idle::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyState_Idle::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	Enemy->RefreshCombatTarget();

	// 订阅直接性受击事件，受击时切 HitBack
	Enemy->OnTakeDirectDamage.AddDynamic(this, &UEnemyState_Idle::OnTakeDirectDamage);

	// 每 0.5s 检测一次玩家距离
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DetectTimerHandle, this,
			&UEnemyState_Idle::CheckPlayerDistance, 0.5f, true, 0.0f);
	}
}

void UEnemyState_Idle::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	// 取消订阅
	Enemy->OnTakeDirectDamage.RemoveDynamic(this, &UEnemyState_Idle::OnTakeDirectDamage);

	// 停止距离检测
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetectTimerHandle);
	}
}

void UEnemyState_Idle::OnTakeDirectDamage(const FTakeDamageInfo& DamageInfo)
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead) return;

	Enemy->SwitchState(UEnemyState_HitBack::StaticClass());
}

void UEnemyState_Idle::CheckPlayerDistance()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget()) return;

	APlayerCharacter* Player = Enemy->GetCombatTarget();

	// 计算 XY 平面距离
	FVector EnemyLoc = Enemy->GetActorLocation();
	FVector PlayerLoc = Player->GetActorLocation();
	FVector Delta = PlayerLoc - EnemyLoc;
	Delta.Z = 0.0f;

	float Distance = Delta.Size();
	if (Distance < Enemy->DetectRange)
	{
		// 切换到 Chase 状态
		Enemy->SwitchState(UEnemyState_Chase::StaticClass());
	}
}

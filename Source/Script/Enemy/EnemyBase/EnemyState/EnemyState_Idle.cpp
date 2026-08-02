// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyState/EnemyState_Idle.h"
#include "Enemy/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase.h"
#include "HikariPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UEnemyState_Idle::UEnemyState_Idle()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyState_Idle::BeginPlay()
{
	Super::BeginPlay();

	// 尝试获取玩家引用
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (Enemy)
	{
		Enemy->CachedPlayer = Cast<AHikariPlayerCharacter>(
			UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	// 每 0.5s 检测一次玩家距离
	GetWorld()->GetTimerManager().SetTimer(DetectTimerHandle, this,
		&UEnemyState_Idle::CheckPlayerDistance, 0.5f, true, 0.0f);
}

void UEnemyState_Idle::CheckPlayerDistance()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || !Enemy->CachedPlayer) return;

	// 计算 XY 平面距离
	FVector EnemyLoc = Enemy->GetActorLocation();
	FVector PlayerLoc = Enemy->CachedPlayer->GetActorLocation();
	FVector Delta = PlayerLoc - EnemyLoc;
	Delta.Z = 0.0f;

	float Distance = Delta.Size();
	if (Distance < Enemy->DetectRange)
	{
		// 切换到 Chase 状态
		Enemy->SwitchState(UEnemyState_Chase::StaticClass());
	}
}
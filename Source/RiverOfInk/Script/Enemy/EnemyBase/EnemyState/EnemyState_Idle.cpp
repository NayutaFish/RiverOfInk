// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Idle.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_HitBack.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"
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
		Enemy->CachedPlayer = Cast<APlayerCharacter>(
			UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	// 兜底：即使状态机未进入 OnEnter，也保证初始 AI 检测启动
	GetWorld()->GetTimerManager().SetTimer(DetectTimerHandle, this,
		&UEnemyState_Idle::CheckPlayerDistance, 0.5f, true, 0.0f);
}

void UEnemyState_Idle::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	// 订阅直接性受击事件，受击时切 HitBack
	Enemy->OnTakeDirectDamage.AddDynamic(this, &UEnemyState_Idle::OnTakeDirectDamage);

	// 每 0.5s 检测一次玩家距离
	GetWorld()->GetTimerManager().SetTimer(DetectTimerHandle, this,
		&UEnemyState_Idle::CheckPlayerDistance, 0.5f, true, 0.0f);
}

void UEnemyState_Idle::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy) return;

	// 取消订阅
	Enemy->OnTakeDirectDamage.RemoveDynamic(this, &UEnemyState_Idle::OnTakeDirectDamage);

	// 停止距离检测
	GetWorld()->GetTimerManager().ClearTimer(DetectTimerHandle);
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
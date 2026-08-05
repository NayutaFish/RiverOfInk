// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_HitBack.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

UPlayerState_HitBack::UPlayerState_HitBack()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPlayerState_HitBack::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 清空残留速度，避免影响击退位移
	Player->GetCharacterMovement()->StopMovementImmediately();

	// 击退方向：攻击者 → 自身（水平方向），无攻击者时朝自身后方
	FVector KnockbackDir = -Player->GetActorForwardVector();
	if (AActor* Attacker = Player->LastAttacker)
	{
		FVector Delta = Player->GetActorLocation() - Attacker->GetActorLocation();
		Delta.Z = 0.0f;
		if (!Delta.IsNearlyZero())
		{
			KnockbackDir = Delta.GetSafeNormal();
		}
	}
	HitBackDirection = KnockbackDir;

	// 击退结束后回到 Idle（Idle 会按输入自动切 Move）
	GetWorld()->GetTimerManager().SetTimer(HitBackTimerHandle, this,
		&UPlayerState_HitBack::OnHitBackEnd, Player->HitBackDuration, false);
}

void UPlayerState_HitBack::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitBackTimerHandle);
	}
}

void UPlayerState_HitBack::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 持续按击退方向位移（Sweep 检测碰撞，撞墙停下）
	Player->AddActorWorldOffset(HitBackDirection * Player->HitBackSpeed * DeltaTime, true);
}

void UPlayerState_HitBack::OnHitBackEnd()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || Player->IsDead()) return;

	Player->SwitchState(UPlayerState_Idle::StaticClass());
}

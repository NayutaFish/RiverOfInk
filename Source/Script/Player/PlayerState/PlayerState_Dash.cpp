// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerState/PlayerState_Dash.h"
#include "Test_GamePlay.h"
#include "PlayerState/PlayerState_Idle.h"
#include "PlayerState/PlayerState_Move.h"
#include "Input/PlayerInputComponent.h"
#include "HikariPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

void UPlayerState_Dash::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (!Player) return;

	// 标记闪避状态
	Player->bIsDashing = true;

	// 订阅 WASD 输入，跟踪退出时是否有移动
	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (Input)
	{
		Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Dash::OnMoveX);
		Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Dash::OnMoveY);
	}

	// 锁定朝向，速度 = 当前朝向 × n
	Player->GetCharacterMovement()->bOrientRotationToMovement = false;
	Player->GetCharacterMovement()->Velocity = Player->GetActorForwardVector() * 3500.0f;

	// ms 后检测退出
	bHadMoveInput = false;
	GetWorld()->GetTimerManager().SetTimer(DashTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
		if (!Player) return;

		if (bHadMoveInput)
		{
			Player->SwitchState(UPlayerState_Move::StaticClass());
		}
		else
		{
			Player->SwitchState(UPlayerState_Idle::StaticClass());
		}
	}), 0.12f, false);

	UE_LOG(LogTest_GamePlay, Log, TEXT("State: Dash"));
}

void UPlayerState_Dash::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (!Player) return;

	// 取消闪避标记
	Player->bIsDashing = false;

	// 冲刺冷却
	Player->StartDashCooldown(0.3f);

	// 恢复朝向跟随移动
	Player->GetCharacterMovement()->bOrientRotationToMovement = true;

	// 取消订阅
	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (Input)
	{
		Input->OnMoveXDelegate.RemoveAll(this);
		Input->OnMoveYDelegate.RemoveAll(this);
	}

	// 取消计时器
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DashTimerHandle);
	}
}

void UPlayerState_Dash::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (!Player) return;

	// 持续保持冲刺速度，抵消摩擦减速
	Player->GetCharacterMovement()->Velocity = Player->GetActorForwardVector() * 2300.0f;
}

void UPlayerState_Dash::OnMoveX(float Value)
{
	if (!FMath::IsNearlyZero(Value)) bHadMoveInput = true;
}

void UPlayerState_Dash::OnMoveY(float Value)
{
	if (!FMath::IsNearlyZero(Value)) bHadMoveInput = true;
}
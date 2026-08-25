// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_Dash.h"
#include "RiverOfInk.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Input/PlayerInputComponent.h"
#include "Player/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

void UPlayerState_Dash::OnEnter_Implementation()
{
	StateEnterSoundName = TEXT("PlayerDash");
	Super::OnEnter_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 通报进入冲刺事件（供音效/特效等订阅）
	FEventBus::Publish<FPlayerEnterDashEvent>(FPlayerEnterDashEvent());

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
	Player->GetCharacterMovement()->Velocity = Player->GetActorForwardVector() * 5500.0f;

	// ms 后检测退出
	bHadMoveInput = false;
	GetWorld()->GetTimerManager().SetTimer(DashTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
		if (!Player) return;

		if (bHadMoveInput)
		{
			Player->SwitchState(UPlayerState_Move::StaticClass());
		}
		else
		{
			Player->SwitchState(UPlayerState_Idle::StaticClass());
		}
	}), 0.15f, false);
}

void UPlayerState_Dash::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 通报退出冲刺事件（供音效/特效等订阅）
	FEventBus::Publish<FPlayerExitDashEvent>(FPlayerExitDashEvent());

	// 取消闪避标记
	Player->bIsDashing = false;

	// 冲刺冷却
	Player->StartDashCooldown();

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

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
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
// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_Skill1.h"
#include "RiverOfInk.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Input/PlayerInputComponent.h"
#include "Player/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/Skill/SkillComponent.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

void UPlayerState_Skill1::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 订阅 WASD 输入
	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (Input)
	{
		Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Skill1::OnMoveX);
		Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Skill1::OnMoveY);
	}

	// 旋转朝向鼠标方向（仅 Yaw）
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		FHitResult Hit;
		PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);
		if (Hit.bBlockingHit)
		{
			FVector ToTarget = Hit.Location - Player->GetActorLocation();
			ToTarget.Z = 0.0f;
			if (!ToTarget.IsNearlyZero())
			{
				Player->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
			}
		}
	}

	// 释放 1 技能（在动画之前，否则 CanStartAction 会拦截）
	Player->TryCastSkillSlot1();

	// 播放攻击动画（和 Attack1 同款）
	Player->BeginAttack();

	// 保存当前速度方向，持续滑行
	FVector CurrentVel = Player->GetCharacterMovement()->Velocity;
	SlideDirection = !CurrentVel.IsNearlyZero() ? CurrentVel.GetSafeNormal() : FVector::ZeroVector;

	// 0.3s 后检测退出
	bHadMoveInput = false;
	GetWorld()->GetTimerManager().SetTimer(SkillTimerHandle, this, &UPlayerState_Skill1::OnSkillTimer, 0.3f, false);
}

void UPlayerState_Skill1::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

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
		GetWorld()->GetTimerManager().ClearTimer(SkillTimerHandle);
	}
}

void UPlayerState_Skill1::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 持续滑行
	if (!SlideDirection.IsNearlyZero())
	{
		Player->GetCharacterMovement()->Velocity = SlideDirection * 200.0f;
	}
	else
	{
		Player->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
}

void UPlayerState_Skill1::OnMoveX(float Value)
{
	if (!FMath::IsNearlyZero(Value)) bHadMoveInput = true;
}

void UPlayerState_Skill1::OnMoveY(float Value)
{
	if (!FMath::IsNearlyZero(Value)) bHadMoveInput = true;
}

void UPlayerState_Skill1::OnSkillTimer()
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
}
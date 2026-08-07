// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_Attack2.h"
#include "RiverOfInk.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Input/PlayerInputComponent.h"
#include "Player/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Common/AttackAreaBase.h"
#include "Player/Attack/AttackArea_PlayerAttack2.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

void UPlayerState_Attack2::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 订阅 WASD 输入，跟踪是否有移动
	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (Input)
	{
		Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Attack2::OnMoveX);
		Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Attack2::OnMoveY);
	}

	// 播放攻击动画
	Player->BeginAttack();

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

	// 在面前生成攻击区域（射弹：非近战，带飞行速度，命中即销毁）
	if (Player->Attack2AreaClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Player;
		SpawnParams.Instigator = Player;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AAttackArea_PlayerAttack2* AttackArea = GetWorld()->SpawnActor<AAttackArea_PlayerAttack2>(
				Player->Attack2AreaClass,
				Player->GetActorLocation() + Player->GetActorForwardVector() * 100.0f,
				Player->GetActorRotation(),
				SpawnParams))
		{
		// 射弹：生命周期 0.5s，飞行速度 900，非近战（命中即销毁）。
		// 注意：不传 FollowTarget——射弹有独立飞行，跟随玩家会导致每帧被拉回原位
		AttackArea->Initialize(0.5f, 900.0f, false);
			AttackArea->bDetectObstacle = true;
		}
	}

	// 0.3s 后检测是否需要切换状态
	bHadMoveInput = false;
	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &UPlayerState_Attack2::OnAttackTimer, 0.3f, false);
}

void UPlayerState_Attack2::OnExit_Implementation()
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
		GetWorld()->GetTimerManager().ClearTimer(AttackTimerHandle);
	}

	// 退出攻击状态时启动攻击冷却
	Player->StartAttack2Cooldown();

	// 清零移动输入记忆，避免下次进入攻击状态残留旧方向产生速度
	bHadMoveInput = false;
	MoveInputX = 0.0f;
	MoveInputY = 0.0f;
}

void UPlayerState_Attack2::OnMoveX(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		bHadMoveInput = true;
		MoveInputX = Value;
	}
	else
	{
		MoveInputX = 0.0f;
	}
}

void UPlayerState_Attack2::OnMoveY(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		bHadMoveInput = true;
		MoveInputY = Value;
	}
	else
	{
		MoveInputY = 0.0f;
	}
}

void UPlayerState_Attack2::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 按当前 WASD 输入方向移动（与 Move 状态相同的方向映射）
	FVector MoveDir = FVector::ZeroVector;
	if (!FMath::IsNearlyZero(MoveInputX))
	{
		MoveDir += (FVector::RightVector - FVector::ForwardVector).GetSafeNormal() * MoveInputX;
	}
	if (!FMath::IsNearlyZero(MoveInputY))
	{
		MoveDir += (FVector::ForwardVector + FVector::RightVector).GetSafeNormal() * MoveInputY;
	}

	if (!MoveDir.IsNearlyZero())
	{
		Player->GetCharacterMovement()->Velocity = MoveDir.GetSafeNormal() * AttackMoveSpeed;
	}
	else
	{
		Player->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
}

void UPlayerState_Attack2::OnAttackTimer()
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

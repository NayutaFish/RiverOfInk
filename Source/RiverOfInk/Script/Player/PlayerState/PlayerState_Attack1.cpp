// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_Attack1.h"
#include "RiverOfInk.h"
#include "Player/PlayerState/PlayerState_Dash.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Input/PlayerInputComponent.h"
#include "Player/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Common/AttackAreaBase.h"
#include "Player/Attack/AttackArea_PlayerAttack1.h"
#include "Components/SphereComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

UPlayerState_Attack1::UPlayerState_Attack1()
{
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> CommonSlashVFX(
		TEXT("/Game/RawContent/VFX/NiagaraSystem/NS/CommonSlash/NS/NS_CommonSlash.NS_CommonSlash"));
	if (CommonSlashVFX.Succeeded())
	{
		AttackVFX = CommonSlashVFX.Object;
	}
}

void UPlayerState_Attack1::SetAttackStageCount(int32 InCount)
{
	countAttackStageCount = FMath::Max(0, InCount);
}

void UPlayerState_Attack1::ResetAttackStageCount()
{
	countAttackStageCount = 0;
}

void UPlayerState_Attack1::PlayAttackMontage()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}

	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerState_Attack1: AttackMontage is not set (attackStage=%d)."), attackStage);
		return;
	}

	// 由 PlayerCharacter::BeginAttack 统一处理动作状态、Montage 播放与结束回调。
	Player->BeginAttack(AttackMontage);
}

void UPlayerState_Attack1::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 保留实现：Montage 结束后的清理已由 PlayerCharacter::OnAttackMontageEnded 处理，
	// 这里仅作为状态侧的兜底，避免状态退出时残留 Attacking 状态。
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
	{
		Player->EndAttack();
	}
}

void UPlayerState_Attack1::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}

	if (UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>())
	{
		Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Attack1::OnMoveX);
		Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Attack1::OnMoveY);
		Input->OnLmbDelegate.AddUObject(this, &UPlayerState_Attack1::OnLmb);
		Input->OnSpaceDelegate.AddUObject(this, &UPlayerState_Attack1::OnSpace);
	}

	bHadMoveInput = false;
	bAttackQueued = false;
	AttackInputBufferAge = -1.0f;
	MoveInputX = 0.0f;
	MoveInputY = 0.0f;
	ComboStep = 1;
	CurrentPhase = EPlayerAttackPhase::Startup;

	FaceAttackDirection();
	StartAttackStep();
}

void UPlayerState_Attack1::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player)
	{
		if (UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>())
		{
			Input->OnMoveXDelegate.RemoveAll(this);
			Input->OnMoveYDelegate.RemoveAll(this);
			Input->OnLmbDelegate.RemoveAll(this);
			Input->OnSpaceDelegate.RemoveAll(this);
		}

		// 防止状态被受击等外部事件打断后残留 Attacking 动作状态。
		Player->EndAttack();
		Player->StartAttack1Cooldown();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
	}

	ClearActiveAttackArea();
	bHadMoveInput = false;
	bAttackQueued = false;
	AttackInputBufferAge = -1.0f;
	MoveInputX = 0.0f;
	MoveInputY = 0.0f;
	ComboStep = 1;
	CurrentPhase = EPlayerAttackPhase::Startup;
}

void UPlayerState_Attack1::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}

	if (bAttackQueued)
	{
		AttackInputBufferAge += DeltaTime;
		if (AttackInputBufferAge > AttackInputBufferWindow)
		{
			bAttackQueued = false;
			AttackInputBufferAge = -1.0f;
		}
	}

	FVector MoveDirection = FVector::ZeroVector;
	if (!FMath::IsNearlyZero(MoveInputX))
	{
		MoveDirection += (FVector::RightVector - FVector::ForwardVector).GetSafeNormal() * MoveInputX;
	}
	if (!FMath::IsNearlyZero(MoveInputY))
	{
		MoveDirection += (FVector::ForwardVector + FVector::RightVector).GetSafeNormal() * MoveInputY;
	}

	if (MoveDirection.IsNearlyZero())
	{
		Player->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		return;
	}

	float MoveSpeed = AttackRecoveryMoveSpeed;
	switch (CurrentPhase)
	{
	case EPlayerAttackPhase::Startup:
		MoveSpeed = AttackStartupMoveSpeed;
		break;
	case EPlayerAttackPhase::Active:
		MoveSpeed = AttackActiveMoveSpeed;
		break;
	case EPlayerAttackPhase::Recovery:
		MoveSpeed = AttackRecoveryMoveSpeed;
		break;
	default:
		break;
	}

	Player->GetCharacterMovement()->Velocity = MoveDirection.GetSafeNormal() * MoveSpeed;
}

void UPlayerState_Attack1::OnMoveX(float Value)
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

void UPlayerState_Attack1::OnMoveY(float Value)
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

void UPlayerState_Attack1::OnLmb()
{
// 多段普攻的段数切换由 PlayerCharacter 的普攻管理组件负责；
// 状态内不再缓存连段输入，避免与 attackStage 组件切换逻辑冲突。
UE_LOG(LogRiverOfInk, Verbose,
TEXT("Player Attack1 stage %d ignores in-state LMB (manager owns combo routing)."),
attackStage);
}

void UPlayerState_Attack1::OnSpace()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}

	if (!bAllowDashCancelInRecovery || CurrentPhase != EPlayerAttackPhase::Recovery)
	{
		UE_LOG(LogRiverOfInk, Verbose,
			TEXT("Player Attack1 dash cancel ignored: Phase=%s."),
			*UEnum::GetValueAsString(CurrentPhase));
		return;
	}

	if (!Player->bCanDash)
	{
		UE_LOG(LogRiverOfInk, Verbose, TEXT("Player Attack1 dash cancel ignored: DashCooldown."));
		return;
	}

	bAttackQueued = false;
	AttackInputBufferAge = -1.0f;
	Player->EndAttack();
	UE_LOG(LogRiverOfInk, Log, TEXT("Player Attack1 recovery canceled into Dash: Step=%d."), ComboStep);
	Player->SwitchState(UPlayerState_Dash::StaticClass());
}

void UPlayerState_Attack1::StartAttackStep()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}

	ClearActiveAttackArea();
	CurrentPhase = EPlayerAttackPhase::Startup;

	// 第一段使用普通进入；二段强制重播当前攻击 Montage。
	PlayAttackMontage();
	UE_LOG(LogRiverOfInk, Log,
		TEXT("Player Attack1 step started: Step=%d Startup=%.2f Active=%.2f Recovery=%.2f."),
		ComboStep,
		AttackStartupTime,
		AttackActiveTime,
		AttackRecoveryTime);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AttackTimerHandle,
			this,
			&UPlayerState_Attack1::BeginActivePhase,
			FMath::Max(0.0f, AttackStartupTime),
			false);
	}
}

void UPlayerState_Attack1::BeginActivePhase()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || Player->IsDead())
	{
		return;
	}

	CurrentPhase = EPlayerAttackPhase::Active;
	ClearActiveAttackArea();

	if (Player->AttackAreaClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Player;
		SpawnParams.Instigator = Player;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// 扇形以玩家为圆心；VFX 的前向偏移仍由 SpawnAttackVFX 单独控制。
		const FVector SpawnLocation = Player->GetActorLocation();
		ActiveAttackArea = GetWorld()->SpawnActor<AAttackArea_PlayerAttack1>(
			Player->AttackAreaClass,
			SpawnLocation,
			Player->GetActorRotation(),
			SpawnParams);

		if (ActiveAttackArea)
		{
			if (ComboStep > 1)
			{
				const float SecondHitboxRadius = ActiveAttackArea->Radius
					* FMath::Max(1.0f, ComboSecondHitboxRadiusMultiplier);
				ActiveAttackArea->Radius = SecondHitboxRadius;
				if (ActiveAttackArea->CollisionSphere)
				{
					ActiveAttackArea->CollisionSphere->SetSphereRadius(
						SecondHitboxRadius,
						ActiveAttackArea->HasActorBegunPlay());
				}

				ActiveAttackArea->DamageInfo.DamageValue *= ComboSecondDamageMultiplier;
				ActiveAttackArea->DamageInfo.HardDamageValue *= ComboSecondDamageMultiplier;
			}

			// 调试球体直接读取 CollisionSphere，确保观察到的描线与真实 Hitbox 一致。
			ActiveAttackArea->bDrawDebugHitbox = bDebugDrawHitbox;
			ActiveAttackArea->DebugHitboxColor = ComboStep > 1
				? SecondHitboxDebugColor
				: FirstHitboxDebugColor;
			ActiveAttackArea->DebugHitboxLineThickness = DebugHitboxLineThickness;

			ActiveAttackArea->Initialize(AttackActiveTime, 0.0f, true, Player);
		}
	}

	SpawnAttackVFX();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Player Attack1 active: Step=%d Hitbox=%s Radius=%.1f Duration=%.2f."),
		ComboStep,
		IsValid(ActiveAttackArea) ? *ActiveAttackArea->GetName() : TEXT("none"),
		IsValid(ActiveAttackArea) ? ActiveAttackArea->Radius : 0.0f,
		AttackActiveTime);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AttackTimerHandle,
			this,
			&UPlayerState_Attack1::BeginRecoveryPhase,
			FMath::Max(0.0f, AttackActiveTime),
			false);
	}
}

void UPlayerState_Attack1::BeginRecoveryPhase()
{
	CurrentPhase = EPlayerAttackPhase::Recovery;
	ClearActiveAttackArea();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AttackTimerHandle,
			this,
			&UPlayerState_Attack1::FinishAttackStep,
			FMath::Max(0.0f, AttackRecoveryTime),
			false);
	}
}

void UPlayerState_Attack1::FinishAttackStep()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}

	ClearActiveAttackArea();

Player->EndAttack();
UE_LOG(LogRiverOfInk, Log, TEXT("Player Attack1 stage %d finished."), attackStage);
SwitchAfterAttack();
}

void UPlayerState_Attack1::ClearActiveAttackArea()
{
	if (IsValid(ActiveAttackArea))
	{
		ActiveAttackArea->Destroy();
	}
	ActiveAttackArea = nullptr;
}

void UPlayerState_Attack1::SpawnAttackVFX()
{
	UWorld* World = GetWorld();
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!World || !Player)
	{
		return;
	}

	UNiagaraSystem* VFX = ComboStep > 1 && ComboSecondVFX
		? ComboSecondVFX
		: AttackVFX;
	if (!VFX)
	{
		UE_LOG(LogRiverOfInk, Verbose,
			TEXT("Player Attack1 VFX skipped: Step=%d has no Niagara asset."),
			ComboStep);
		return;
	}

	const bool bIsSecondStep = ComboStep > 1;
	const float VFXForwardOffset = bIsSecondStep
		? ComboSecondVFXForwardOffset
		: AttackVFXForwardOffset;
	const float VFXScale = FMath::Max(0.01f, bIsSecondStep
		? ComboSecondVFXScale
		: AttackVFXScale);
	const FVector SpawnLocation = Player->GetActorLocation()
		+ Player->GetActorForwardVector() * VFXForwardOffset;
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		VFX,
		SpawnLocation,
		Player->GetActorRotation(),
		FVector(VFXScale));

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Player Attack1 VFX spawned: Step=%d Asset=%s Scale=%.2f Offset=%.1f%s."),
		ComboStep,
		*VFX->GetName(),
		VFXScale,
		VFXForwardOffset,
		bIsSecondStep && !ComboSecondVFX ? TEXT(" (first-step placeholder)") : TEXT(""));
}

void UPlayerState_Attack1::FaceAttackDirection()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}

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
}

void UPlayerState_Attack1::SwitchAfterAttack()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return;
	}

	Player->SwitchState(bHadMoveInput
		? UPlayerState_Move::StaticClass()
		: UPlayerState_Idle::StaticClass());
}

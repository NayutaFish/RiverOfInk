// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_Attack2.h"
#include "RiverOfInk.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Input/PlayerInputComponent.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"
#include "Player/ProjectileTargetingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Common/CombatEffectTags.h"
#include "Common/AttackAreaBase.h"
#include "Player/Attack/AttackArea_PlayerAttack2.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

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

	// 生成沿玩家朝向移动的球形弹幕；Hitbox 与蓝图中的火球 VFX 由同一个 Actor 驱动。
	if (Player->Attack2AreaClass)
	{
		const FVector SpawnLocation = Player->GetActorLocation()
			+ Player->GetActorForwardVector() * ProjectileSpawnForwardOffset;
		const FTransform SpawnTransform(Player->GetActorRotation(), SpawnLocation);

		// Deferred spawn 让投射物在 BeginPlay 前完成速度、半径和形状配置，避免蓝图旧默认值抢先生效。
		if (AAttackArea_PlayerAttack2* AttackArea = GetWorld()->SpawnActorDeferred<AAttackArea_PlayerAttack2>(
				Player->Attack2AreaClass,
				SpawnTransform,
				Player,
				Player,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			FProjectileSpec ProjectileSpec;
			ProjectileSpec.LifeTime = ProjectileLifeTime;
			ProjectileSpec.ProjectileSpeed = ProjectileSpeed;
			ProjectileSpec.HomingTurnRate = ProjectileHomingTurnRate;
			AEnemyBase* HomingTarget = nullptr;
			FCombatEffectHandle HomingMarkHandle;
			if (UProjectileTargetingComponent* Targeting = Player->GetProjectileTargetingComponent())
			{
				if (Targeting->HasHomingBuild())
				{
					Targeting->GetCurrentMarkedTargetSnapshot(HomingTarget, HomingMarkHandle);
					ProjectileSpec.HomingTarget = HomingTarget;
					ProjectileSpec.HomingMarkHandle = HomingMarkHandle;
					ProjectileSpec.bEnableHoming = IsValid(HomingTarget)
						&& HomingMarkHandle.IsValid();
					if (ProjectileSpec.bEnableHoming)
					{
						ProjectileSpec.ProjectileTags.AddTag(RiverOfInkCombatEffectTags::Build_Projectile_Homing);
					}
				}
			}
			AttackArea->InitializeProjectile(ProjectileSpec);
			AttackArea->Radius = ProjectileHitboxRadius;
			AttackArea->CollisionSphere->SetSphereRadius(ProjectileHitboxRadius);
			// 运行时强制使用球形投射物，避免蓝图旧默认值恢复扇形/玩家跟随行为。
			AttackArea->bUseFanHitbox = false;
			AttackArea->bIsMeleeAttack = false;
			AttackArea->bFollowTargetRotation = false;
			AttackArea->bDetectObstacle = true;
			UGameplayStatics::FinishSpawningActor(AttackArea, SpawnTransform);
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

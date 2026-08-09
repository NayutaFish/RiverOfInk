// Fill out your copyright notice in the Description page of Project Settings.

#include "CameraManager/CameraManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ACameraManager::ACameraManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// ── 显式根组件，避免引擎自动挑选场景组件作为根 ──
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// ── 创建弹簧弓（Camera Boom），效果同旧项目 ──
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);   // 弹簧臂自身保持固定旋转（俯视角）
	CameraBoom->TargetArmLength = 1600.f;         // 相机距离目标 1600 单位（高度差约 800，原为 400）
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 45.f, 0.f)); // 俯视 60 度 + Yaw 45 度对齐 WASD 移动方向
	CameraBoom->bDoCollisionTest = false;

	// ── 创建摄像机 ──
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;  // 相机不随控制器转
}

void ACameraManager::BeginPlay()
{
	Super::BeginPlay();
	// 玩家可能比本管理器晚生成，由 Tick 每帧检测并接管
	CurrentFollowStrength = IntroFollowStrength;
}

void ACameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── 玩家生成时自动接管 ──
	if (!IsValid(TargetActor))
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			TargetActor = Pawn;

			// 进场：回到 Intro 强度，镜头从当前位置缓慢飘向玩家
			CurrentFollowStrength = IntroFollowStrength;

			// 只对齐 Z（仅一次），避免 GameMode 生成在原点导致镜头贴地；XY 由 Intro 强度自然过渡
			FVector StartLocation = GetActorLocation();
			StartLocation.Z = Pawn->GetActorLocation().Z;
			SetActorLocation(StartLocation);

			// 玩家身上没有相机组件，把渲染视角切换到本管理器
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				PC->SetViewTarget(this);
			}
		}
		return;
	}

	// 跟随强度从 Intro 平滑过渡到 Normal
	CurrentFollowStrength = FMath::FInterpTo(CurrentFollowStrength, NormalFollowStrength, DeltaTime, StrengthBlendSpeed);

	// 帧率无关平滑跟随（同旧项目思路：VLerp + Strength，越大越紧）
	const float Alpha = FMath::Clamp(CurrentFollowStrength * DeltaTime, 0.0f, 1.0f);
	FVector NewLocation = GetActorLocation() + (TargetActor->GetActorLocation() - GetActorLocation()) * Alpha;

	// 仅跟随 XY：保持本管理器当前的 Z
	if (bFollowOnlyXY)
	{
		NewLocation.Z = GetActorLocation().Z;
	}

	// 叠加相机震动偏移（非震动时为零向量，不影响正常跟随）
	NewLocation += CurrentShakeOffset;

	SetActorLocation(NewLocation);
}

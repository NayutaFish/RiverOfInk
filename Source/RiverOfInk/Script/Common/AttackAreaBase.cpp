// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/AttackAreaBase.h"
#include "RiverOfInk.h"
#include "Core/GlobalStructs.h"
#include "Core/Audio/AudioManager.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"
#include "Player/ProjectileTargetingComponent.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AAttackAreaBase::AAttackAreaBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// 碰撞根组件：Overlap 检测命中，无需额外配置碰撞通道
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(Radius);
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_GameTraceChannel1);    // DamageArea
	// 只与玩家/敌人胶囊体（Channel 2 / 3）Overlap，其余忽略
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap);    // EnemyHitbox
	CollisionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);    // PlayerHitbox
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AAttackAreaBase::OnCollisionOverlap);

	// 复用 UE 内置 WireframeMaterial，避免为调试功能新增项目资产。
	DebugHitboxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugHitboxWireframe"));
	DebugHitboxMesh->SetupAttachment(CollisionSphere);
	DebugHitboxMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugHitboxMesh->SetGenerateOverlapEvents(false);
	DebugHitboxMesh->SetCastShadow(false);
	DebugHitboxMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DebugSphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DebugSphereMesh.Succeeded())
	{
		DebugHitboxMesh->SetStaticMesh(DebugSphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WireframeMaterial(
		TEXT("/Engine/EngineDebugMaterials/WireframeMaterial.WireframeMaterial"));
	if (WireframeMaterial.Succeeded())
	{
		DebugHitboxMesh->SetMaterial(0, WireframeMaterial.Object);
	}
}

void AAttackAreaBase::BeginPlay()
{
	Super::BeginPlay();
	ElapsedTime = 0.0f;
	CollisionSphere->SetSphereRadius(Radius);
	UpdateDebugHitboxVisualization();
}

void AAttackAreaBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateHoming(DeltaTime);

	// 障碍物检测（射线扫描前方，不依赖碰撞系统）
	if (bDetectObstacle && Speed > 0.0f)
	{
		PerformObstacleScan(DeltaTime);
	}

	// 近战：持续检查重叠范围内是否仍有未结算目标（不会漏掉生成时已在范围内的目标）
	if (bIsMeleeAttack)
	{
		TArray<AActor*> Overlapping;
		CollisionSphere->GetOverlappingActors(Overlapping);
		for (AActor* Other : Overlapping)
		{
			if (!IsValid(Other) || HitActors.Contains(Other) || !IsValidTarget(Other))
			{
				continue;
			}

			HitActors.Add(Other);
			ApplyDamage(Other);
		}
	}

	ElapsedTime += DeltaTime;
	if (ElapsedTime >= LifeTime)
	{
		Disappear(EAttackAreaDisappearReason::Lifetime);
		return;
	}

	if (Speed > 0.0f)
	{
		AddActorWorldOffset(GetActorForwardVector() * Speed * DeltaTime);
	}

	// 跟随目标（保持初始相对偏移）
	if (FollowTarget)
	{
		SetActorLocation(FollowTarget->GetActorLocation() + FollowOffset);
		if (bFollowTargetRotation)
		{
			SetActorRotation(FollowTarget->GetActorRotation());
		}
	}

	UpdateDebugHitboxVisualization();
	if (bDrawDebugHitbox && GetWorld())
	{
		if (bUseFanHitbox)
		{
			DrawDebugFanHitbox();
		}
		else
		{
			DrawDebugSphere(
				GetWorld(),
				CollisionSphere->GetComponentLocation(),
				CollisionSphere->GetScaledSphereRadius(),
				FMath::Clamp(DebugHitboxSegments, 8, 64),
				DebugHitboxColor,
				false,
				0.0f,
				0,
				FMath::Max(0.1f, DebugHitboxLineThickness));
		}
	}
}

void AAttackAreaBase::UpdateDebugHitboxVisualization()
{
	if (!DebugHitboxMesh || !CollisionSphere)
	{
		return;
	}

	// BasicShapes/Sphere 的默认半径为 50 cm；按碰撞体的实际半径同步缩放。
	const float MeshScale = FMath::Max(0.01f, CollisionSphere->GetUnscaledSphereRadius() / 50.0f);
	DebugHitboxMesh->SetRelativeScale3D(FVector(MeshScale));
	DebugHitboxMesh->SetVisibility(bDrawDebugHitbox && !bUseFanHitbox, true);
}

void AAttackAreaBase::DrawDebugFanHitbox() const
{
	if (!GetWorld() || !CollisionSphere)
	{
		return;
	}

	const FVector Origin = CollisionSphere->GetComponentLocation() + FVector(0.0f, 0.0f, 3.0f);
	const float WorldRadius = CollisionSphere->GetScaledSphereRadius();
	const float HalfAngle = FMath::Clamp(FanHalfAngleDegrees, 0.0f, 180.0f);
	const int32 SegmentCount = FMath::Clamp(DebugHitboxSegments, 8, 64);
	const float AngleStep = (HalfAngle * 2.0f) / static_cast<float>(SegmentCount);

	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward.Normalize();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	FVector Previous = Origin + Forward.RotateAngleAxis(-HalfAngle, FVector::UpVector) * WorldRadius;
	for (int32 Index = 1; Index <= SegmentCount; ++Index)
	{
		const float Angle = -HalfAngle + AngleStep * static_cast<float>(Index);
		const FVector Current = Origin + Forward.RotateAngleAxis(Angle, FVector::UpVector) * WorldRadius;
		DrawDebugLine(
			GetWorld(),
			Previous,
			Current,
			DebugHitboxColor,
			false,
			0.0f,
			0,
			FMath::Max(0.1f, DebugHitboxLineThickness));
		Previous = Current;
	}

	const FVector LeftEdge = Origin + Forward.RotateAngleAxis(-HalfAngle, FVector::UpVector) * WorldRadius;
	const FVector RightEdge = Origin + Forward.RotateAngleAxis(HalfAngle, FVector::UpVector) * WorldRadius;
	DrawDebugLine(GetWorld(), Origin, LeftEdge, DebugHitboxColor, false, 0.0f, 0, FMath::Max(0.1f, DebugHitboxLineThickness));
	DrawDebugLine(GetWorld(), Origin, RightEdge, DebugHitboxColor, false, 0.0f, 0, FMath::Max(0.1f, DebugHitboxLineThickness));
}
void AAttackAreaBase::OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 远程攻击（子弹）：首次撞到目标或障碍即结算并销毁
	if (bIsMeleeAttack)
	{
		return;
	}

	if (!IsValid(OtherActor) || HitActors.Contains(OtherActor) || !IsValidTarget(OtherActor))
	{
		return;
	}

	HitActors.Add(OtherActor);
	ApplyDamage(OtherActor);
	Disappear(EAttackAreaDisappearReason::HitEnemy);
}

void AAttackAreaBase::PerformObstacleScan(float DeltaTime)
{
	FHitResult Hit;
	FVector Start = GetActorLocation();
	FVector End = Start + GetActorForwardVector() * Speed * DeltaTime * 2.0f;

	// 只检测 WorldStatic 对象，忽略 Character/Pawn
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());

	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjParams, QueryParams))
	{
		Disappear(EAttackAreaDisappearReason::HitObstacle);
	}
}

void AAttackAreaBase::Disappear(EAttackAreaDisappearReason Reason)
{
	switch (Reason)
	{
	case EAttackAreaDisappearReason::Lifetime:
		UE_LOG(LogRiverOfInk, Log, TEXT("AttackArea: Lifetime ended"));
		break;
	case EAttackAreaDisappearReason::HitEnemy:
		UE_LOG(LogRiverOfInk, Log, TEXT("AttackArea: Hit enemy"));
		break;
	case EAttackAreaDisappearReason::HitObstacle:
		UE_LOG(LogRiverOfInk, Log, TEXT("AttackArea: Hit obstacle"));
		break;
	}

	Destroy();
}

void AAttackAreaBase::Initialize(float InLifeTime, float InSpeed, bool InIsMeleeAttack, AActor* InFollowTarget)
{
	ProjectileSpec = FProjectileSpec();
	ProjectileSpec.LifeTime = InLifeTime;
	ProjectileSpec.ProjectileSpeed = InSpeed;
	LifeTime = InLifeTime;
	Speed = InSpeed;
	bIsMeleeAttack = InIsMeleeAttack;
	FollowTarget = InFollowTarget;

	// 生成后立即记录与跟随目标的相对偏移
	if (FollowTarget)
	{
		FollowOffset = GetActorLocation() - FollowTarget->GetActorLocation();
	}
}

void AAttackAreaBase::InitializeProjectile(const FProjectileSpec& InProjectileSpec)
{
	ProjectileSpec = InProjectileSpec;
	ProjectileSpec.LifeTime = FMath::Max(0.01f, InProjectileSpec.LifeTime);
	ProjectileSpec.ProjectileSpeed = FMath::Max(0.0f, InProjectileSpec.ProjectileSpeed);
	ProjectileSpec.HomingTurnRate = FMath::Max(0.0f, InProjectileSpec.HomingTurnRate);
	if (!ProjectileSpec.bEnableHoming)
	{
		ProjectileSpec.HomingTarget = nullptr;
	}

	LifeTime = ProjectileSpec.LifeTime;
	Speed = ProjectileSpec.ProjectileSpeed;
	bIsMeleeAttack = false;
	FollowTarget = nullptr;
	FollowOffset = FVector::ZeroVector;
}

void AAttackAreaBase::UpdateHoming(float DeltaTime)
{
	if (!ProjectileSpec.bEnableHoming || !ProjectileSpec.HomingTarget)
	{
		return;
	}

	AEnemyBase* Target = Cast<AEnemyBase>(ProjectileSpec.HomingTarget.Get());
	if (!IsValid(Target))
	{
		ProjectileSpec.bEnableHoming = false;
		ProjectileSpec.HomingTarget = nullptr;
		return;
	}

	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
	{
		UProjectileTargetingComponent* Targeting = Player->GetProjectileTargetingComponent();
		if (!Targeting || !Targeting->IsHomingMarkActive(Target))
		{
			ProjectileSpec.bEnableHoming = false;
			ProjectileSpec.HomingTarget = nullptr;
			return;
		}
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (ToTarget.IsNearlyZero() || ProjectileSpec.HomingTurnRate <= 0.0f)
	{
		return;
	}

	SetActorRotation(FMath::RInterpConstantTo(
		GetActorRotation(),
		ToTarget.Rotation(),
		DeltaTime,
		ProjectileSpec.HomingTurnRate));
}

bool AAttackAreaBase::NullifyEnemyProjectile()
{
	if (!bIsEnemyProjectile || IsActorBeingDestroyed())
	{
		return false;
	}

	// Disable the harmful overlap before Destroy() is processed at the end of
	// the frame. This keeps a projectile from dealing damage in the same frame
	// in which a Null Ring erases it.
	CollisionSphere->SetGenerateOverlapEvents(false);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorEnableCollision(false);
	UE_LOG(LogRiverOfInk, Log, TEXT("Enemy projectile nullified: %s."), *GetName());
	Destroy();
	return true;
}

void AAttackAreaBase::ApplyDamage_Implementation(AActor* Target)
{
	// 攻击者由代码填充（施放者），不依赖编辑器配置
	DamageInfo.Attacker = GetOwner();

	// 命中敌人时，在敌人位置生成命中特效，朝向=攻击者→受击者方向
	if (HitSpark && Target)
	{
		FVector SpawnLocation = Target->GetActorLocation();
		FVector Direction = Target->GetActorLocation() - GetActorLocation();
		Direction.Z = 0.0f;
		FRotator SpawnRotation = Direction.IsNearlyZero()
			? Target->GetActorRotation()
			: Direction.Rotation();

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), HitSpark, SpawnLocation, SpawnRotation);
	}

	// 播放命中音效（名称对应 AudioDataAsset 配置，未配置时静默跳过）
	if (!HitSoundName.IsEmpty())
	{
		FAudioManager::Play(HitSoundName);
	}

	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Target))
	{
		Enemy->TakeDamage(DamageInfo, this);
		if (ProjectileSpec.bEnableHoming
			&& IsValid(Enemy)
			&& !Enemy->bIsDead
			&& Enemy->LastDamageResult.ResolvedDamage.bDamageApplied)
		{
			if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
			{
				if (UProjectileTargetingComponent* Targeting = Player->GetProjectileTargetingComponent())
				{
					Targeting->NotifyProjectileHit(Enemy);
				}
			}
		}
	}
	else if (APlayerCharacter* Player = Cast<APlayerCharacter>(Target))
	{
		Player->TakeDamage(DamageInfo);
	}
}

bool AAttackAreaBase::IsTargetWithinFanHitbox(const AActor* Target) const
{
	if (!bUseFanHitbox)
	{
		return true;
	}

	if (!Target || !CollisionSphere)
	{
		return false;
	}

	const FVector Origin = CollisionSphere->GetComponentLocation();
	FVector ToTarget = Target->GetActorLocation() - Origin;
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return true;
	}

	if (ToTarget.SizeSquared() > FMath::Square(CollisionSphere->GetScaledSphereRadius()) + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward.Normalize();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	ToTarget.Normalize();
	const float HalfAngle = FMath::Clamp(FanHalfAngleDegrees, 0.0f, 180.0f);
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(HalfAngle));
	return FVector::DotProduct(Forward, ToTarget) >= MinimumDot - KINDA_SMALL_NUMBER;
}

bool AAttackAreaBase::IsValidTarget_Implementation(AActor* Target)
{
	// 如果目标是玩家且正在闪避，跳过
	if (APlayerCharacter* PlayerTarget = Cast<APlayerCharacter>(Target))
	{
		if (PlayerTarget->IsDashing()) return false;
	}

	bool bValidFaction = true;
	if (bDamageOpponentOnly)
	{
		if (GetOwner() && GetOwner()->IsA<APlayerCharacter>())
		{
			bValidFaction = Target->IsA<AEnemyBase>();
		}
		else
		{
			bValidFaction = Target->IsA<APlayerCharacter>();
		}
	}

	return bValidFaction && IsTargetWithinFanHitbox(Target);
}

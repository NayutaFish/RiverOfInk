// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/AttackAreaBase.h"
#include "RiverOfInk.h"
#include "Core/GlobalStructs.h"
#include "Engine/World.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"

AAttackAreaBase::AAttackAreaBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// 纯可视化根组件：不参与任何碰撞，检测全部走射线，无需配置碰撞通道
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	CollisionBox->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAttackAreaBase::BeginPlay()
{
	Super::BeginPlay();
	ElapsedTime = 0.0f;
}

void AAttackAreaBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 障碍物检测（射线扫描前方，不依赖碰撞系统）
	if (bDetectObstacle && Speed > 0.0f)
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
			return;
		}
	}

	// 目标检测：每帧沿正方向发射一小段射线
	PerformTargetScan(DeltaTime);

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
	}
}

void AAttackAreaBase::PerformTargetScan(float DeltaTime)
{
	// 检测距离：至少覆盖本帧位移（高速子弹不漏检），近战用 MinDetectRange 保证基础范围
	float ScanDistance = FMath::Max(Speed * DeltaTime, MinDetectRange);
	if (ScanDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector Start = GetActorLocation();
	FVector End = Start + GetActorForwardVector() * ScanDistance;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bReturnPhysicalMaterial = false;

	// 引擎内置 Pawn 通道即可命中玩家/敌人胶囊体，无需任何自定义碰撞配置
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, QueryParams))
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	if (!IsValid(HitActor) || HitActors.Contains(HitActor) || !IsValidTarget(HitActor))
	{
		return;
	}

	// 同一目标只结算一次伤害（近战攻击生命期内会持续扫描）
	HitActors.Add(HitActor);

	ApplyDamage(HitActor);

	// 远程攻击（子弹/射弹）打到目标后立即销毁
	// 近战攻击不销毁，等 LifeTime 自然结束
	if (!bIsMeleeAttack)
	{
		UE_LOG(LogRiverOfInk, Log, TEXT("AttackArea: Hit enemy"));
		Disappear(EAttackAreaDisappearReason::HitEnemy);
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

void AAttackAreaBase::ApplyDamage_Implementation(AActor* Target)
{
	// 攻击者由代码填充（施放者），不依赖编辑器配置
	DamageInfo.Attacker = GetOwner();

	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Target))
	{
		Enemy->TakeDamage(DamageInfo);
	}
	else if (APlayerCharacter* Player = Cast<APlayerCharacter>(Target))
	{
		Player->TakeDamage(DamageInfo);
	}
}

bool AAttackAreaBase::IsValidTarget_Implementation(AActor* Target)
{
	// 如果目标是玩家且正在闪避，跳过
	if (APlayerCharacter* PlayerTarget = Cast<APlayerCharacter>(Target))
	{
		if (PlayerTarget->IsDashing()) return false;
	}

	if (!bDamageOpponentOnly) return true;

	if (GetOwner() && GetOwner()->IsA<APlayerCharacter>())
	{
		return Target->IsA<AEnemyBase>();
	}
	else
	{
		return Target->IsA<APlayerCharacter>();
	}
}

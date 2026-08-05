// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/AttackAreaBase.h"
#include "RiverOfInk.h"
#include "Core/GlobalStructs.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"

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
}

void AAttackAreaBase::BeginPlay()
{
	Super::BeginPlay();
	ElapsedTime = 0.0f;
	CollisionSphere->SetSphereRadius(Radius);
}

void AAttackAreaBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
	}
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

		// 玩家处于直接性伤害无敌状态，跳过
		if (PlayerTarget->IsInvincible()) return false;
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
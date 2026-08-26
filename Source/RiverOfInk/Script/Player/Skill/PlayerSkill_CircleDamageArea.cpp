// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Skill/PlayerSkill_CircleDamageArea.h"

#include "Common/AttackAreaBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GlobalStructs.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "NiagaraComponent.h"
#include "Player/Skill/SkillComponent.h"
#include "UObject/ConstructorHelpers.h"

APlayerSkill_CircleDamageArea::APlayerSkill_CircleDamageArea()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->InitSphereRadius(Radius);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_GameTraceChannel1); // DamageArea
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap); // EnemyHitbox
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->SetCanEverAffectNavigation(false);
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &APlayerSkill_CircleDamageArea::OnSphereBeginOverlap);

	VisualPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualPlane"));
	VisualPlane->SetupAttachment(CollisionSphere);
	VisualPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualPlane->SetGenerateOverlapEvents(false);
	VisualPlane->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshAsset.Succeeded())
	{
		VisualPlane->SetStaticMesh(PlaneMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlaneMaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (PlaneMaterialAsset.Succeeded())
	{
		VisualPlane->SetMaterial(0, PlaneMaterialAsset.Object);
	}
}

void APlayerSkill_CircleDamageArea::BeginPlay()
{
	Super::BeginPlay();

	CollisionSphere->SetSphereRadius(Radius, true);
	UpdateVisualPlaneScale();
	SetLifeSpan(LifeTime);

	if (VisualPlane)
	{
		VisualPlane->SetVisibility(false, true);
	}

	if (bUsePlaceholderVFXOnly)
	{
		SuppressLegacyNiagaraVFX();
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugArea)
	{
		if (bUseArcHitbox)
		{
			FVector Forward = GetActorForwardVector();
			Forward.Z = 0.0f;
			if (!Forward.Normalize())
			{
				Forward = FVector::ForwardVector;
			}

			DrawDebugCone(
				GetWorld(),
				GetActorLocation() + FVector(0.0f, 0.0f, 3.0f),
				Forward,
				Radius,
				FMath::DegreesToRadians(ArcHalfAngle),
				FMath::DegreesToRadians(ArcHalfAngle),
				32,
				FColor::Yellow,
				false,
				LifeTime,
				0,
				2.0f);
		}
		else
		{
			DrawDebugCircle(
				GetWorld(),
				GetActorLocation() + FVector(0.0f, 0.0f, 3.0f),
				Radius,
				32,
				FColor::Blue,
				false,
				LifeTime,
				0,
				2.0f,
				FVector::ForwardVector,
				FVector::UpVector,
				false);
		}
	}
#endif

	UE_LOG(LogSkill, Log,
		TEXT("CircularSlash damage area ready: Radius=%.0f LifeTime=%.2f Arc=%s HalfAngle=%.1f."),
		Radius,
		LifeTime,
		bUseArcHitbox ? TEXT("true") : TEXT("false"),
		ArcHalfAngle);

	if (bNullifyEnemyProjectiles)
	{
		NullifyEnemyProjectilesInRange();
	}

	// Include enemies that were already inside the final radius when the deferred spawn finished.
	TArray<AActor*> OverlappingActors;
	CollisionSphere->GetOverlappingActors(OverlappingActors, AEnemyBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryDamageActor(OverlappingActor);
	}
}

void APlayerSkill_CircleDamageArea::SetUsePlaceholderVFXOnly(bool bInUsePlaceholderVFXOnly)
{
	bUsePlaceholderVFXOnly = bInUsePlaceholderVFXOnly;

	// Deferred-spawn callers set this before FinishSpawningActor. Keep the
	// setter safe for Blueprint/debug callers that may invoke it afterwards.
	if (bUsePlaceholderVFXOnly && HasActorBegunPlay())
	{
		SuppressLegacyNiagaraVFX();
	}
}

void APlayerSkill_CircleDamageArea::Initialize(
	float InRadius,
	float InDamage,
	float InLifeTime,
	AActor* InInstigator,
	bool bInNullifyEnemyProjectiles,
	bool bInUseArcHitbox,
	float InArcHalfAngle)
{
	Radius = FMath::Max(1.0f, InRadius);
	Damage = FMath::Max(0.0f, InDamage);
	LifeTime = FMath::Max(0.01f, InLifeTime);
	DamageInstigator = InInstigator;
	bNullifyEnemyProjectiles = bInNullifyEnemyProjectiles;
	bUseArcHitbox = bInUseArcHitbox;
	ArcHalfAngle = FMath::Clamp(InArcHalfAngle, 0.0f, 180.0f);
	CollisionSphere->SetSphereRadius(Radius, HasActorBegunPlay());
	UpdateVisualPlaneScale();

	if (HasActorBegunPlay())
	{
		SetLifeSpan(LifeTime);
	}
}

void APlayerSkill_CircleDamageArea::SuppressLegacyNiagaraVFX()
{
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents<UNiagaraComponent>(NiagaraComponents);

	int32 SuppressedCount = 0;
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!IsValid(NiagaraComponent))
		{
			continue;
		}

		NiagaraComponent->DeactivateImmediate();
		NiagaraComponent->SetVisibility(false, true);
		++SuppressedCount;
	}

	UE_LOG(LogSkill, Log,
		TEXT("TwoStageArc placeholder VFX: suppressed %d legacy Niagara component(s) on %s."),
		SuppressedCount,
		*GetNameSafe(this));
}

void APlayerSkill_CircleDamageArea::NullifyEnemyProjectilesInRange()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_GameTraceChannel1); // DamageArea
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NullRing), false);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(DamageInstigator);

	TArray<FOverlapResult> Overlaps;
	const bool bHasOverlaps = World->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(Radius),
		QueryParams);

	int32 ProjectileCount = 0;
	int32 NullifiedCount = 0;
	if (bHasOverlaps)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AAttackAreaBase* AttackArea = Cast<AAttackAreaBase>(Overlap.GetActor());
			if (!IsValid(AttackArea) || !AttackArea->bIsEnemyProjectile)
			{
				continue;
			}

			++ProjectileCount;
			if (AttackArea->NullifyEnemyProjectile())
			{
				++NullifiedCount;
			}
		}
	}

	UE_LOG(LogSkill, Log,
		TEXT("Null Ring query resolved: Radius=%.0f EnemyProjectiles=%d Nullified=%d."),
		Radius,
		ProjectileCount,
		NullifiedCount);
}

void APlayerSkill_CircleDamageArea::UpdateVisualPlaneScale()
{
	if (VisualPlane)
	{
		// UE's basic Plane mesh is 100 cm wide, so its local half-width is 50 cm.
		const float MeshHalfWidth = 50.0f;
		VisualPlane->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
		VisualPlane->SetRelativeScale3D(FVector(Radius / MeshHalfWidth, Radius / MeshHalfWidth, 1.0f));
	}
}

void APlayerSkill_CircleDamageArea::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	TryDamageActor(OtherActor);
}

void APlayerSkill_CircleDamageArea::TryDamageActor(AActor* OtherActor)
{
	if (!IsValid(OtherActor) || OtherActor == DamageInstigator || HitActors.Contains(OtherActor))
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(OtherActor);
	if (!Enemy)
	{
		return;
	}

	if (bUseArcHitbox)
	{
		FVector ToEnemy = Enemy->GetActorLocation() - GetActorLocation();
		ToEnemy.Z = 0.0f;
		FVector Forward = GetActorForwardVector();
		Forward.Z = 0.0f;

		const bool bAtOrigin = ToEnemy.IsNearlyZero();
		if (!bAtOrigin && (!ToEnemy.Normalize() || !Forward.Normalize()))
		{
			return;
		}

		if (!bAtOrigin)
		{
			const float CosineThreshold = FMath::Cos(FMath::DegreesToRadians(ArcHalfAngle));
			if (FVector::DotProduct(Forward, ToEnemy) < CosineThreshold)
			{
				return;
			}
		}
	}

	HitActors.Add(OtherActor);

	FTakeDamageInfo DamageInfo;
	DamageInfo.Attacker = DamageInstigator;
	DamageInfo.DamageValue = Damage;
	DamageInfo.DamageType = DamageType;
	DamageInfo.bIsDirectDamage = bIsDirectDamage;
	DamageInfo.bCanCauseDeath = bCanCauseDeath;
	DamageInfo.bIgnoreInvincible = bIgnoreInvincible;

	Enemy->TakeDamage(DamageInfo);
	OnHitConfirmed.Broadcast(Enemy);
	UE_LOG(LogSkill, Display, TEXT("Circular Slash Hit Enemy: %s"), *GetNameSafe(Enemy));
}

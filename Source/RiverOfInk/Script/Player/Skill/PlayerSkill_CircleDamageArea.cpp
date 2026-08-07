// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Skill/PlayerSkill_CircleDamageArea.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GlobalStructs.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
		VisualPlane->SetVisibility(true, true);
		if (UMaterialInterface* BaseMaterial = VisualPlane->GetMaterial(0))
		{
			UMaterialInstanceDynamic* BlueMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			BlueMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.25f, 1.0f, 1.0f));
			VisualPlane->SetMaterial(0, BlueMaterial);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugArea)
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
#endif

	UE_LOG(LogSkill, Log, TEXT("CircularSlash blue plane visual ready: Radius=%.0f LifeTime=%.2f."), Radius, LifeTime);

	// Include enemies that were already inside the final radius when the deferred spawn finished.
	TArray<AActor*> OverlappingActors;
	CollisionSphere->GetOverlappingActors(OverlappingActors, AEnemyBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryDamageActor(OverlappingActor);
	}
}

void APlayerSkill_CircleDamageArea::Initialize(float InRadius, float InDamage, float InLifeTime, AActor* InInstigator)
{
	Radius = FMath::Max(1.0f, InRadius);
	Damage = FMath::Max(0.0f, InDamage);
	LifeTime = FMath::Max(0.01f, InLifeTime);
	DamageInstigator = InInstigator;
	CollisionSphere->SetSphereRadius(Radius, HasActorBegunPlay());
	UpdateVisualPlaneScale();

	if (HasActorBegunPlay())
	{
		SetLifeSpan(LifeTime);
	}
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

	HitActors.Add(OtherActor);

	FTakeDamageInfo DamageInfo;
	DamageInfo.Attacker = DamageInstigator;
	DamageInfo.DamageValue = Damage;
	DamageInfo.DamageType = DamageType;
	DamageInfo.bIsDirectDamage = bIsDirectDamage;
	DamageInfo.bCanCauseDeath = bCanCauseDeath;
	DamageInfo.bIgnoreInvincible = bIgnoreInvincible;

	Enemy->TakeDamage(DamageInfo);
	UE_LOG(LogSkill, Display, TEXT("Circular Slash Hit Enemy: %s"), *GetNameSafe(Enemy));
}

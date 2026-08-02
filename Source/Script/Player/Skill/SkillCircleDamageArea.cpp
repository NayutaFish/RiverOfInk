// Copyright Epic Games, Inc. All Rights Reserved.

#include "Skill/SkillCircleDamageArea.h"

#include "Components/SphereComponent.h"
#include "Core/GlobalStructs.h"
#include "DrawDebugHelpers.h"
#include "Enemy/EnemyBase.h"
#include "Skill/HikariSkillComponent.h"

ASkillCircleDamageArea::ASkillCircleDamageArea()
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
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ASkillCircleDamageArea::OnSphereBeginOverlap);
}

void ASkillCircleDamageArea::BeginPlay()
{
	Super::BeginPlay();

	CollisionSphere->SetSphereRadius(Radius, true);
	SetLifeSpan(LifeTime);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugArea)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), Radius, 32, FColor::Cyan, false, LifeTime, 0, 2.0f);
	}
#endif

	// Include enemies that were already inside the final radius when the deferred spawn finished.
	TArray<AActor*> OverlappingActors;
	CollisionSphere->GetOverlappingActors(OverlappingActors, AEnemyBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryDamageActor(OverlappingActor);
	}
}

void ASkillCircleDamageArea::Initialize(float InRadius, float InDamage, float InLifeTime, AActor* InInstigator)
{
	Radius = FMath::Max(1.0f, InRadius);
	Damage = FMath::Max(0.0f, InDamage);
	LifeTime = FMath::Max(0.01f, InLifeTime);
	DamageInstigator = InInstigator;
	CollisionSphere->SetSphereRadius(Radius, HasActorBegunPlay());

	if (HasActorBegunPlay())
	{
		SetLifeSpan(LifeTime);
	}
}

void ASkillCircleDamageArea::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	TryDamageActor(OtherActor);
}

void ASkillCircleDamageArea::TryDamageActor(AActor* OtherActor)
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

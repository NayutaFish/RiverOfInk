// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Skill/PlayerSkill_ThrownGrenade.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GlobalEnums.h"
#include "Common/CombatEffectTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/OverlapResult.h"
#include "Player/PlayerCharacter.h"
#include "Player/ProjectileTargetingComponent.h"
#include "Player/Skill/SkillComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr ECollisionChannel EnemyHitboxChannel = ECC_GameTraceChannel2;
}

APlayerSkill_ThrownGrenade::APlayerSkill_ThrownGrenade()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->InitSphereRadius(CollisionRadius);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_GameTraceChannel1); // DamageArea
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(EnemyHitboxChannel, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->SetCanEverAffectNavigation(false);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionSphere);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshAsset.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SphereMaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMaterialAsset.Succeeded())
	{
		VisualMesh->SetMaterial(0, SphereMaterialAsset.Object);
	}

	DamageInfo.DamageType = EDamageType::Unified;
	DamageInfo.bCanCauseDeath = true;
	DamageInfo.bIsDirectDamage = true;
	ExplosionsRemaining = 1;
}

void APlayerSkill_ThrownGrenade::BeginPlay()
{
	Super::BeginPlay();

	ElapsedTime = 0.0f;
	CollisionRadius = FMath::Max(1.0f, CollisionRadius);
	ExplosionRadius = FMath::Max(1.0f, ExplosionRadius);
	FuseTime = FMath::Max(0.05f, FuseTime);
	ExplosionCount = FMath::Max(1, ExplosionCount);
	ExplosionDelay = FMath::Max(0.0f, ExplosionDelay);
	ExplosionsRemaining = ExplosionCount;
	CollisionSphere->SetSphereRadius(CollisionRadius, true);

	if (VisualMesh)
	{
		VisualMesh->SetRelativeScale3D(FVector(CollisionRadius / 50.0f));
		if (UMaterialInterface* BaseMaterial = VisualMesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* GrenadeMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			GrenadeMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.04f, 0.22f, 0.95f, 1.0f));
			VisualMesh->SetMaterial(0, GrenadeMaterial);
		}
	}

	UE_LOG(LogSkill, Log,
		TEXT("ThrownGrenade launched: Fuse=%.2f Radius=%.0f Damage=%.1f Velocity=%s."),
		FuseTime,
		ExplosionRadius,
		Damage,
		*Velocity.ToString());
}

void APlayerSkill_ThrownGrenade::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExplosionTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void APlayerSkill_ThrownGrenade::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDetonated || !GetWorld())
	{
		return;
	}

	ElapsedTime += DeltaTime;
	UpdateHoming(DeltaTime);
	Velocity.Z += GravityZ * DeltaTime;

	const FVector Start = GetActorLocation();
	const FVector End = Start + Velocity * DeltaTime;
	FHitResult ImpactHit;
	if (SweepForImpact(Start, End, ImpactHit))
	{
		SetActorLocation(ImpactHit.Location);
		Detonate();
		return;
	}

	SetActorLocation(End);
	if (ElapsedTime >= FuseTime)
	{
		Detonate();
	}
}

void APlayerSkill_ThrownGrenade::Initialize(
	float InFuseTime,
	float InExplosionRadius,
	float InDamage,
	float InGravityZ,
	float InCollisionRadius,
	const FVector& InInitialVelocity,
	AActor* InInstigator,
	int32 InExplosionCount,
	float InExplosionDelay,
	AActor* InHomingTarget,
	float InHomingTurnRate,
	FCombatEffectHandle InHomingMarkHandle,
	float InHomingStartDelay,
	float InHomingMaxDistance,
	float InHomingAcceptanceRadius,
	EProjectileGuidanceMode InGuidanceMode,
	FVector InGuidanceTargetOffset
)
{
	FuseTime = FMath::Max(0.05f, InFuseTime);
	ExplosionRadius = FMath::Max(1.0f, InExplosionRadius);
	Damage = FMath::Max(0.0f, InDamage);
	GravityZ = InGravityZ;
	CollisionRadius = FMath::Max(1.0f, InCollisionRadius);
	ExplosionCount = FMath::Max(1, InExplosionCount);
	ExplosionDelay = FMath::Max(0.0f, InExplosionDelay);
	ExplosionsRemaining = ExplosionCount;
	Velocity = InInitialVelocity;
	DamageInstigator = InInstigator;
	bGuidanceCorrectionLogged = false;
	ProjectileSpec = FProjectileSpec();
	ProjectileSpec.LifeTime = FuseTime;
	ProjectileSpec.ProjectileSpeed = InInitialVelocity.Size();
	ProjectileSpec.HomingTarget = InHomingTarget;
	ProjectileSpec.HomingMarkHandle = InHomingMarkHandle;
	ProjectileSpec.HomingTurnRate = FMath::Max(0.0f, InHomingTurnRate);
	ProjectileSpec.HomingStartDelay = FMath::Max(0.0f, InHomingStartDelay);
	ProjectileSpec.HomingMaxDistance = FMath::Max(0.0f, InHomingMaxDistance);
	ProjectileSpec.HomingAcceptanceRadius = FMath::Max(0.0f, InHomingAcceptanceRadius);
	ProjectileSpec.GuidanceMode = InGuidanceMode;
	ProjectileSpec.GuidanceTargetOffset = InGuidanceTargetOffset;
	ProjectileSpec.bEnableHoming = IsValid(InHomingTarget) && InHomingMarkHandle.IsValid();
	if (ProjectileSpec.bEnableHoming && ProjectileSpec.GuidanceMode == EProjectileGuidanceMode::None)
	{
		// Preserve the original direct-projectile behavior for legacy callers that
		// provide a target but do not yet specify a guidance mode.
		ProjectileSpec.GuidanceMode = EProjectileGuidanceMode::SoftProjectileHoming;
	}
	DamageInfo.Attacker = InInstigator;
	DamageInfo.DamageValue = Damage;
	DamageInfo.DamageType = EDamageType::Unified;

	if (CollisionSphere)
	{
		CollisionSphere->SetSphereRadius(CollisionRadius, HasActorBegunPlay());
	}
}

void APlayerSkill_ThrownGrenade::UpdateHoming(float DeltaTime)
{
	if (!ProjectileSpec.bEnableHoming || !ProjectileSpec.HomingTarget)
	{
		return;
	}

	AEnemyBase* Target = Cast<AEnemyBase>(ProjectileSpec.HomingTarget.Get());
	APlayerCharacter* Player = Cast<APlayerCharacter>(DamageInstigator);
	UProjectileTargetingComponent* Targeting = Player
		? Player->GetProjectileTargetingComponent()
		: nullptr;
	if (!IsValid(Target)
		|| !Targeting
		|| !Targeting->IsHomingMarkActive(Target, ProjectileSpec.HomingMarkHandle))
	{
		ClearGuidance();
		return;
	}

	if (ElapsedTime < ProjectileSpec.HomingStartDelay)
	{
		return;
	}

	if (ProjectileSpec.GuidanceMode == EProjectileGuidanceMode::TargetedArcLanding)
	{
		UpdateTargetedArcGuidance(Target, DeltaTime);
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float DistanceSquared = ToTarget.SizeSquared();
	if (ProjectileSpec.HomingMaxDistance > 0.0f
		&& DistanceSquared > FMath::Square(ProjectileSpec.HomingMaxDistance))
	{
		ClearGuidance();
		return;
	}

	const float CurrentSpeed = Velocity.Size();
	if (ToTarget.IsNearlyZero()
		|| (ProjectileSpec.HomingAcceptanceRadius > 0.0f
			&& DistanceSquared <= FMath::Square(ProjectileSpec.HomingAcceptanceRadius))
		|| CurrentSpeed <= KINDA_SMALL_NUMBER
		|| ProjectileSpec.HomingTurnRate <= 0.0f)
	{
		return;
	}

	const FRotator NewRotation = FMath::RInterpConstantTo(
		Velocity.Rotation(),
		ToTarget.Rotation(),
		DeltaTime,
		ProjectileSpec.HomingTurnRate);
	Velocity = NewRotation.Vector() * CurrentSpeed;
}

void APlayerSkill_ThrownGrenade::UpdateTargetedArcGuidance(AEnemyBase* Target, float DeltaTime)
{
	if (!IsValid(Target))
	{
		ClearGuidance();
		return;
	}

	FVector ToLandingPoint = Target->GetActorLocation() + ProjectileSpec.GuidanceTargetOffset - GetActorLocation();
	ToLandingPoint.Z = 0.0f;
	const float DistanceSquared = ToLandingPoint.SizeSquared();
	if (ProjectileSpec.HomingMaxDistance > 0.0f
		&& DistanceSquared > FMath::Square(ProjectileSpec.HomingMaxDistance))
	{
		ClearGuidance();
		return;
	}

	FVector HorizontalVelocity = Velocity;
	HorizontalVelocity.Z = 0.0f;
	const float HorizontalSpeed = HorizontalVelocity.Size();
	if (ToLandingPoint.IsNearlyZero()
		|| (ProjectileSpec.HomingAcceptanceRadius > 0.0f
			&& DistanceSquared <= FMath::Square(ProjectileSpec.HomingAcceptanceRadius))
		|| HorizontalSpeed <= KINDA_SMALL_NUMBER
		|| ProjectileSpec.HomingTurnRate <= 0.0f)
	{
		return;
	}

	const FRotator NewRotation = FMath::RInterpConstantTo(
		HorizontalVelocity.Rotation(),
		ToLandingPoint.Rotation(),
		DeltaTime,
		ProjectileSpec.HomingTurnRate);
	const FVector NewHorizontalVelocity = NewRotation.Vector() * HorizontalSpeed;
	if (!bGuidanceCorrectionLogged && !NewHorizontalVelocity.Equals(HorizontalVelocity, 0.1f))
	{
		UE_LOG(LogSkill, Verbose,
			TEXT("ThrownGrenade guidance steering: Target=%s DeltaYaw=%.1f Distance=%.1f."),
			*Target->GetName(),
			FMath::FindDeltaAngleDegrees(HorizontalVelocity.Rotation().Yaw, NewRotation.Yaw),
			FMath::Sqrt(DistanceSquared));
		bGuidanceCorrectionLogged = true;
	}
	Velocity.X = NewHorizontalVelocity.X;
	Velocity.Y = NewHorizontalVelocity.Y;
}

void APlayerSkill_ThrownGrenade::ClearGuidance()
{
	ProjectileSpec.bEnableHoming = false;
	ProjectileSpec.HomingTarget = nullptr;
	ProjectileSpec.HomingMarkHandle = FCombatEffectHandle();
	ProjectileSpec.GuidanceMode = EProjectileGuidanceMode::None;
	ProjectileSpec.GuidanceTargetOffset = FVector::ZeroVector;
}

bool APlayerSkill_ThrownGrenade::SweepForImpact(
	const FVector& Start,
	const FVector& End,
	FHitResult& OutHit
) const
{
	if (!GetWorld())
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(EnemyHitboxChannel);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ThrownGrenadeImpact), false);
	QueryParams.AddIgnoredActor(this);
	if (DamageInstigator)
	{
		QueryParams.AddIgnoredActor(DamageInstigator);
	}

	// Object-type sweeps use the engine's default WorldStatic query channel.
	// The white-box floor uses BlockAllDynamic (WorldDynamic), so it is not
	// returned by that overload even when WorldDynamic is in ObjectQuery.
	// Visibility is the project's geometry trace channel and is blocked by the
	// floor, therefore use it for the world-impact leg of the sweep.
	FHitResult WorldHit;
	const bool bWorldHit = GetWorld()->SweepSingleByChannel(
		WorldHit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(CollisionRadius),
		QueryParams);

	// Keep enemy detection on the explicit EnemyHitbox object type. This is an
	// overlap response by design, so it remains independent of the floor trace
	// channel and preserves direct enemy contact as a valid detonation trigger.
	FHitResult EnemyHit;
	const bool bEnemyHit = GetWorld()->SweepSingleByObjectType(
		EnemyHit,
		Start,
		End,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(CollisionRadius),
		QueryParams);

	// A grenade can be spawned exactly on the floor when the character capsule
	// is standing on a low platform. Do not detonate on that initial overlap;
	// the next sweep starts from the airborne position and still catches every
	// real world collision afterwards. Enemy contact remains valid on spawn.
	const bool bValidWorldHit = bWorldHit && !WorldHit.bStartPenetrating;
	if (bValidWorldHit && (!bEnemyHit || WorldHit.Time <= EnemyHit.Time))
	{
		OutHit = WorldHit;
	}
	else if (bEnemyHit)
	{
		OutHit = EnemyHit;
	}
	else
	{
		return false;
	}

	UE_LOG(LogSkill, Log,
		TEXT("ThrownGrenade impact: Actor=%s Component=%s Location=%s ImpactPoint=%s."),
		*GetNameSafe(OutHit.GetActor()),
		*GetNameSafe(OutHit.GetComponent()),
		*OutHit.Location.ToString(),
		*OutHit.ImpactPoint.ToString());

	return true;
}

void APlayerSkill_ThrownGrenade::Detonate()
{
	if (bDetonated || !GetWorld())
	{
		return;
	}

	bDetonated = true;
	ExplosionsRemaining = FMath::Max(1, ExplosionCount);
	PerformExplosion();

	if (ExplosionsRemaining > 0 && GetWorld())
	{
		if (ExplosionDelay <= KINDA_SMALL_NUMBER)
		{
			PerformExplosion();
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(
				ExplosionTimerHandle,
				this,
				&APlayerSkill_ThrownGrenade::PerformExplosion,
				ExplosionDelay,
				false);
			return;
		}
	}

	if (ExplosionsRemaining <= 0)
	{
		Destroy();
	}
}

void APlayerSkill_ThrownGrenade::PerformExplosion()
{
	if (!GetWorld() || ExplosionsRemaining <= 0)
	{
		return;
	}

	const int32 ExplosionIndex = ExplosionCount - ExplosionsRemaining + 1;
	--ExplosionsRemaining;
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(EnemyHitboxChannel);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ThrownGrenadeExplosion), false);
	QueryParams.AddIgnoredActor(this);
	if (DamageInstigator)
	{
		QueryParams.AddIgnoredActor(DamageInstigator);
	}

	const bool bHasOverlaps = GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(ExplosionRadius),
		QueryParams);

	int32 HitCount = 0;
	if (bHasOverlaps)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AEnemyBase* Enemy = Cast<AEnemyBase>(Overlap.GetActor());
			if (!IsValid(Enemy))
			{
				continue;
			}

			Enemy->TakeDamage(DamageInfo);
			++HitCount;
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugExplosion)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), ExplosionRadius, 32, FColor::Blue, false, 0.35f, 0, 3.0f);
	}
#endif

	UE_LOG(LogSkill, Log,
		TEXT("ThrownGrenade explosion %d/%d: Radius=%.0f Damage=%.1f Hits=%d."),
		ExplosionIndex,
		ExplosionCount,
		ExplosionRadius,
		Damage,
		HitCount);

	if (ExplosionsRemaining <= 0)
	{
		Destroy();
	}
}

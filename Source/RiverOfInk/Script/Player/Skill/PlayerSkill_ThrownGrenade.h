// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GlobalStructs.h"
#include "Common/ProjectileTypes.h"
#include "PlayerSkill_ThrownGrenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class AEnemyBase;

/**
 * First-pass Q form: a player-owned projectile that follows a short arc and
 * detonates on world/enemy impact or when its fuse expires.
 *
 * The actor owns movement and the explosion query. Damage is still delegated
 * to AEnemyBase so the project-wide Damage/Defense calculator remains the
 * single damage rule.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API APlayerSkill_ThrownGrenade : public AActor
{
	GENERATED_BODY()

public:
	APlayerSkill_ThrownGrenade();

	/** Configure the throw before FinishSpawningActor is called. */
	UFUNCTION(BlueprintCallable, Category = "Skill|ThrownGrenade")
	void Initialize(
		float InFuseTime,
		float InExplosionRadius,
		float InDamage,
		float InGravityZ,
		float InCollisionRadius,
		const FVector& InInitialVelocity,
		AActor* InInstigator,
		int32 InExplosionCount = 1,
		float InExplosionDelay = 0.12f,
		AActor* InHomingTarget = nullptr,
		float InHomingTurnRate = 360.0f,
		FCombatEffectHandle InHomingMarkHandle = FCombatEffectHandle(),
		float InHomingStartDelay = 0.06f,
		float InHomingMaxDistance = 2500.0f,
		float InHomingAcceptanceRadius = 80.0f,
		EProjectileGuidanceMode InGuidanceMode = EProjectileGuidanceMode::None,
		FVector InGuidanceTargetOffset = FVector::ZeroVector,
		UNiagaraSystem* InNiagaraSystem = nullptr
	);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|ThrownGrenade|Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	/** Blue-white placeholder mesh for the in-flight grenade. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|ThrownGrenade|Visual")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** 飞行中的雷电球 Niagara 特效；在编辑器里赋值后，将替代蓝色占位球体。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade|Visual")
	TObjectPtr<UNiagaraSystem> ProjectileNiagaraSystem;

	/** 已挂接在投射物上的 Niagara 特效组件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|ThrownGrenade|Visual")
	TObjectPtr<UNiagaraComponent> NiagaraEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade", meta = (ClampMin = "0.05"))
	float FuseTime = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade", meta = (ClampMin = "1.0"))
	float ExplosionRadius = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade", meta = (ClampMin = "0.0"))
	float Damage = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade")
	float GravityZ = -980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade", meta = (ClampMin = "1.0"))
	float CollisionRadius = 32.0f;

	/** Number of explosions at the detonation location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade", meta = (ClampMin = "1"))
	int32 ExplosionCount = 1;

	/** Delay between repeated explosions from ExtraExplosion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade", meta = (ClampMin = "0.0", Units = "s"))
	float ExplosionDelay = 0.12f;

	/** Development-only debug sphere at the explosion location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|ThrownGrenade|Debug")
	bool bDrawDebugExplosion = true;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Detonate();
	void PerformExplosion();
	void UpdateHoming(float DeltaTime);
	void UpdateTargetedArcGuidance(AEnemyBase* Target, float DeltaTime);
	void ClearGuidance();
	bool SweepForImpact(const FVector& Start, const FVector& End, FHitResult& OutHit) const;

	UPROPERTY(Transient)
	TObjectPtr<AActor> DamageInstigator;

	FTakeDamageInfo DamageInfo;
	FProjectileSpec ProjectileSpec;
	FVector Velocity = FVector::ZeroVector;
	float ElapsedTime = 0.0f;
	bool bDetonated = false;
	int32 ExplosionsRemaining = 1;
	FTimerHandle ExplosionTimerHandle;
};

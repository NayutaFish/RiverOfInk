// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GlobalEnums.h"
#include "PlayerSkill_CircleDamageArea.generated.h"

class USphereComponent;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerSkillAreaHitConfirmed, AActor*);

/** Short-lived, player-owned radial damage area for Circular Slash. */
UCLASS(Blueprintable)
class RIVEROFINK_API APlayerSkill_CircleDamageArea : public AActor
{
	GENERATED_BODY()

public:
	APlayerSkill_CircleDamageArea();

	UFUNCTION(BlueprintCallable, Category = "SkillArea")
	void Initialize(
		float InRadius,
		float InDamage,
		float InLifeTime,
		AActor* InInstigator,
		bool bInNullifyEnemyProjectiles = false,
		bool bInUseArcHitbox = false,
		float InArcHalfAngle = 180.0f);

	/** Keep the original Blueprint circular slash VFX for the normal E path. */
	UFUNCTION(BlueprintCallable, Category = "SkillArea|Visual")
	void SetUseLegacyCircularSlashVFX(bool bInUseLegacyCircularSlashVFX);

	/** Native notification emitted once for each enemy damaged by this area. */
	FOnPlayerSkillAreaHitConfirmed OnHitConfirmed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillArea")
	TObjectPtr<USphereComponent> CollisionSphere;

	/** Blue whitebox plane used as the first-pass CircularSlash visual. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillArea|Visual")
	TObjectPtr<UStaticMeshComponent> VisualPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillArea", meta = (ClampMin = "0.0"))
	float Damage = 120.0f;

	/** Legacy metadata only; all skills use the unified damage calculation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillArea")
	EDamageType DamageType = EDamageType::Unified;

	/** 是否为直接性伤害（非持续/非技能间接伤害） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillArea")
	bool bIsDirectDamage = true;

	/** 能否致死 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillArea")
	bool bCanCauseDeath = true;

	/** 是否无视无敌状态 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillArea")
	bool bIgnoreInvincible = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillArea", meta = (ClampMin = "1.0"))
	float Radius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillArea", meta = (ClampMin = "0.01"))
	float LifeTime = 0.25f;

	/** Development-only wireframe preview; compiled out when debug drawing is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillArea|Debug")
	bool bDrawDebugArea = true;

	/** True only for the E Null Ring form. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillArea|NullRing")
	bool bNullifyEnemyProjectiles = false;

	/** True when enemy damage is restricted to the horizontal close-range arc. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillArea|Arc")
	bool bUseArcHitbox = false;

	/** Horizontal half-angle of the arc filter, in degrees. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillArea|Arc", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float ArcHalfAngle = 180.0f;

	/** False only when a dedicated skill form supplies its own VFX. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillArea|Visual")
	bool bUseLegacyCircularSlashVFX = true;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	void UpdateVisualPlaneScale();
	void SuppressLegacyCircularSlashVFX();
	void TryDamageActor(AActor* OtherActor);
	void NullifyEnemyProjectilesInRange();

	UPROPERTY(Transient)
	TObjectPtr<AActor> DamageInstigator;

	TSet<TWeakObjectPtr<AActor>> HitActors;
};

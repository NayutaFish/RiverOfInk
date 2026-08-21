// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectileTargetingComponent.generated.h"

class AEnemyBase;

/**
 * Player-owned target selection for marked-projectile builds.
 *
 * The component deliberately owns no projectile movement. It only answers
 * "which marked enemy belongs to this player?" and consumes one mark charge
 * after a projectile reports an effective hit.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UProjectileTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProjectileTargetingComponent();

	/** Maximum distance used when selecting a marked target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (ClampMin = "0.0", Units = "cm"))
	float HomingSearchRadius = 2500.0f;

	/** Returns true when the owning player has the Projectile Homing build. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	bool HasHomingBuild() const;

	/** Select the nearest live enemy carrying this player's active mark. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	AEnemyBase* FindBestHomingTarget() const;

	/** True only while this player-owned mark is still active on the enemy. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	bool IsHomingMarkActive(const AEnemyBase* Target) const;

	/** Consume one mark charge after an effective projectile hit. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Homing")
	bool ConsumeHomingMark(AEnemyBase* Target);

	/** Report an effective hit without coupling projectile classes to effects. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Homing")
	void NotifyProjectileHit(AActor* Target);
};

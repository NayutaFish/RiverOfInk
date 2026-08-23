// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/CombatEffectTypes.h"
#include "ProjectileTargetingComponent.generated.h"

class AEnemyBase;

/**
 * Player-owned target selection for marked-projectile builds.
 *
 * The component deliberately owns no projectile movement. It owns the one
 * current target selected by this player and the effect handle used to
 * validate that target while projectiles are in flight.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UProjectileTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProjectileTargetingComponent();

	/**
	 * Legacy serialized property kept so existing Blueprint assets do not lose
	 * their value during migration. Target selection no longer scans by range.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (DeprecatedProperty, DeprecationMessage = "Homing uses the player's single current marked target; range scanning is no longer used."))
	float HomingSearchRadius = 2500.0f;

	/** Returns true when the owning player has the Projectile Homing build. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	bool HasHomingBuild() const;

	/** Return this player's current live marked target, if its effect is valid. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	AEnemyBase* FindBestHomingTarget() const;

	/** Apply the timed mark, transferring it away from the previous target. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Homing")
	FCombatEffectHandle ApplyOrTransferHomingMark(AEnemyBase* NewTarget, float Duration);

	/** Return the single target currently owned by this player. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	AEnemyBase* GetCurrentMarkedTarget() const;

	/** Return the effect handle associated with the current marked target. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	FCombatEffectHandle GetCurrentMarkedEffectHandle() const { return CurrentMarkedEffectHandle; }

	/** Capture the current target and exact mark identity as one spawn-time snapshot. */
	bool GetCurrentMarkedTargetSnapshot(
		AEnemyBase*& OutTarget,
		FCombatEffectHandle& OutMarkHandle) const;

	/** True when the current target and its exact effect handle are still valid. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	bool IsCurrentMarkedTargetValid() const;

	/** Remove this player's current mark and clear the local target state. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Homing")
	void ClearCurrentMarkedTarget();

	/** True only while this player-owned mark is still active on the enemy. */
	UFUNCTION(BlueprintPure, Category = "Projectile|Homing")
	bool IsHomingMarkActive(const AEnemyBase* Target) const;

	/** Validate a projectile against the exact mark identity captured at spawn. */
	bool IsHomingMarkActive(
		const AEnemyBase* Target,
		const FCombatEffectHandle& ExpectedMarkHandle) const;

	/** Current target and handle are the source of truth; no hit-count state exists. */
	private:
	TWeakObjectPtr<AEnemyBase> CurrentMarkedTarget;
	FCombatEffectHandle CurrentMarkedEffectHandle;
};

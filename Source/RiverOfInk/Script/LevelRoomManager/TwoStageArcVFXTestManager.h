// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TwoStageArcVFXTestManager.generated.h"

class AEnemyBase;
class AEnemySpawnPoint;
class USceneComponent;

/**
 * Small, isolated manager used by the TwoStageArc VFX test map.
 *
 * It keeps exactly one test enemy alive: after the enemy broadcasts its death
 * event, a new instance is spawned at the configured spawn point.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API ATwoStageArcVFXTestManager : public AActor
{
	GENERATED_BODY()

public:
	ATwoStageArcVFXTestManager();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Enemy class used for the repeated VFX test cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Enemy")
	TSubclassOf<AEnemyBase> EnemyClass;

	/** If unset, the first AEnemySpawnPoint in the level is used. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Test|Enemy")
	TObjectPtr<AEnemySpawnPoint> SpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Enemy", meta = (ClampMin = "0.0", Units = "s"))
	float InitialSpawnDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Enemy", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnDelay = 0.75f;

	/** Keep the target stationary so VFX comparisons are repeatable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test|Enemy")
	bool bFreezeTestEnemy = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test|Runtime")
	TObjectPtr<AEnemyBase> ActiveEnemy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Test|Runtime")
	int32 SpawnedEnemyCount = 0;

	UFUNCTION(BlueprintCallable, Category = "Test")
	void SpawnTestEnemy();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveSpawnPoint();

	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadEnemy);

	FTimerHandle SpawnTimerHandle;
	bool bShuttingDown = false;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawnPoint.generated.h"

class UArrowComponent;
class UBillboardComponent;
class USceneComponent;

/**
 * Enemy spawn point.
 *
 * Provides spawn position, rotation, and editor visualization only.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API AEnemySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawnPoint();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Point")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Point")
	TObjectPtr<UArrowComponent> Arrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn Point")
	TObjectPtr<UBillboardComponent> Billboard;

public:
	/** 出生点为中心的随机生成半径（XY 平面内），0 表示固定单点 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (ClampMin = "0.0"))
	float SpawnRadius = 500.0f;

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	FTransform GetSpawnTransform() const;
};

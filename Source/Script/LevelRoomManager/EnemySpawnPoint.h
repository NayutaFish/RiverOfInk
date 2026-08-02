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
class TEST_GAMEPLAY_API AEnemySpawnPoint : public AActor
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
	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	FTransform GetSpawnTransform() const;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawnPoint.generated.h"

class UArrowComponent;
class UBillboardComponent;
class USceneComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

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


	/** 墨水坑材质参数名，默认 fadeValue */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point|Fade")
	FName FadeParameterName = TEXT("fadeValue");

	/** fadeValue 渐变速度（每秒变化量） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point|Fade", meta = (ClampMin = "0.01"))
	float FadeInterpSpeed = 0.05f;

	/** 由 DemoRoomManager 分配该出生点本局应刷怪的总数。 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point|Fade")
	void AssignSpawnCount(int32 InTotalSpawnCount);

	/** 每次成功从该出生点刷怪时调用，推进墨水坑溶解。 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point|Fade")
	void NotifyEnemySpawned();

	/** 直接让墨水坑完全溶解（清场时调用）。 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point|Fade")
	void CompleteInkFade();

	/** 当前墨水坑溶解进度 0~1。 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point|Fade")
	float GetFadeValue() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	void SetupInkMaterial();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> InkMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FadeMaterialInstance;

	int32 TotalSpawnCount = 0;
	int32 SpawnedCount = 0;
	float CurrentFadeValue = 0.0f;
	float TargetFadeValue = 0.0f;
};

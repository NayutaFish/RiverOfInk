// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoRoomManager.generated.h"

class AEnemyBase;
class AEnemySpawnPoint;
class AActor;
class USceneComponent;
class ARoguelikeRewardManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDemoRoomStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDemoRoomCleared);

/**
 * Demo room manager.
 *
 * Runs the first spawn-flow pass:
 * collect spawn points, spawn enemies, listen for death, and report room clear.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API ADemoRoomManager : public AActor
{
	GENERATED_BODY()

public:
	ADemoRoomManager();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Spawn")
	TArray<TSubclassOf<AEnemyBase>> EnemyClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Spawn")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Spawn", meta = (ClampMin = "0.0", Units = "s"))
	float StartDelay = 1.0f;

	/** 房间最大敌人刷新数（总配额） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Spawn", meta = (ClampMin = "1"))
	int32 MaxEnemySpawnCount = 10;

	/** 房间最大敌人存在数（同时存活上限） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Spawn", meta = (ClampMin = "1"))
	int32 MaxEnemyAliveCount = 5;

	/** 刷新检测计时器间隔 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Spawn", meta = (ClampMin = "0.1", Units = "s"))
	float SpawnCheckInterval = 2.0f;

	/** 目标歼敌数（达到后清除房间） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Spawn", meta = (ClampMin = "1"))
	int32 TargetEliminateCount = 5;

	/** Fixed Pure Ink granted by this Combat Room's result settlement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Economy|Pure Ink", meta = (ClampMin = "0"))
	int32 PureInkRoomResultReward = 20;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Runtime")
	int32 AliveEnemyCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Runtime")
	TArray<TObjectPtr<AEnemySpawnPoint>> SpawnPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Runtime")
	bool bRoomStarted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Runtime")
	bool bRoomCleared = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Runtime")
	int32 EliminatedEnemyCount = 0;

	/** Optional reward manager notified when this room is cleared. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Room|Reward")
	TObjectPtr<ARoguelikeRewardManager> RewardManager;

	UPROPERTY(BlueprintAssignable, Category = "Room|Events")
	FOnDemoRoomStarted OnRoomStarted;

	UPROPERTY(BlueprintAssignable, Category = "Room|Events")
	FOnDemoRoomCleared OnRoomCleared;

private:
	/** 敌人刷新余量 */
	int32 SpawnQuota = 0;

	/** 存活敌人列表 */
	UPROPERTY()
	TArray<TObjectPtr<AEnemyBase>> ActiveEnemyList;

	/** 当前房间敌人刷新计时器 */
	FTimerHandle SpawnCheckTimerHandle;

	/** 定时检测刷新 */
	void CheckAndSpawn();

public:
	UFUNCTION(BlueprintCallable, Category = "Room")
	void CollectSpawnPoints();

	UFUNCTION(BlueprintCallable, Category = "Room")
	void StartRoom();

	UFUNCTION(BlueprintCallable, Category = "Room")
	void CheckRoomClear();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadEnemy);

	UFUNCTION(BlueprintImplementableEvent, Category = "Room")
	void OnRoomClear();
};

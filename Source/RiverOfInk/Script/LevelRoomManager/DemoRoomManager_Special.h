// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LevelRoomManager/DemoRoomManager.h"
#include "DemoRoomManager_Special.generated.h"

class AEnemyBase;

/**
 * 一次性刷怪的敌人配置项：敌人蓝图类 + 数量。
 */
USTRUCT(BlueprintType)
struct FSpecialRoomEnemyEntry
{
	GENERATED_BODY()

	/** 要生成的敌人蓝图类（EnemyBase 子类） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	TSubclassOf<AEnemyBase> EnemyClass;

	/** 生成数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy", meta = (ClampMin = "0"))
	int32 Count = 1;
};

/**
 * 特殊房间管理器：开局一次性把配置表里的敌人全部生成完，全部击杀即清场。
 *
 * 与基类 ADemoRoomManager 的差异：
 *  - **不使用**基类的 EnemyClasses / MaxEnemySpawnCount / MaxEnemyAliveCount / TargetEliminateCount 配额与定时刷怪逻辑
 *    （构造时已把 bAutoStart 关掉，BeginPlay 里也会再强制关一次，避免误开）；
 *  - 改用 EnemySpawnEntries（敌人蓝图 + 数量）配置：开局按数组顺序，把每个条目的 Count 个敌人在
 *    场景已有的 AEnemySpawnPoint 上轮转生成，一次性全部生成完（受基类 StartDelay 控制延迟，默认 1 秒）；
 *  - 清场条件为“本房间生成出来的敌人全部死亡”，随后沿用基类的清场流程：
 *    OnRoomCleared 广播、FCombatRoomClearedEvent、墨水坑完全溶解、RewardManager 奖励界面、OnRoomClear 事件。
 *
 * 房间类型判定（准备房间 / 非战斗房间跳过刷怪）仍沿用基类 BeginPlay 的逻辑。
 */
UCLASS(Blueprintable)
class RIVEROFINK_API ADemoRoomManager_Special : public ADemoRoomManager
{
	GENERATED_BODY()

public:
	ADemoRoomManager_Special();

	/** 开局一次性生成的敌人配置：每一项 = 一种敌人蓝图 + 数量，按数组顺序生成。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Special Spawn")
	TArray<FSpecialRoomEnemyEntry> EnemySpawnEntries;

	/** 本次实际生成出来的敌人总数（运行时只读）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Special Spawn")
	int32 TotalSpawnedEnemyCount = 0;

	/** 一次性生成全部敌人；BeginPlay 会在 StartDelay 之后自动调用。 */
	UFUNCTION(BlueprintCallable, Category = "Room|Special Spawn")
	void SpawnAllEnemies();

protected:
	virtual void BeginPlay() override;

	/** 本房间敌人的死亡回调（独立于基类的存活列表） */
	UFUNCTION()
	void HandleSpecialEnemyDeath(AActor* DeadEnemy);

private:
	/** 生成一个敌人并登记到本房间的列表 */
	AEnemyBase* SpawnSpecialEnemy(TSubclassOf<AEnemyBase> ClassToSpawn, const FTransform& SpawnTransform);

	/** 本次生成出来、尚未死亡的敌人 */
	UPROPERTY()
	TArray<TObjectPtr<AEnemyBase>> SpecialRoomEnemies;

	/** StartDelay 之后开始生成的计时器 */
	FTimerHandle StartSpawnTimerHandle;
};

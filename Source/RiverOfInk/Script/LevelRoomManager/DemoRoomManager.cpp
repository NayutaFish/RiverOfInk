// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelRoomManager/DemoRoomManager.h"

#include "Components/SceneComponent.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Kismet/GameplayStatics.h"
#include "LevelRoomManager/EnemySpawnPoint.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "RiverOfInk.h"
#include "TimerManager.h"

ADemoRoomManager::ADemoRoomManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void ADemoRoomManager::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CollectSpawnPoints();

	if (!RewardManager)
	{
		TArray<AActor*> RewardManagers;
		UGameplayStatics::GetAllActorsOfClass(World, ARoguelikeRewardManager::StaticClass(), RewardManagers);
		if (RewardManagers.Num() > 0)
		{
			RewardManager = Cast<ARoguelikeRewardManager>(RewardManagers[0]);
		}
	}

	if (!RewardManager)
	{
		UE_LOG(LogRoguelike, Warning, TEXT("RoomManager has no RoguelikeRewardManager; Room Clear UI cannot be shown."));
	}

	if (TargetEliminateCount > MaxEnemySpawnCount)
	{
		UE_LOG(LogRoguelike, Warning, TEXT("TargetEliminateCount=%d exceeds MaxEnemySpawnCount=%d; clamping target."),
			TargetEliminateCount, MaxEnemySpawnCount);
		TargetEliminateCount = MaxEnemySpawnCount;
	}

	SpawnQuota = MaxEnemySpawnCount;

	if (bAutoStart)
	{
		FTimerHandle StartTimerHandle;
		GetWorldTimerManager().SetTimer(
			StartTimerHandle,
			this,
			&ADemoRoomManager::StartRoom,
			StartDelay,
			false
		);
	}
}

void ADemoRoomManager::CollectSpawnPoints()
{
	SpawnPoints.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemySpawnPoint::StaticClass(), FoundActors);

	for (AActor* Actor : FoundActors)
	{
		if (AEnemySpawnPoint* SpawnPoint = Cast<AEnemySpawnPoint>(Actor))
		{
			SpawnPoints.Add(SpawnPoint);
		}
	}

	UE_LOG(LogRiverOfInk, Log, TEXT("RoomManager found %d spawn points."), SpawnPoints.Num());
}

void ADemoRoomManager::StartRoom()
{
	if (bRoomStarted)
	{
		return;
	}

	if (EnemyClasses.IsEmpty())
	{
		UE_LOG(LogRiverOfInk, Error, TEXT("EnemyClasses is empty in DemoRoomManager."));
		return;
	}

	if (SpawnPoints.Num() <= 0)
	{
		UE_LOG(LogRiverOfInk, Error, TEXT("No spawn points found."));
		return;
	}

	bRoomStarted = true;
	bRoomCleared = false;

	UE_LOG(LogRiverOfInk, Log, TEXT("Room started."));

	// 初始化
	AliveEnemyCount = 0;
	ActiveEnemyList.Empty();
	EliminatedEnemyCount = 0;

	// 启动定时刷新检测
	GetWorldTimerManager().SetTimer(SpawnCheckTimerHandle, this,
		&ADemoRoomManager::CheckAndSpawn, SpawnCheckInterval, true, 0.0f);
}

void ADemoRoomManager::CheckAndSpawn()
{
	if (bRoomCleared || SpawnQuota <= 0)
	{
		// 如果已无余量且所有敌人都死光了，检查房间清除
		if (SpawnQuota <= 0 && AliveEnemyCount <= 0)
		{
			CheckRoomClear();
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World || EnemyClasses.IsEmpty() || SpawnPoints.Num() <= 0) return;

	int32 ClassIndex = 0;
	for (AEnemySpawnPoint* SpawnPoint : SpawnPoints)
	{
		if (!IsValid(SpawnPoint)) continue;
		if (SpawnQuota <= 0) break;
		if (AliveEnemyCount >= MaxEnemyAliveCount) break;

		TSubclassOf<AEnemyBase> ClassToSpawn = EnemyClasses[FMath::RandRange(0, EnemyClasses.Num() - 1)];
		if (!ClassToSpawn) continue;

		AEnemyBase* SpawnedEnemy = World->SpawnActor<AEnemyBase>(
			ClassToSpawn,
			SpawnPoint->GetSpawnTransform()
		);

		if (!IsValid(SpawnedEnemy)) continue;

		SpawnedEnemy->OnEnemyDeath.AddDynamic(this, &ADemoRoomManager::HandleEnemyDeath);

		ActiveEnemyList.Add(SpawnedEnemy);
		++AliveEnemyCount;
		--SpawnQuota;

		UE_LOG(LogRiverOfInk, Log, TEXT("Spawned enemy: %s (alive=%d, quota=%d)"),
			*SpawnedEnemy->GetName(), AliveEnemyCount, SpawnQuota);
	}
}

void ADemoRoomManager::HandleEnemyDeath(AActor* DeadEnemy)
{
	AEnemyBase* DeadEnemyBase = Cast<AEnemyBase>(DeadEnemy);
	if (!IsValid(DeadEnemyBase)) return;

	const int32 RemovedCount = ActiveEnemyList.Remove(DeadEnemyBase);
	if (RemovedCount <= 0) return;

	AliveEnemyCount = FMath::Max(0, AliveEnemyCount - RemovedCount);
	++EliminatedEnemyCount;
	UE_LOG(LogRoguelike, Log, TEXT("Enemy eliminated: Count=%d Target=%d."), EliminatedEnemyCount, TargetEliminateCount);

	UE_LOG(LogRiverOfInk, Log, TEXT("Enemy died. alive=%d, eliminated=%d, target=%d"),
		AliveEnemyCount, EliminatedEnemyCount, TargetEliminateCount);

	// 达到目标歼敌数 → 处决剩余敌人 → 房间清除
	if (EliminatedEnemyCount >= TargetEliminateCount)
	{
		// 先取消订阅，再处决，避免递归
		TArray<TObjectPtr<AEnemyBase>> Remaining = ActiveEnemyList;
		ActiveEnemyList.Empty();

		for (TObjectPtr<AEnemyBase>& Enemy : Remaining)
		{
			if (IsValid(Enemy))
			{
				Enemy->OnEnemyDeath.RemoveAll(this);
				Enemy->Die();
			}
		}

		AliveEnemyCount = 0;
		EliminatedEnemyCount = TargetEliminateCount;
		CheckRoomClear();
	}
}

void ADemoRoomManager::CheckRoomClear()
{
	if (bRoomCleared || AliveEnemyCount > 0)
	{
		return;
	}

	bRoomCleared = true;

	// 停止刷新计时器
	GetWorldTimerManager().ClearTimer(SpawnCheckTimerHandle);

	UE_LOG(LogRiverOfInk, Log, TEXT("Room Clear!"));
	if (RewardManager)
	{
		UE_LOG(LogRoguelike, Log, TEXT("Room clear reached target; requesting reward UI. Eliminated=%d Target=%d."),
			EliminatedEnemyCount, TargetEliminateCount);
		RewardManager->ShowRewardAfterRoomClear();
	}
	OnRoomClear();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelRoomManager/DemoRoomManager_Special.h"

#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/World.h"
#include "LevelRoomManager/EnemySpawnPoint.h"
#include "NiagaraFunctionLibrary.h"
#include "RiverOfInk.h"
#include "TimerManager.h"

ADemoRoomManager_Special::ADemoRoomManager_Special()
{
	PrimaryActorTick.bCanEverTick = false;

	// 本房间不使用基类的定时刷怪流程，开局一次性生成
	bAutoStart = false;
}

void ADemoRoomManager_Special::BeginPlay()
{
	// 再强制关一次自动开局：基类 bAutoStart 为真时会走 StartRoom（配额 + 定时刷怪），那不是本房间要的行为
	bAutoStart = false;

	// 基类 BeginPlay 负责：房间类型判定（准备/非战斗房间直接 return）、收集场景出生点、查找 RewardManager
	Super::BeginPlay();

	if (!GetWorld())
	{
		return;
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Special room '%s' BeginPlay: Entries=%d SpawnPoints=%d StartDelay=%.2fs."),
		*GetName(),
		EnemySpawnEntries.Num(),
		SpawnPoints.Num(),
		StartDelay);

	if (StartDelay > KINDA_SMALL_NUMBER)
	{
		// rate <= 0 会被 SetTimer 丢弃，这里用同一个防御写法
		GetWorldTimerManager().SetTimer(
			StartSpawnTimerHandle,
			this,
			&ADemoRoomManager_Special::SpawnAllEnemies,
			FMath::Max(StartDelay, KINDA_SMALL_NUMBER),
			false);
	}
	else
	{
		SpawnAllEnemies();
	}
}

void ADemoRoomManager_Special::SpawnAllEnemies()
{
	if (bRoomStarted)
	{
		UE_LOG(LogRiverOfInk, Verbose, TEXT("Special room '%s' already started; SpawnAllEnemies ignored."), *GetName());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 出生点由基类 BeginPlay 收集；为空说明基类把本房间判定为准备房间/非战斗房间而提前返回了
	if (SpawnPoints.Num() <= 0)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Special room '%s': no spawn points collected; nothing spawned (preparation or non-combat room?)."),
			*GetName());
		return;
	}

	if (EnemySpawnEntries.IsEmpty())
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Special room '%s': EnemySpawnEntries is empty; nothing spawned."),
			*GetName());
		return;
	}

	// 先统计配置总数，便于日志与早期返回
	int32 ConfiguredTotal = 0;
	for (const FSpecialRoomEnemyEntry& Entry : EnemySpawnEntries)
	{
		if (Entry.EnemyClass && Entry.Count > 0)
		{
			ConfiguredTotal += Entry.Count;
		}
	}

	if (ConfiguredTotal <= 0)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Special room '%s': every entry has no enemy class or zero count; nothing spawned."),
			*GetName());
		return;
	}

	// 房间状态初始化（对齐基类 StartRoom，但清场条件改成“全部死亡”）
	bRoomStarted = true;
	bRoomCleared = false;
	AliveEnemyCount = 0;
	EliminatedEnemyCount = 0;
	TotalSpawnedEnemyCount = 0;
	SpecialRoomEnemies.Reset();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Special room '%s' started: Configured=%d across %d spawn point(s)."),
		*GetName(),
		ConfiguredTotal,
		SpawnPoints.Num());

	OnRoomStarted.Broadcast();
	FEventBus::Publish<FCombatRoomStartedEvent>(FCombatRoomStartedEvent());

	// 每个出生点实际生成的数量，用于墨水坑溶解进度
	TMap<AEnemySpawnPoint*, int32> SpawnCountByPoint;

	// 按配置顺序一次性全部生成完；出生点轮转使用
	int32 SpawnPointCursor = 0;
	for (const FSpecialRoomEnemyEntry& Entry : EnemySpawnEntries)
	{
		if (!Entry.EnemyClass || Entry.Count <= 0)
		{
			continue;
		}

		for (int32 Index = 0; Index < Entry.Count; ++Index)
		{
			AEnemySpawnPoint* SpawnPoint = SpawnPoints[SpawnPointCursor % SpawnPoints.Num()];
			++SpawnPointCursor;

			if (!IsValid(SpawnPoint))
			{
				UE_LOG(LogRiverOfInk, Warning, TEXT("Special room '%s': skipped an invalid spawn point."), *GetName());
				continue;
			}

			const FTransform SpawnTransform = SpawnPoint->GetSpawnTransform();

			// 出生特效（可选）：基类会在特效 1 秒后再生成敌人，这里是立即生成，所以特效与敌人同时出现
			if (SpawnVFX)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SpawnVFX, SpawnTransform.GetLocation());
			}

			if (SpawnSpecialEnemy(Entry.EnemyClass, SpawnTransform))
			{
				++TotalSpawnedEnemyCount;
				SpawnCountByPoint.FindOrAdd(SpawnPoint) += 1;
			}
		}
	}

	// 墨水坑按各出生点的生成数分配总量（与基类 StartRoom 中的处理一致）
	for (AEnemySpawnPoint* SpawnPoint : SpawnPoints)
	{
		if (!IsValid(SpawnPoint))
		{
			continue;
		}

		const int32* FoundCount = SpawnCountByPoint.Find(SpawnPoint);
		SpawnPoint->AssignSpawnCount(FoundCount ? *FoundCount : 0);
	}

	// 存活数 = 实际生成数；清场目标也用实际生成数，保证“全部死亡”才清场
	AliveEnemyCount = SpecialRoomEnemies.Num();
	EliminatedEnemyCount = 0;
	TargetEliminateCount = SpecialRoomEnemies.Num();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Special room '%s' spawned %d/%d enemies at once; clear target=%d."),
		*GetName(),
		TotalSpawnedEnemyCount,
		ConfiguredTotal,
		TargetEliminateCount);

	// 一个都没生成成功：直接判定清场，避免房间永远卡住
	if (SpecialRoomEnemies.IsEmpty())
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Special room '%s' spawned nothing; clearing the room immediately."),
			*GetName());
		CheckRoomClear();
	}
}

AEnemyBase* ADemoRoomManager_Special::SpawnSpecialEnemy(
	TSubclassOf<AEnemyBase> ClassToSpawn,
	const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	if (!World || !ClassToSpawn)
	{
		return nullptr;
	}

	AEnemyBase* SpawnedEnemy = World->SpawnActor<AEnemyBase>(ClassToSpawn, SpawnTransform);
	if (!IsValid(SpawnedEnemy))
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Special room '%s' failed to spawn enemy class %s."),
			*GetName(),
			*GetNameSafe(ClassToSpawn.Get()));
		return nullptr;
	}

	// 只订阅本房间自己的回调：与基类的私有存活列表互不干扰
	SpawnedEnemy->OnEnemyDeath.AddDynamic(this, &ADemoRoomManager_Special::HandleSpecialEnemyDeath);
	SpecialRoomEnemies.Add(SpawnedEnemy);

	return SpawnedEnemy;
}

void ADemoRoomManager_Special::HandleSpecialEnemyDeath(AActor* DeadEnemy)
{
	AEnemyBase* DeadEnemyBase = Cast<AEnemyBase>(DeadEnemy);
	if (!IsValid(DeadEnemyBase))
	{
		return;
	}

	// 只统计本房间生成出来的敌人
	const int32 RemovedCount = SpecialRoomEnemies.Remove(DeadEnemyBase);
	if (RemovedCount <= 0)
	{
		return;
	}

	AliveEnemyCount = FMath::Max(0, AliveEnemyCount - RemovedCount);
	++EliminatedEnemyCount;

	// 墨水坑溶解进度沿用基类的口径（按歼敌进度推进）
	if (TargetEliminateCount > 0)
	{
		const float Progress = static_cast<float>(EliminatedEnemyCount) / static_cast<float>(TargetEliminateCount);
		for (AEnemySpawnPoint* SpawnPoint : SpawnPoints)
		{
			if (IsValid(SpawnPoint))
			{
				SpawnPoint->SetInkFadeProgress(Progress);
			}
		}
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Special room '%s' enemy died: %s (alive=%d, eliminated=%d, target=%d)."),
		*GetName(),
		*DeadEnemyBase->GetName(),
		AliveEnemyCount,
		EliminatedEnemyCount,
		TargetEliminateCount);

	// 全部打完 → 清场：沿用基类的墨水坑完成、OnRoomCleared 广播、奖励与管理器事件
	if (AliveEnemyCount <= 0)
	{
		CheckRoomClear();
	}
}

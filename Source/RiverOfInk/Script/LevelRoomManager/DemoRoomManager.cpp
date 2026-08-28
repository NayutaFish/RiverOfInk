// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelRoomManager/DemoRoomManager.h"

#include "Components/SceneComponent.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "LevelRoomManager/EnemySpawnPoint.h"
#include "NiagaraFunctionLibrary.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"
#include "RiverOfInk.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "TimerManager.h"

/** 刷怪特效播放后延迟生成敌人的时长（秒） */
static constexpr float SpawnVFXDelaySeconds = 1.0f;

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

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URoguelikeRunFlowSubsystem* RunFlow = GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>())
		{
			if (RunFlow->IsPreparationRoomMap(World))
			{
				UE_LOG(LogRoguelike, Log,
					TEXT("Preparation room detected; skipping enemy spawn setup."));
				return;
			}

			if (RunFlow->GetRunState() == ERoguelikeRunState::InRoom
				&& RunFlow->GetCurrentRoomDefinition().RoomType != ERoguelikeRoomType::Combat)
			{
				UE_LOG(LogRoguelike, Log,
					TEXT("Non-combat room detected; skipping DemoRoomManager enemy spawn setup. RoomType=%d."),
					static_cast<int32>(RunFlow->GetCurrentRoomDefinition().RoomType));
				return;
			}
		}
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

	// 给每个出生点分配本局刷怪数，保证清完后墨水坑刚好溶解完。
	{
		const int32 BaseCount = MaxEnemySpawnCount / SpawnPoints.Num();
		int32 Remainder = MaxEnemySpawnCount % SpawnPoints.Num();
		for (AEnemySpawnPoint* SpawnPoint : SpawnPoints)
		{
			if (!IsValid(SpawnPoint))
			{
				continue;
			}

			const int32 Count = BaseCount + (Remainder > 0 ? 1 : 0);
			if (Remainder > 0)
			{
				--Remainder;
			}
			SpawnPoint->AssignSpawnCount(Count);
		}
	}

	bRoomStarted = true;
	bRoomCleared = false;

	UE_LOG(LogRiverOfInk, Log, TEXT("Room started."));
	OnRoomStarted.Broadcast();
	FEventBus::Publish<FCombatRoomStartedEvent>(FCombatRoomStartedEvent());

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

		const FTransform SpawnTransform = SpawnPoint->GetSpawnTransform();

		// 刷怪前在刷怪点播放诞生特效（未配置则跳过）
		if (SpawnVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, SpawnVFX, SpawnTransform.GetLocation());
		}

		// 特效播放 1 秒后再真正生成敌人（配额在生成时扣除）
		FTimerHandle SpawnDelayTimerHandle;
		GetWorldTimerManager().SetTimer(
			SpawnDelayTimerHandle,
                  FTimerDelegate::CreateWeakLambda(this, [this, ClassToSpawn, SpawnTransform, SpawnPoint]()
                  {
                          if (SpawnEnemy(ClassToSpawn, SpawnTransform))
                          {
                                  SpawnPoint->NotifyEnemySpawned();
                          }
                  }),
			SpawnVFXDelaySeconds,
			false);
	}
}

bool ADemoRoomManager::SpawnEnemy(TSubclassOf<AEnemyBase> ClassToSpawn, const FTransform& SpawnTransform)
{
if (bRoomCleared || SpawnQuota <= 0 || !ClassToSpawn)
{
return false;
}

UWorld* World = GetWorld();
if (!World)
{
return false;
}

AEnemyBase* SpawnedEnemy = World->SpawnActor<AEnemyBase>(ClassToSpawn, SpawnTransform);
if (!IsValid(SpawnedEnemy))
{
return false;
}

SpawnedEnemy->OnEnemyDeath.AddDynamic(this, &ADemoRoomManager::HandleEnemyDeath);

ActiveEnemyList.Add(SpawnedEnemy);
++AliveEnemyCount;
--SpawnQuota;

UE_LOG(LogRiverOfInk, Log, TEXT("Spawned enemy: %s (alive=%d, quota=%d)"),
*SpawnedEnemy->GetName(), AliveEnemyCount, SpawnQuota);

return true;
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

	// 清场时让所有墨水坑直接完全溶解。
	for (AEnemySpawnPoint* SpawnPoint : SpawnPoints)
	{
		if (IsValid(SpawnPoint))
		{
			SpawnPoint->CompleteInkFade();
		}
	}

	// 停止刷新计时器
	GetWorldTimerManager().ClearTimer(SpawnCheckTimerHandle);
	OnRoomCleared.Broadcast();

	UE_LOG(LogRiverOfInk, Log, TEXT("Room Clear!"));
	FEventBus::Publish<FCombatRoomClearedEvent>(FCombatRoomClearedEvent());
	if (RewardManager)
	{
		UE_LOG(LogRoguelike, Log, TEXT("Room clear reached target; requesting reward UI. Eliminated=%d Target=%d."),
			EliminatedEnemyCount, TargetEliminateCount);
		RewardManager->ShowRewardAfterRoomClear();
	}
	OnRoomClear();
}

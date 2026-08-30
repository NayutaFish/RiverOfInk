// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelRoomManager/TwoStageArcVFXTestManager.h"

#include "Components/SceneComponent.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LevelRoomManager/EnemySpawnPoint.h"
#include "RiverOfInk.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATwoStageArcVFXTestManager::ATwoStageArcVFXTestManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	static ConstructorHelpers::FClassFinder<AEnemyBase> EnemyClassFinder(
		TEXT("/Game/Blueprint/GamePlay/Enemy/EnemyTest1/BP_EnemyTest1"));
	if (EnemyClassFinder.Succeeded())
	{
		EnemyClass = EnemyClassFinder.Class;
	}
}

void ATwoStageArcVFXTestManager::BeginPlay()
{
	Super::BeginPlay();

	ResolveSpawnPoint();
	if (!IsValid(SpawnPoint))
	{
		UE_LOG(LogRiverOfInk, Error,
			TEXT("TwoStageArcVFXTestManager has no valid EnemySpawnPoint."));
		return;
	}

	if (InitialSpawnDelay > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&ATwoStageArcVFXTestManager::SpawnTestEnemy,
			InitialSpawnDelay,
			false);
	}
	else
	{
		SpawnTestEnemy();
	}
}

void ATwoStageArcVFXTestManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bShuttingDown = true;
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);

	if (IsValid(ActiveEnemy))
	{
		ActiveEnemy->OnEnemyDeath.RemoveAll(this);
	}
	ActiveEnemy = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ATwoStageArcVFXTestManager::ResolveSpawnPoint()
{
	if (IsValid(SpawnPoint))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> FoundSpawnPoints;
	UGameplayStatics::GetAllActorsOfClass(
		World,
		AEnemySpawnPoint::StaticClass(),
		FoundSpawnPoints);

	for (AActor* Actor : FoundSpawnPoints)
	{
		if (AEnemySpawnPoint* Candidate = Cast<AEnemySpawnPoint>(Actor))
		{
			SpawnPoint = Candidate;
			break;
		}
	}
}

void ATwoStageArcVFXTestManager::SpawnTestEnemy()
{
	if (bShuttingDown)
	{
		return;
	}

	if (IsValid(ActiveEnemy))
	{
		if (!ActiveEnemy->bIsDead)
		{
			return;
		}

		ActiveEnemy->OnEnemyDeath.RemoveAll(this);
		ActiveEnemy = nullptr;
	}

	ResolveSpawnPoint();
	if (!IsValid(SpawnPoint) || !EnemyClass)
	{
		UE_LOG(LogRiverOfInk, Error,
			TEXT("TwoStageArcVFXTestManager cannot spawn: EnemyClass or SpawnPoint is missing."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemyBase* SpawnedEnemy = World->SpawnActor<AEnemyBase>(
		EnemyClass,
		SpawnPoint->GetSpawnTransform(),
		SpawnParameters);
	if (!IsValid(SpawnedEnemy))
	{
		UE_LOG(LogRiverOfInk, Error,
			TEXT("TwoStageArcVFXTestManager failed to spawn test enemy."));
		return;
	}

	SpawnedEnemy->OnEnemyDeath.AddUniqueDynamic(
		this,
		&ATwoStageArcVFXTestManager::HandleEnemyDeath);

	if (bFreezeTestEnemy)
	{
		SpawnedEnemy->SetActorTickEnabled(false);
	}

	ActiveEnemy = SpawnedEnemy;
	++SpawnedEnemyCount;

	UE_LOG(LogRiverOfInk, Log,
		TEXT("TwoStageArcVFXTestManager spawned enemy: %s (cycle=%d, respawn=%.2fs)."),
		*GetNameSafe(SpawnedEnemy),
		SpawnedEnemyCount,
		RespawnDelay);
}

void ATwoStageArcVFXTestManager::HandleEnemyDeath(AActor* DeadEnemy)
{
	if (DeadEnemy != ActiveEnemy.Get())
	{
		return;
	}

	if (AEnemyBase* DeadEnemyBase = Cast<AEnemyBase>(DeadEnemy))
	{
		DeadEnemyBase->OnEnemyDeath.RemoveAll(this);
	}
	ActiveEnemy = nullptr;

	if (bShuttingDown)
	{
		return;
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("TwoStageArcVFXTestManager received enemy death; respawning in %.2fs."),
		RespawnDelay);

	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&ATwoStageArcVFXTestManager::SpawnTestEnemy,
		RespawnDelay,
		false);
}

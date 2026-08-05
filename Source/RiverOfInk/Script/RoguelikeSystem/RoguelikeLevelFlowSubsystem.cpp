// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeLevelFlowSubsystem.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "RoguelikeSystem/RoguelikeExitTrigger.h"
#include "RoguelikeSystem/RoguelikeRuntimeDataSubsystem.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY(LogRoguelikeLevelFlow);

void URoguelikeLevelFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentMajorLevelId = PreparationLevelId;
	CurrentMinorLevelIndex = INDEX_NONE;
	ActiveMinorLevelSequence.Reset();
	ConfigureDefaultWhiteboxMapsIfUnset();

	const int32 Seed = RandomSeed != 0 ? RandomSeed : FMath::Rand();
	LevelRandomStream.Initialize(Seed);

	UE_LOG(LogRoguelikeLevelFlow, Log,
		TEXT("Level flow initialized. CurrentMajor=%d Seed=%d."),
		CurrentMajorLevelId, Seed);
}

void URoguelikeLevelFlowSubsystem::ConfigureDefaultWhiteboxMapsIfUnset()
{
	if (!PreparationLevel.IsNull() || !MajorLevelPools.IsEmpty())
	{
		return;
	}

	// Temporary whitebox defaults for the maps currently in the repository:
	// TestMap_0 is preparation, then TestMap_1/2/3 are major levels 0/1/2.
	// These remain soft references so opening the preparation map does not load
	// every candidate map into memory.
	PreparationLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Level/TestMap_0.TestMap_0")));

	const TCHAR* MajorMapPaths[] =
	{
		TEXT("/Game/Level/TestMap_1.TestMap_1"),
		TEXT("/Game/Level/TestMap_2.TestMap_2"),
		TEXT("/Game/Level/TestMap_3.TestMap_3")
	};

	for (int32 MajorLevelId = FirstMajorLevelId; MajorLevelId <= LastMajorLevelId; ++MajorLevelId)
	{
		FRoguelikeMinorLevelPool Pool;
		Pool.SequenceLength = 1;
		Pool.LevelPool.Add(TSoftObjectPtr<UWorld>(FSoftObjectPath(MajorMapPaths[MajorLevelId])));
		MajorLevelPools.Add(MajorLevelId, MoveTemp(Pool));
	}

	UE_LOG(LogRoguelikeLevelFlow, Warning,
		TEXT("No level-flow configuration was supplied; using TestMap_0 preparation and TestMap_1/2/3 whitebox defaults."));
}

void URoguelikeLevelFlowSubsystem::Deinitialize()
{
	ActiveMinorLevelSequence.Reset();
	CurrentMinorLevelIndex = INDEX_NONE;
	CurrentMajorLevelId = PreparationLevelId;

	Super::Deinitialize();
}

void URoguelikeLevelFlowSubsystem::SetPreparationLevel(TSoftObjectPtr<UWorld> InPreparationLevel)
{
	PreparationLevel = MoveTemp(InPreparationLevel);
	UE_LOG(LogRoguelikeLevelFlow, Log,
		TEXT("Preparation level configured: %s."),
		*PreparationLevel.ToSoftObjectPath().ToString());
}

void URoguelikeLevelFlowSubsystem::SetMajorLevelPool(
	int32 MajorLevelId,
	const FRoguelikeMinorLevelPool& InPool
)
{
	if (MajorLevelId < FirstMajorLevelId || MajorLevelId > LastMajorLevelId)
	{
		UE_LOG(LogRoguelikeLevelFlow, Warning,
			TEXT("Rejected major level pool id %d. Expected range is %d..%d."),
			MajorLevelId, FirstMajorLevelId, LastMajorLevelId);
		return;
	}

	MajorLevelPools.Add(MajorLevelId, InPool);
	UE_LOG(LogRoguelikeLevelFlow, Log,
		TEXT("Major level pool configured: Major=%d PoolSize=%d SequenceLength=%d."),
		MajorLevelId, InPool.LevelPool.Num(), InPool.SequenceLength);
}

bool URoguelikeLevelFlowSubsystem::LoadPreparationLevel()
{
	ActiveMinorLevelSequence.Reset();
	CurrentMinorLevelIndex = INDEX_NONE;
	CurrentMajorLevelId = PreparationLevelId;

	return RequestLevelTravel(PreparationLevel, TEXT("Preparation"));
}

bool URoguelikeLevelFlowSubsystem::EnsurePreparationExit()
{
	if (CurrentMajorLevelId != PreparationLevelId)
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogRoguelikeLevelFlow, Error,
			TEXT("Cannot create preparation exit: World is unavailable."));
		return false;
	}

	TArray<AActor*> ExistingExits;
	UGameplayStatics::GetAllActorsOfClass(World, ARoguelikeExitTrigger::StaticClass(), ExistingExits);
	if (ExistingExits.Num() > 0)
	{
		if (ARoguelikeExitTrigger* ExistingExit = Cast<ARoguelikeExitTrigger>(ExistingExits[0]))
		{
			ExistingExit->ActivateExit();
			UE_LOG(LogRoguelikeLevelFlow, Log,
				TEXT("Preparation exit found and activated. Major=%d."), CurrentMajorLevelId);
			return true;
		}
	}

	if (!ExitTriggerClass)
	{
		ExitTriggerClass = ARoguelikeExitTrigger::StaticClass();
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const FVector SpawnLocation = (PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector)
		+ PreparationExitSpawnOffset;
	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	ARoguelikeExitTrigger* PreparationExit = World->SpawnActorDeferred<ARoguelikeExitTrigger>(
		ExitTriggerClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);
	if (!IsValid(PreparationExit))
	{
		UE_LOG(LogRoguelikeLevelFlow, Error,
			TEXT("Failed to spawn preparation exit at %s."), *SpawnLocation.ToString());
		return false;
	}

	UGameplayStatics::FinishSpawningActor(PreparationExit, SpawnTransform);
	PreparationExit->ActivateExit();
	UE_LOG(LogRoguelikeLevelFlow, Log,
		TEXT("Preparation exit spawned and activated. Major=%d Location=%s."),
		CurrentMajorLevelId, *SpawnLocation.ToString());
	return true;
}

bool URoguelikeLevelFlowSubsystem::AdvanceToNextMajorLevel()
{
	const int32 NextMajorLevelId = CurrentMajorLevelId + 1;
	if (NextMajorLevelId > LastMajorLevelId)
	{
		UE_LOG(LogRoguelikeLevelFlow, Log,
			TEXT("No next major level after %d. Completion handling remains outside level flow."),
			CurrentMajorLevelId);
		return false;
	}

	if (!BuildMinorLevelSequence(NextMajorLevelId))
	{
		return false;
	}

	CurrentMajorLevelId = NextMajorLevelId;
	CurrentMinorLevelIndex = INDEX_NONE;

	UE_LOG(LogRoguelikeLevelFlow, Log,
		TEXT("Advanced to major level %d. Generated %d minor level(s)."),
		CurrentMajorLevelId, ActiveMinorLevelSequence.Num());

	return AdvanceToNextMinorLevel();
}

bool URoguelikeLevelFlowSubsystem::AdvanceToNextMinorLevel()
{
	const int32 NextMinorLevelIndex = CurrentMinorLevelIndex + 1;
	if (!ActiveMinorLevelSequence.IsValidIndex(NextMinorLevelIndex))
	{
		UE_LOG(LogRoguelikeLevelFlow, Log,
			TEXT("No next minor level. Major=%d CurrentMinorIndex=%d SequenceSize=%d."),
			CurrentMajorLevelId, CurrentMinorLevelIndex, ActiveMinorLevelSequence.Num());
		return false;
	}

	const TSoftObjectPtr<UWorld>& NextLevel = ActiveMinorLevelSequence[NextMinorLevelIndex];
	if (!RequestLevelTravel(NextLevel, TEXT("Minor level")))
	{
		return false;
	}

	CurrentMinorLevelIndex = NextMinorLevelIndex;
	UE_LOG(LogRoguelikeLevelFlow, Log,
		TEXT("Loading minor level. Major=%d MinorIndex=%d Asset=%s."),
		CurrentMajorLevelId,
		CurrentMinorLevelIndex,
		*NextLevel.ToSoftObjectPath().ToString());
	return true;
}

bool URoguelikeLevelFlowSubsystem::AdvanceToNextLevel()
{
	if (HasNextMinorLevel())
	{
		return AdvanceToNextMinorLevel();
	}

	return AdvanceToNextMajorLevel();
}

bool URoguelikeLevelFlowSubsystem::HasNextMinorLevel() const
{
	return ActiveMinorLevelSequence.IsValidIndex(CurrentMinorLevelIndex + 1);
}

bool URoguelikeLevelFlowSubsystem::HasNextMajorLevel() const
{
	return CurrentMajorLevelId < LastMajorLevelId;
}

bool URoguelikeLevelFlowSubsystem::BuildMinorLevelSequence(int32 MajorLevelId)
{
	const FRoguelikeMinorLevelPool* Pool = MajorLevelPools.Find(MajorLevelId);
	if (!Pool)
	{
		UE_LOG(LogRoguelikeLevelFlow, Error,
			TEXT("Cannot build minor level sequence: no pool configured for Major=%d."),
			MajorLevelId);
		return false;
	}

	if (Pool->LevelPool.IsEmpty())
	{
		UE_LOG(LogRoguelikeLevelFlow, Error,
			TEXT("Cannot build minor level sequence: pool is empty for Major=%d."),
			MajorLevelId);
		return false;
	}

	if (Pool->SequenceLength <= 0)
	{
		UE_LOG(LogRoguelikeLevelFlow, Error,
			TEXT("Cannot build minor level sequence: SequenceLength=%d for Major=%d."),
			Pool->SequenceLength, MajorLevelId);
		return false;
	}

	// Clearing this array releases the previous major level's sequence. Soft
	// object pointers do not keep the map package loaded by themselves.
	ActiveMinorLevelSequence.Reset();

	TArray<TSoftObjectPtr<UWorld>> Candidates = Pool->LevelPool;
	const int32 SequenceLength = FMath::Min(Pool->SequenceLength, Candidates.Num());
	if (Pool->SequenceLength > Candidates.Num())
	{
		UE_LOG(LogRoguelikeLevelFlow, Warning,
			TEXT("SequenceLength=%d exceeds pool size=%d for Major=%d; clamping to pool size."),
			Pool->SequenceLength, Candidates.Num(), MajorLevelId);
	}

	while (ActiveMinorLevelSequence.Num() < SequenceLength)
	{
		const int32 CandidateIndex = LevelRandomStream.RandRange(0, Candidates.Num() - 1);
		ActiveMinorLevelSequence.Add(Candidates[CandidateIndex]);
		Candidates.RemoveAtSwap(CandidateIndex);
	}

	return ActiveMinorLevelSequence.Num() > 0;
}

bool URoguelikeLevelFlowSubsystem::RequestLevelTravel(
	const TSoftObjectPtr<UWorld>& LevelAsset,
	const TCHAR* TransitionReason
)
{
	const FSoftObjectPath LevelPath = LevelAsset.ToSoftObjectPath();
	const FString PackageName = LevelPath.GetLongPackageName();
	if (PackageName.IsEmpty())
	{
		UE_LOG(LogRoguelikeLevelFlow, Error,
			TEXT("Cannot request %s level travel: map asset is not configured."),
			TransitionReason);
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogRoguelikeLevelFlow, Error,
			TEXT("Cannot request %s level travel: GameInstance is unavailable."),
			TransitionReason);
		return false;
	}

	if (UWorld* CurrentWorld = GameInstance->GetWorld())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(CurrentWorld, 0)))
		{
			if (URoguelikeRuntimeDataSubsystem* RuntimeData = GameInstance->GetSubsystem<URoguelikeRuntimeDataSubsystem>())
			{
				RuntimeData->CapturePlayerRuntimeData(Player);
			}
		}
	}

	UE_LOG(LogRoguelikeLevelFlow, Log,
		TEXT("Requesting %s level travel: %s."),
		TransitionReason, *PackageName);
	UGameplayStatics::OpenLevel(GameInstance, FName(*PackageName));
	return true;
}

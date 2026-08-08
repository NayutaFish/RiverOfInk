// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"
#include "RoguelikeSystem/RoguelikeExitTrigger.h"
#include "RoguelikeSystem/RoguelikeRuntimeDataSubsystem.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY(LogRoguelikeRunFlow);

void URoguelikeRunFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<URoguelikeEconomySubsystem>();

	ResetRunProgress();
	CurrentRunState = ERoguelikeRunState::MainMenu;
	LastTransitionReason = ERoguelikeRunTransitionReason::None;
	ConfigureDefaultWhiteboxRoomsIfUnset();

	const int32 Seed = RandomSeed != 0 ? RandomSeed : FMath::Rand();
	RoomRandomStream.Initialize(Seed);

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Run flow initialized. State=%d Seed=%d."),
		static_cast<int32>(CurrentRunState), Seed);
}

void URoguelikeRunFlowSubsystem::Deinitialize()
{
	ResetRunProgress();
	CurrentRunState = ERoguelikeRunState::MainMenu;
	LastTransitionReason = ERoguelikeRunTransitionReason::None;

	Super::Deinitialize();
}

void URoguelikeRunFlowSubsystem::SetPreparationRoomMap(TSoftObjectPtr<UWorld> InPreparationRoomMap)
{
	PreparationRoomMap = MoveTemp(InPreparationRoomMap);
	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Preparation room configured: %s."),
		*PreparationRoomMap.ToSoftObjectPath().ToString());
}

void URoguelikeRunFlowSubsystem::SetMajorStageDefinition(
	int32 MajorStageIndex,
	const FMajorStageDefinition& InDefinition
)
{
	if (MajorStageIndex < FirstMajorStageIndex || MajorStageIndex > LastMajorStageIndex)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Rejected major-stage definition index %d. Expected range is %d..%d."),
			MajorStageIndex, FirstMajorStageIndex, LastMajorStageIndex);
		return;
	}

	FMajorStageDefinition Definition = InDefinition;
	if (Definition.MajorStageIndex != MajorStageIndex)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Major-stage definition index mismatch. Key=%d Definition=%d; using key."),
			MajorStageIndex, Definition.MajorStageIndex);
		Definition.MajorStageIndex = MajorStageIndex;
	}

	MajorStageDefinitions.Add(MajorStageIndex, MoveTemp(Definition));
	const FMajorStageDefinition& StoredDefinition = MajorStageDefinitions.FindChecked(MajorStageIndex);
	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Major stage configured: Index=%d PoolSize=%d SequenceLength=%d."),
		MajorStageIndex,
		StoredDefinition.RoomPool.Num(),
		StoredDefinition.RoomSequenceLength);
}

bool URoguelikeRunFlowSubsystem::LoadPreparationRoom()
{
	if (CurrentRunState == ERoguelikeRunState::LoadingRoom)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Preparation-room request rejected while a room is already loading."));
		return false;
	}

	if (!ResetPlayerRuntimeData())
	{
		return false;
	}

	if (!ResetEconomyData())
	{
		return false;
	}

	ResetRunProgress();
	return RequestMapTravel(
		PreparationRoomMap,
		ERoguelikeRunTransitionReason::ReturnToMainMenu,
		TEXT("preparation room"),
		false
	);
}

bool URoguelikeRunFlowSubsystem::EnsurePreparationStartExit()
{
	if (CurrentRunState != ERoguelikeRunState::Preparation)
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot create preparation start exit: World is unavailable."));
		return false;
	}

	TArray<AActor*> ExistingExits;
	UGameplayStatics::GetAllActorsOfClass(World, ARoguelikeExitTrigger::StaticClass(), ExistingExits);
	if (ExistingExits.Num() > 0)
	{
		if (ARoguelikeExitTrigger* ExistingExit = Cast<ARoguelikeExitTrigger>(ExistingExits[0]))
		{
			ExistingExit->ActivateExit();
			UE_LOG(LogRoguelikeRunFlow, Log,
				TEXT("Preparation start exit found and activated."));
			return true;
		}
	}

	if (!PreparationExitTriggerClass)
	{
		PreparationExitTriggerClass = ARoguelikeExitTrigger::StaticClass();
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const FVector SpawnLocation = (PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector)
		+ PreparationExitSpawnOffset;
	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	ARoguelikeExitTrigger* PreparationExit = World->SpawnActorDeferred<ARoguelikeExitTrigger>(
		PreparationExitTriggerClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);
	if (!IsValid(PreparationExit))
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Failed to spawn preparation start exit at %s."), *SpawnLocation.ToString());
		return false;
	}

	UGameplayStatics::FinishSpawningActor(PreparationExit, SpawnTransform);
	PreparationExit->ActivateExit();
	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Preparation start exit spawned and activated at %s."), *SpawnLocation.ToString());
	return true;
}

bool URoguelikeRunFlowSubsystem::StartNewRun()
{
	if (CurrentRunState != ERoguelikeRunState::MainMenu
		&& CurrentRunState != ERoguelikeRunState::Preparation
		&& CurrentRunState != ERoguelikeRunState::Result)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("StartNewRun rejected in state %d."), static_cast<int32>(CurrentRunState));
		return false;
	}

	return BeginNewRun(ERoguelikeRunTransitionReason::StartRun);
}

bool URoguelikeRunFlowSubsystem::RestartRun()
{
	if (CurrentRunState != ERoguelikeRunState::Result)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("RestartRun rejected in state %d; Result is required."), static_cast<int32>(CurrentRunState));
		return false;
	}

	return BeginNewRun(ERoguelikeRunTransitionReason::Restart);
}

bool URoguelikeRunFlowSubsystem::RequestAdvanceFromExit()
{
	if (CurrentRunState == ERoguelikeRunState::Preparation)
	{
		return StartNewRun();
	}

	if (CurrentRunState != ERoguelikeRunState::InRoom)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Exit advance rejected in state %d."), static_cast<int32>(CurrentRunState));
		return false;
	}

	return HasNextRoom() ? AdvanceToNextRoom() : AdvanceToNextMajorStage();
}

bool URoguelikeRunFlowSubsystem::AdvanceToNextRoom()
{
	if (CurrentRunState != ERoguelikeRunState::InRoom)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Next-room request rejected in state %d."), static_cast<int32>(CurrentRunState));
		return false;
	}

	const int32 NextRoomIndex = CurrentRoomIndex + 1;
	if (!ActiveRoomSequence.IsValidIndex(NextRoomIndex))
	{
		UE_LOG(LogRoguelikeRunFlow, Log,
			TEXT("No next room. MajorStage=%d CurrentRoom=%d SequenceSize=%d."),
			CurrentMajorStageIndex, CurrentRoomIndex, ActiveRoomSequence.Num());
		return false;
	}

	const int32 PreviousRoomIndex = CurrentRoomIndex;
	CurrentRoomIndex = NextRoomIndex;
	const FRoguelikeRoomDefinition& NextRoom = ActiveRoomSequence[CurrentRoomIndex];
	if (!RequestMapTravel(
		NextRoom.RoomMap,
		ERoguelikeRunTransitionReason::NextRoom,
		TEXT("next room"),
		true
	))
	{
		CurrentRoomIndex = PreviousRoomIndex;
		return false;
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Loading next room. MajorStage=%d RoomIndex=%d RoomId=%s."),
		CurrentMajorStageIndex, CurrentRoomIndex, *NextRoom.RoomId.ToString());
	return true;
}

bool URoguelikeRunFlowSubsystem::AdvanceToNextMajorStage()
{
	if (CurrentRunState != ERoguelikeRunState::InRoom)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Next-major-stage request rejected in state %d."), static_cast<int32>(CurrentRunState));
		return false;
	}

	const int32 NextMajorStageIndex = CurrentMajorStageIndex + 1;
	if (NextMajorStageIndex > LastMajorStageIndex)
	{
		if (!CaptureCurrentPlayerRuntimeData())
		{
			return false;
		}

		RunOutcome = ERoguelikeRunOutcome::Victory;
		return TransitionRunState(
			ERoguelikeRunState::Result,
			ERoguelikeRunTransitionReason::RunCompleted
		);
	}

	TArray<FRoguelikeRoomDefinition> NextSequence;
	if (!BuildRoomSequence(NextMajorStageIndex, NextSequence))
	{
		return false;
	}

	const int32 PreviousMajorStageIndex = CurrentMajorStageIndex;
	const int32 PreviousRoomIndex = CurrentRoomIndex;
	TArray<FRoguelikeRoomDefinition> PreviousSequence = ActiveRoomSequence;

	CurrentMajorStageIndex = NextMajorStageIndex;
	CurrentRoomIndex = 0;
	ActiveRoomSequence = MoveTemp(NextSequence);

	const FRoguelikeRoomDefinition& FirstRoom = ActiveRoomSequence[CurrentRoomIndex];
	if (!RequestMapTravel(
		FirstRoom.RoomMap,
		ERoguelikeRunTransitionReason::NextMajorStage,
		TEXT("next major stage"),
		true
	))
	{
		CurrentMajorStageIndex = PreviousMajorStageIndex;
		CurrentRoomIndex = PreviousRoomIndex;
		ActiveRoomSequence = MoveTemp(PreviousSequence);
		return false;
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Advanced to MajorStage=%d. GeneratedRooms=%d FirstRoom=%s."),
		CurrentMajorStageIndex,
		ActiveRoomSequence.Num(),
		*FirstRoom.RoomId.ToString());
	return true;
}

bool URoguelikeRunFlowSubsystem::NotifyRoomLoaded(UWorld* LoadedWorld)
{
	if (!IsValid(LoadedWorld))
	{
		UE_LOG(LogRoguelikeRunFlow, Error, TEXT("Cannot notify room loaded: World is invalid."));
		return false;
	}

	if (IsPreparationRoomMap(LoadedWorld))
	{
		if (CurrentRunState == ERoguelikeRunState::Preparation)
		{
			return true;
		}

		return TransitionRunState(
			ERoguelikeRunState::Preparation,
			ERoguelikeRunTransitionReason::EnterPreparation
		);
	}

	if (CurrentRunState == ERoguelikeRunState::InRoom)
	{
		return true;
	}

	if (CurrentRunState != ERoguelikeRunState::LoadingRoom)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Room load notification ignored in state %d. World=%s."),
			static_cast<int32>(CurrentRunState), *LoadedWorld->GetName());
		return false;
	}

	return TransitionRunState(
		ERoguelikeRunState::InRoom,
		ERoguelikeRunTransitionReason::RoomLoaded
	);
}

bool URoguelikeRunFlowSubsystem::HasNextRoom() const
{
	return ActiveRoomSequence.IsValidIndex(CurrentRoomIndex + 1);
}

bool URoguelikeRunFlowSubsystem::HasNextMajorStage() const
{
	return CurrentMajorStageIndex >= FirstMajorStageIndex
		&& CurrentMajorStageIndex < LastMajorStageIndex;
}

FRoguelikeRoomDefinition URoguelikeRunFlowSubsystem::GetCurrentRoomDefinition() const
{
	return ActiveRoomSequence.IsValidIndex(CurrentRoomIndex)
		? ActiveRoomSequence[CurrentRoomIndex]
		: FRoguelikeRoomDefinition();
}

void URoguelikeRunFlowSubsystem::ConfigureDefaultWhiteboxRoomsIfUnset()
{
	if (!PreparationRoomMap.IsNull() || !MajorStageDefinitions.IsEmpty())
	{
		return;
	}

	// Temporary whitebox configuration. The labels are game-domain RoomIds;
	// the TestMap asset names remain UE map asset paths.
	PreparationRoomMap = TSoftObjectPtr<UWorld>(
		FSoftObjectPath(TEXT("/Game/Level/TestMap_0.TestMap_0"))
	);

	const TCHAR* MajorStageRoomPaths[] =
	{
		TEXT("/Game/Level/TestMap_1.TestMap_1"),
		TEXT("/Game/Level/TestMap_2.TestMap_2"),
		TEXT("/Game/Level/TestMap_3.TestMap_3")
	};

	for (int32 MajorStageIndex = FirstMajorStageIndex;
		MajorStageIndex <= LastMajorStageIndex;
		++MajorStageIndex)
	{
		FMajorStageDefinition Definition;
		Definition.MajorStageIndex = MajorStageIndex;
		Definition.RoomSequenceLength = 1;

		FRoguelikeRoomDefinition Room;
		Room.RoomId = FName(*FString::Printf(TEXT("M%02d_Combat_A"), MajorStageIndex + 1));
		Room.RoomType = ERoguelikeRoomType::Combat;
		Room.SelectionWeight = 1;
		Room.RoomMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(MajorStageRoomPaths[MajorStageIndex]));
		Definition.RoomPool.Add(MoveTemp(Room));

		MajorStageDefinitions.Add(MajorStageIndex, MoveTemp(Definition));
	}

	UE_LOG(LogRoguelikeRunFlow, Warning,
		TEXT("No run-flow configuration was supplied; using TestMap_0 preparation and TestMap_1/2/3 whitebox room defaults."));
}

void URoguelikeRunFlowSubsystem::ResetRunProgress()
{
	CurrentMajorStageIndex = INDEX_NONE;
	CurrentRoomIndex = INDEX_NONE;
	ActiveRoomSequence.Reset();
	RunOutcome = ERoguelikeRunOutcome::None;
}

bool URoguelikeRunFlowSubsystem::BeginNewRun(ERoguelikeRunTransitionReason Reason)
{
	if (!ResetPlayerRuntimeData())
	{
		return false;
	}

	if (!ResetEconomyData())
	{
		return false;
	}

	ResetRunProgress();

	TArray<FRoguelikeRoomDefinition> FirstMajorStageSequence;
	if (!BuildRoomSequence(FirstMajorStageIndex, FirstMajorStageSequence))
	{
		return false;
	}

	CurrentMajorStageIndex = FirstMajorStageIndex;
	CurrentRoomIndex = 0;
	ActiveRoomSequence = MoveTemp(FirstMajorStageSequence);

	const FRoguelikeRoomDefinition& FirstRoom = ActiveRoomSequence[CurrentRoomIndex];
	if (!RequestMapTravel(FirstRoom.RoomMap, Reason, TEXT("new run first room"), false))
	{
		ResetRunProgress();
		return false;
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("New run started. MajorStage=%d RoomIndex=%d RoomId=%s."),
		CurrentMajorStageIndex, CurrentRoomIndex, *FirstRoom.RoomId.ToString());
	return true;
}

bool URoguelikeRunFlowSubsystem::BuildRoomSequence(
	int32 MajorStageIndex,
	TArray<FRoguelikeRoomDefinition>& OutSequence
)
{
	OutSequence.Reset();

	const FMajorStageDefinition* Definition = MajorStageDefinitions.Find(MajorStageIndex);
	if (!Definition)
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot build room sequence: no major-stage definition for Index=%d."), MajorStageIndex);
		return false;
	}

	if (Definition->RoomSequenceLength <= 0)
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot build room sequence: RoomSequenceLength=%d for MajorStage=%d."),
			Definition->RoomSequenceLength, MajorStageIndex);
		return false;
	}

	TArray<FRoguelikeRoomDefinition> Candidates;
	for (const FRoguelikeRoomDefinition& Room : Definition->RoomPool)
	{
		if (Room.RoomId.IsNone())
		{
			UE_LOG(LogRoguelikeRunFlow, Warning,
				TEXT("Skipping unnamed room in MajorStage=%d."), MajorStageIndex);
			continue;
		}

		if (Room.RoomMap.IsNull())
		{
			UE_LOG(LogRoguelikeRunFlow, Warning,
				TEXT("Skipping RoomId=%s because its RoomMap is not configured."), *Room.RoomId.ToString());
			continue;
		}

		Candidates.Add(Room);
	}

	if (Candidates.IsEmpty())
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot build room sequence: MajorStage=%d has no valid room candidates."), MajorStageIndex);
		return false;
	}

	const int32 RequestedLength = Definition->RoomSequenceLength;
	const int32 SequenceLength = FMath::Min(RequestedLength, Candidates.Num());
	if (RequestedLength > Candidates.Num())
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("RoomSequenceLength=%d exceeds valid pool size=%d for MajorStage=%d; drawing without replacement."),
			RequestedLength, Candidates.Num(), MajorStageIndex);
	}

	while (OutSequence.Num() < SequenceLength)
	{
		double TotalWeight = 0.0;
		for (const FRoguelikeRoomDefinition& Candidate : Candidates)
		{
			TotalWeight += static_cast<double>(FMath::Max(1, Candidate.SelectionWeight));
		}

		const double Roll = static_cast<double>(RoomRandomStream.FRandRange(0.0f, static_cast<float>(TotalWeight)));
		double AccumulatedWeight = 0.0;
		int32 SelectedIndex = Candidates.Num() - 1;
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			AccumulatedWeight += static_cast<double>(FMath::Max(1, Candidates[CandidateIndex].SelectionWeight));
			if (Roll <= AccumulatedWeight)
			{
				SelectedIndex = CandidateIndex;
				break;
			}
		}

		OutSequence.Add(Candidates[SelectedIndex]);
		Candidates.RemoveAtSwap(SelectedIndex);
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Built room sequence. MajorStage=%d RoomCount=%d."), MajorStageIndex, OutSequence.Num());
	return !OutSequence.IsEmpty();
}

bool URoguelikeRunFlowSubsystem::RequestMapTravel(
	const TSoftObjectPtr<UWorld>& MapAsset,
	ERoguelikeRunTransitionReason Reason,
	const TCHAR* TransitionDescription,
	bool bCapturePlayerData
)
{
	const FSoftObjectPath MapPath = MapAsset.ToSoftObjectPath();
	const FString PackageName = MapPath.GetLongPackageName();
	if (PackageName.IsEmpty())
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot request %s travel: RoomMap is not configured."), TransitionDescription);
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot request %s travel: GameInstance is unavailable."), TransitionDescription);
		return false;
	}

	if (bCapturePlayerData && !CaptureCurrentPlayerRuntimeData())
	{
		return false;
	}

	if (!TransitionRunState(ERoguelikeRunState::LoadingRoom, Reason))
	{
		return false;
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Requesting %s travel: %s."), TransitionDescription, *PackageName);
	UGameplayStatics::OpenLevel(GameInstance, FName(*PackageName));
	return true;
}

bool URoguelikeRunFlowSubsystem::TransitionRunState(
	ERoguelikeRunState NextState,
	ERoguelikeRunTransitionReason Reason
)
{
	if (CurrentRunState == NextState)
	{
		LastTransitionReason = Reason;
		return true;
	}

	if (!IsTransitionAllowed(NextState))
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Rejected run-state transition: %d -> %d Reason=%d."),
			static_cast<int32>(CurrentRunState),
			static_cast<int32>(NextState),
			static_cast<int32>(Reason));
		return false;
	}

	const ERoguelikeRunState PreviousState = CurrentRunState;
	CurrentRunState = NextState;
	LastTransitionReason = Reason;

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Run state changed: %d -> %d Reason=%d MajorStage=%d Room=%d."),
		static_cast<int32>(PreviousState),
		static_cast<int32>(CurrentRunState),
		static_cast<int32>(Reason),
		CurrentMajorStageIndex,
		CurrentRoomIndex);
	OnRunStateChanged.Broadcast(PreviousState, CurrentRunState, Reason);
	return true;
}

bool URoguelikeRunFlowSubsystem::IsTransitionAllowed(ERoguelikeRunState NextState) const
{
	switch (CurrentRunState)
	{
	case ERoguelikeRunState::MainMenu:
		return NextState == ERoguelikeRunState::Preparation
			|| NextState == ERoguelikeRunState::LoadingRoom;

	case ERoguelikeRunState::Preparation:
		return NextState == ERoguelikeRunState::MainMenu
			|| NextState == ERoguelikeRunState::LoadingRoom;

	case ERoguelikeRunState::LoadingRoom:
		return NextState == ERoguelikeRunState::Preparation
			|| NextState == ERoguelikeRunState::InRoom
			|| NextState == ERoguelikeRunState::MainMenu
			|| NextState == ERoguelikeRunState::Result;

	case ERoguelikeRunState::InRoom:
		return NextState == ERoguelikeRunState::LoadingRoom
			|| NextState == ERoguelikeRunState::Result;

	case ERoguelikeRunState::Result:
		return NextState == ERoguelikeRunState::MainMenu
			|| NextState == ERoguelikeRunState::Preparation
			|| NextState == ERoguelikeRunState::LoadingRoom;

	default:
		return false;
	}
}

bool URoguelikeRunFlowSubsystem::IsPreparationRoomMap(const UWorld* World) const
{
	if (!IsValid(World) || PreparationRoomMap.IsNull())
	{
		return false;
	}

	const FString PreparationPackageName = PreparationRoomMap.ToSoftObjectPath().GetLongPackageName();
	const UPackage* WorldPackage = World->GetOutermost();
	const FString WorldPackageName = WorldPackage
		? UWorld::RemovePIEPrefix(WorldPackage->GetName())
		: FString();
	return WorldPackageName.Equals(PreparationPackageName, ESearchCase::CaseSensitive);
}

bool URoguelikeRunFlowSubsystem::ResetPlayerRuntimeData()
{
	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeRuntimeDataSubsystem* RuntimeData = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRuntimeDataSubsystem>()
		: nullptr;
	if (!RuntimeData)
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot reset player runtime data: runtime-data subsystem is unavailable."));
		return false;
	}

	RuntimeData->ResetPlayerRuntimeData();
	UE_LOG(LogRoguelikeRunFlow, Log, TEXT("Player runtime data reset for a new run boundary."));
	return true;
}

bool URoguelikeRunFlowSubsystem::ResetEconomyData()
{
	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeEconomySubsystem* Economy = GameInstance
		? GameInstance->GetSubsystem<URoguelikeEconomySubsystem>()
		: nullptr;
	if (!Economy)
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot reset Pure Ink economy: economy subsystem is unavailable."));
		return false;
	}

	Economy->ResetForNewRun();
	UE_LOG(LogRoguelikeRunFlow, Log, TEXT("Pure Ink economy reset for a new run boundary."));
	return true;
}

bool URoguelikeRunFlowSubsystem::CaptureCurrentPlayerRuntimeData()
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	APlayerCharacter* Player = World
		? Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(World, 0))
		: nullptr;
	if (!IsValid(Player))
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot capture player runtime data before travel: Player 0 is unavailable."));
		return false;
	}

	URoguelikeRuntimeDataSubsystem* RuntimeData = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRuntimeDataSubsystem>()
		: nullptr;
	if (!RuntimeData)
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Cannot capture player runtime data before travel: runtime-data subsystem is unavailable."));
		return false;
	}

	return RuntimeData->CapturePlayerRuntimeData(Player);
}

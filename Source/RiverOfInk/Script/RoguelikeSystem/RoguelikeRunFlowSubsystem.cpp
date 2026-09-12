// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"

#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Core/SettlementDataRecorder.h"
#include "Player/PlayerCharacter.h"
#include "RoguelikeSystem/LevelDataAsset.h"
#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"
#include "RoguelikeSystem/RoguelikeExitTrigger.h"
#include "RoguelikeSystem/RoguelikeRuntimeDataSubsystem.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY(LogRoguelikeRunFlow);

namespace
{
	/**
	 * 关卡数据资产的固定资产路径：在内容浏览器里建一个 ULevelDataAsset，
	 * 放到 Content/DataAsset/LevelData/ 下并命名为 DA_LevelData，把顺序关卡拖进它的 Levels 数组即可。
	 * 资产不存在（或列表为空）时退回白盒大关 / 房间池配置。
	 */
	const TCHAR* RunLevelDataAssetPath = TEXT("/Game/DataAsset/LevelData/DA_LevelData.DA_LevelData");

	bool HasConfiguredTravelMap(const TSoftObjectPtr<UWorld>& MapAsset)
	{
		return !MapAsset.ToSoftObjectPath().GetLongPackageName().IsEmpty();
	}
}

void URoguelikeRunFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<URoguelikeEconomySubsystem>();

	ResetRunProgress();
	CurrentRunState = ERoguelikeRunState::MainMenu;
	LastTransitionReason = ERoguelikeRunTransitionReason::None;
	ConfigureDefaultWhiteboxRoomsIfUnset();
	ResolveLevelDataAsset();

	// 新的游戏会话（含 PIE 重进）：清掉上一局残留的静态结算数据，
	// 否则结算/统计数值会跨会话继续累加。
	USettlementDataRecorder::Reset();

	const int32 Seed = RandomSeed != 0 ? RandomSeed : FMath::Rand();
	RoomRandomStream.Initialize(Seed);

	PlayerDiedDelegateHandle = FEventBus::Subscribe<FPlayerDiedEvent>(
		[this](const FPlayerDiedEvent& Event)
		{
			HandlePlayerDefeated(Event);
		});
	bPlayerDiedSubscribed = PlayerDiedDelegateHandle.IsValid();

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Run flow initialized. State=%d Seed=%d."),
		static_cast<int32>(CurrentRunState), Seed);
}

void URoguelikeRunFlowSubsystem::Deinitialize()
{
	if (bPlayerDiedSubscribed)
	{
		FEventBus::Unsubscribe<FPlayerDiedEvent>(PlayerDiedDelegateHandle);
		PlayerDiedDelegateHandle.Reset();
		bPlayerDiedSubscribed = false;
	}

	ResetRunProgress();
	CurrentRunState = ERoguelikeRunState::MainMenu;
	LastTransitionReason = ERoguelikeRunTransitionReason::None;

	Super::Deinitialize();
}

void URoguelikeRunFlowSubsystem::HandlePlayerDefeated(const FPlayerDiedEvent& Event)
{
	if (CurrentRunState != ERoguelikeRunState::InRoom)
	{
		UE_LOG(LogRoguelikeRunFlow, Verbose,
			TEXT("Player defeat ignored outside an active room. State=%d."),
			static_cast<int32>(CurrentRunState));
		return;
	}

	APlayerCharacter* DefeatedPlayer = Cast<APlayerCharacter>(Event.Player.Get());
	if (!IsValid(DefeatedPlayer))
	{
		UE_LOG(LogRoguelikeRunFlow, Verbose,
			TEXT("Player defeat ignored because the event does not identify a valid player character."));
		return;
	}

	if (APawn* ControlledPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (ControlledPawn != DefeatedPlayer)
		{
			UE_LOG(LogRoguelikeRunFlow, Verbose,
				TEXT("Player defeat ignored for an unpossessed pawn: %s."), *GetNameSafe(DefeatedPlayer));
			return;
		}
	}

	FinalizeCurrentRun(ERoguelikeRunOutcome::Defeat, ERoguelikeRunTransitionReason::PlayerDefeated);
}

bool URoguelikeRunFlowSubsystem::FinalizeCurrentRun(
	ERoguelikeRunOutcome Outcome,
	ERoguelikeRunTransitionReason Reason)
{
	if (CurrentRunState == ERoguelikeRunState::Result)
	{
		return true;
	}

	if (CurrentRunState != ERoguelikeRunState::InRoom
		|| !IsTransitionAllowed(ERoguelikeRunState::Result))
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Run finalization rejected. State=%d Outcome=%d Reason=%d."),
			static_cast<int32>(CurrentRunState),
			static_cast<int32>(Outcome),
			static_cast<int32>(Reason));
		return false;
	}

	// Result UI only reads the settlement snapshot, but preserve the final live
	// player state for systems which already consume runtime-data snapshots.
	if (!CaptureCurrentPlayerRuntimeData())
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Result finalization could not capture player runtime data; continuing with settlement data."));
	}

	ResultSnapshot = USettlementDataRecorder::CaptureSnapshot();
	USettlementDataRecorder::FreezeCollection();
	RunOutcome = Outcome;

	if (!TransitionRunState(ERoguelikeRunState::Result, Reason))
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Run finalization could not enter Result after snapshot capture."));
		return false;
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Run finalized. Outcome=%d Kills=%d Damage=%.0f Picks=%d."),
		static_cast<int32>(RunOutcome),
		ResultSnapshot.NonPlayerDeathCount,
		ResultSnapshot.TotalDamageDealtToNonPlayer,
		ResultSnapshot.RewardPicks.Num());
	return true;
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

	// Keep the visible Result and its frozen snapshot intact until the target is
	// known to be configured. A bad return-map reference must not strand the
	// player on a cleared page with disabled actions.
	if (!HasConfiguredTravelMap(PreparationRoomMap))
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("Preparation-room request rejected: the map is not configured."));
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
		TEXT("Loading next room. MajorStage=%d RoomIndex=%d RoomId=%s EncounterTier=%d."),
		CurrentMajorStageIndex,
		CurrentRoomIndex,
		*NextRoom.RoomId.ToString(),
		static_cast<int32>(NextRoom.EncounterTier));
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

	// 顺序关卡列表模式没有"大关"概念：走到最后一个关卡之后就算通关。
	// 终点的战斗关卡通常在清场时就已通关（见 CompleteRunFromFinalRoomClear 的调用方）；
	// 这里兜住"终点是商店房间（没有清场）"的情况：走完它的出口即通关。
	if (bOrderedLevelListMode)
	{
		return CompleteRunFromFinalRoomClear();
	}

	const int32 NextMajorStageIndex = CurrentMajorStageIndex + 1;
	if (NextMajorStageIndex > LastMajorStageIndex)
	{
		// 白盒大关模式下的终点：同样走"通关"入口（内部会填结算快照 + 广播通关事件）。
		return CompleteRunFromFinalRoomClear();
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
		TEXT("Advanced to MajorStage=%d. GeneratedRooms=%d FirstRoom=%s EncounterTier=%d."),
		CurrentMajorStageIndex,
		ActiveRoomSequence.Num(),
		*FirstRoom.RoomId.ToString(),
		static_cast<int32>(FirstRoom.EncounterTier));
	return true;
}

bool URoguelikeRunFlowSubsystem::DebugFinalizeRunForPIE(ERoguelikeRunOutcome Outcome)
{
	if (Outcome != ERoguelikeRunOutcome::Victory && Outcome != ERoguelikeRunOutcome::Defeat)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("PIE result helper rejected an invalid outcome %d."),
			static_cast<int32>(Outcome));
		return false;
	}

	const ERoguelikeRunTransitionReason Reason = Outcome == ERoguelikeRunOutcome::Victory
		? ERoguelikeRunTransitionReason::RunCompleted
		: ERoguelikeRunTransitionReason::PlayerDefeated;
	return FinalizeCurrentRun(Outcome, Reason);
}

bool URoguelikeRunFlowSubsystem::DebugActivateCurrentMapForPIE(UWorld* CurrentWorld)
{
	if (CurrentRunState == ERoguelikeRunState::InRoom)
	{
		return true;
	}

	if (CurrentRunState != ERoguelikeRunState::MainMenu || !IsValid(CurrentWorld))
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Direct-PIE room activation rejected. State=%d World=%s."),
			static_cast<int32>(CurrentRunState),
			*GetNameSafe(CurrentWorld));
		return false;
	}

	// 与 BeginNewRun 用同一套关卡来源：顺序关卡列表优先，没有数据时退回大关房间池。
	// 这样直接 PIE 打开关卡数据资产里的任意一关也能被正确接管。
	ResolveLevelDataAsset();

	TArray<FRoguelikeRoomDefinition> FirstMajorStageSequence;
	const bool bUseOrderedLevelList = BuildRoomSequenceFromLevelData(FirstMajorStageSequence);
	if (!bUseOrderedLevelList && !BuildRoomSequence(FirstMajorStageIndex, FirstMajorStageSequence))
	{
		return false;
	}

	const UPackage* WorldPackage = CurrentWorld->GetOutermost();
	const FString CurrentPackageName = WorldPackage
		? UWorld::RemovePIEPrefix(WorldPackage->GetName())
		: FString();
	const int32 CurrentRoomSequenceIndex = FirstMajorStageSequence.IndexOfByPredicate(
		[&CurrentPackageName](const FRoguelikeRoomDefinition& Room)
		{
			return Room.RoomMap.ToSoftObjectPath().GetLongPackageName().Equals(
				CurrentPackageName,
				ESearchCase::CaseSensitive);
		});
	if (CurrentRoomSequenceIndex == INDEX_NONE)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Direct-PIE room activation rejected: World=%s is not in MajorStage=%d."),
			*CurrentPackageName,
			FirstMajorStageIndex);
		return false;
	}

	CurrentMajorStageIndex = FirstMajorStageIndex;
	CurrentRoomIndex = CurrentRoomSequenceIndex;
	bOrderedLevelListMode = bUseOrderedLevelList;
	ActiveRoomSequence = MoveTemp(FirstMajorStageSequence);
	if (!TransitionRunState(ERoguelikeRunState::LoadingRoom, ERoguelikeRunTransitionReason::StartRun))
	{
		return false;
	}

	const bool bActivated = TransitionRunState(
		ERoguelikeRunState::InRoom,
		ERoguelikeRunTransitionReason::RoomLoaded);
	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Direct-PIE map activation %s. World=%s MajorStage=%d Room=%d."),
		bActivated ? TEXT("succeeded") : TEXT("failed"),
		*CurrentPackageName,
		CurrentMajorStageIndex,
		CurrentRoomIndex);
	return bActivated;
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

bool URoguelikeRunFlowSubsystem::IsCurrentRoomFinalLevel() const
{
	if (!ActiveRoomSequence.IsValidIndex(CurrentRoomIndex))
	{
		return false;
	}

	if (bOrderedLevelListMode)
	{
		return CurrentRoomIndex >= ActiveRoomSequence.Num() - 1;
	}

	// 白盒大关模式：最后一个大关的最后一个房间。
	return CurrentMajorStageIndex >= LastMajorStageIndex && !HasNextRoom();
}

bool URoguelikeRunFlowSubsystem::CompleteRunFromFinalRoomClear()
{
	if (CurrentRunState == ERoguelikeRunState::Result)
	{
		// 已经通关过（例如清场结算与出口推进都触发了），保持幂等。
		return true;
	}

	if (CurrentRunState != ERoguelikeRunState::InRoom)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Cannot complete the run in state %d; InRoom is required."),
			static_cast<int32>(CurrentRunState));
		return false;
	}

	const FRoguelikeRoomDefinition FinalRoom = GetCurrentRoomDefinition();
	const FString FinalPackageName = FinalRoom.RoomMap.ToSoftObjectPath().GetLongPackageName();
	const int32 ClearedLevelCount = ActiveRoomSequence.Num();

	// 结算入口统一走 FinalizeCurrentRun：它会抓取结算快照（Result 界面读的就是它）、
	// 冻结结算数据、写入 RunOutcome 并切到 Result 状态（GameMode 随后弹出结算界面）。
	const bool bFinalized = FinalizeCurrentRun(
		ERoguelikeRunOutcome::Victory,
		ERoguelikeRunTransitionReason::RunCompleted);
	if (!bFinalized)
	{
		return false;
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Run cleared! Mode=%s Levels=%d FinalLevelId=%s FinalLevel=%s."),
		bOrderedLevelListMode ? TEXT("OrderedLevelList") : TEXT("MajorStagePool"),
		ClearedLevelCount,
		*FinalRoom.RoomId.ToString(),
		*FinalPackageName);

	FEventBus::Publish<FGameClearedEvent>(FGameClearedEvent(ClearedLevelCount, FinalPackageName));

	return true;
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
		// Whitebox topology: combat -> final combat -> shop. Until dedicated boss
		// maps exist, the stage combat map is intentionally reused for the final
		// placeholder room under a different RoomId.
		Definition.RoomSequenceLength = 3;

		FRoguelikeRoomDefinition FirstCombatRoom;
		FirstCombatRoom.RoomId = FName(*FString::Printf(TEXT("M%02d_Combat_A"), MajorStageIndex + 1));
		FirstCombatRoom.RoomType = ERoguelikeRoomType::Combat;
		FirstCombatRoom.SelectionWeight = 1;
		FirstCombatRoom.RoomMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(MajorStageRoomPaths[MajorStageIndex]));
		Definition.RoomPool.Add(MoveTemp(FirstCombatRoom));

		FRoguelikeRoomDefinition FinalCombatRoom;
		FinalCombatRoom.RoomId = FName(*FString::Printf(TEXT("M%02d_Combat_B"), MajorStageIndex + 1));
		FinalCombatRoom.RoomType = ERoguelikeRoomType::Combat;
		FinalCombatRoom.EncounterTier = ERoguelikeEncounterTier::Boss;
		FinalCombatRoom.SelectionWeight = 1;
		FinalCombatRoom.RoomMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(MajorStageRoomPaths[MajorStageIndex]));
		Definition.RoomPool.Add(MoveTemp(FinalCombatRoom));

		FRoguelikeRoomDefinition ShopRoom;
		ShopRoom.RoomId = FName(*FString::Printf(TEXT("M%02d_Shop_A"), MajorStageIndex + 1));
		ShopRoom.RoomType = ERoguelikeRoomType::Shop;
		ShopRoom.SelectionWeight = 1;
		ShopRoom.RoomMap = TSoftObjectPtr<UWorld>(
			FSoftObjectPath(TEXT("/Game/Level/TestMap_Shop.TestMap_Shop")));
		Definition.RoomPool.Add(MoveTemp(ShopRoom));

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
	bOrderedLevelListMode = false;
	RunOutcome = ERoguelikeRunOutcome::None;
	ResultSnapshot = FRoguelikeRunResultSnapshot();
}

void URoguelikeRunFlowSubsystem::ResolveLevelDataAsset()
{
	if (IsValid(RunLevelData))
	{
		return;
	}

	const FSoftObjectPath LevelDataPath(RunLevelDataAssetPath);
	RunLevelData = Cast<ULevelDataAsset>(LevelDataPath.TryLoad());

	if (!RunLevelData)
	{
		UE_LOG(LogRoguelikeRunFlow, Log,
			TEXT("No level data asset at %s; using the whitebox major-stage / room-pool configuration."),
			RunLevelDataAssetPath);
		return;
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Level data asset loaded: %s Levels=%d."),
		*LevelDataPath.ToString(),
		RunLevelData->Levels.Num());

	if (RunLevelData->Levels.Num() == 0)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Level data asset %s has an empty Levels array; using the whitebox configuration."),
			RunLevelDataAssetPath);
	}
}

bool URoguelikeRunFlowSubsystem::BuildRoomSequenceFromLevelData(TArray<FRoguelikeRoomDefinition>& OutSequence)
{
	OutSequence.Reset();

	if (!IsValid(RunLevelData))
	{
		return false;
	}

	for (int32 EntryIndex = 0; EntryIndex < RunLevelData->Levels.Num(); ++EntryIndex)
	{
		const FRunLevelEntry& Entry = RunLevelData->Levels[EntryIndex];
		if (Entry.LevelMap.IsNull())
		{
			UE_LOG(LogRoguelikeRunFlow, Warning,
				TEXT("Levels[%d] has no LevelMap assigned; entry skipped."),
				EntryIndex);
			continue;
		}

		FRoguelikeRoomDefinition Room;
		Room.RoomId = Entry.LevelId.IsNone()
			? FName(*FString::Printf(TEXT("Level_%02d"), OutSequence.Num() + 1))
			: Entry.LevelId;
		Room.RoomType = Entry.RoomType;
		Room.EncounterTier = Entry.EncounterTier;
		// 顺序列表模式下不使用抽签权重，固定填 1 以免误导。
		Room.SelectionWeight = 1;
		Room.RoomMap = Entry.LevelMap;
		OutSequence.Add(MoveTemp(Room));
	}

	if (OutSequence.Num() == 0)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Level data asset provided no usable level (all entries missing a LevelMap); using the whitebox configuration."));
		return false;
	}

	for (int32 LevelIndex = 0; LevelIndex < OutSequence.Num(); ++LevelIndex)
	{
		const FRoguelikeRoomDefinition& Room = OutSequence[LevelIndex];
		UE_LOG(LogRoguelikeRunFlow, Log,
			TEXT("Ordered level list entry. Index=%d LevelId=%s RoomType=%d EncounterTier=%d Map=%s."),
			LevelIndex,
			*Room.RoomId.ToString(),
			static_cast<int32>(Room.RoomType),
			static_cast<int32>(Room.EncounterTier),
			*Room.RoomMap.ToSoftObjectPath().ToString());
	}

	return true;
}

bool URoguelikeRunFlowSubsystem::BeginNewRun(ERoguelikeRunTransitionReason Reason)
{
	// Build and validate the destination before committing any reset. This is
	// especially important when RestartRun is called from Result: a rejected
	// configuration leaves its snapshot and buttons available for recovery.
	// 目标序列优先用关卡数据资产的顺序关卡列表，没有可用数据时退回白盒大关 / 房间池。
	ResolveLevelDataAsset();

	TArray<FRoguelikeRoomDefinition> FirstSequence;
	const bool bUseOrderedLevelList = BuildRoomSequenceFromLevelData(FirstSequence);
	if (!bUseOrderedLevelList)
	{
		if (!BuildRoomSequence(FirstMajorStageIndex, FirstSequence))
		{
			return false;
		}
	}

	if (!FirstSequence.IsValidIndex(0))
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("New-run request rejected: no usable room sequence could be built."));
		return false;
	}

	const FRoguelikeRoomDefinition& FirstRoom = FirstSequence[0];
	if (!HasConfiguredTravelMap(FirstRoom.RoomMap))
	{
		UE_LOG(LogRoguelikeRunFlow, Error,
			TEXT("New-run request rejected: the first room map is not configured."));
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

	// 新一局开始：结算数据从 0 重新累计。
	// 注意这里（以及"回到准备房间"那条路径刻意不清）—— Result/主菜单阶段仍可读取本局数据，
	// 直到下一局真正开始才会被清空。
	USettlementDataRecorder::Reset();

	ResetRunProgress();

	// 注意：ResetRunProgress 会把模式标记清掉，所以序列模式在重置之后再写入。
	bOrderedLevelListMode = bUseOrderedLevelList;
	CurrentMajorStageIndex = FirstMajorStageIndex;
	CurrentRoomIndex = 0;
	ActiveRoomSequence = MoveTemp(FirstSequence);

	const FRoguelikeRoomDefinition& ActiveFirstRoom = ActiveRoomSequence[CurrentRoomIndex];
	if (!RequestMapTravel(ActiveFirstRoom.RoomMap, Reason, TEXT("new run first room"), false))
	{
		ResetRunProgress();
		return false;
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("New run started. Mode=%s Levels=%d MajorStage=%d RoomIndex=%d RoomId=%s EncounterTier=%d."),
		bOrderedLevelListMode ? TEXT("OrderedLevelList") : TEXT("MajorStagePool"),
		ActiveRoomSequence.Num(),
		CurrentMajorStageIndex,
		CurrentRoomIndex,
		*ActiveFirstRoom.RoomId.ToString(),
		static_cast<int32>(ActiveFirstRoom.EncounterTier));
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
	FRoguelikeRoomDefinition ReservedShopRoom;
	bool bHasReservedShopRoom = false;
	for (int32 CandidateIndex = Candidates.Num() - 1; CandidateIndex >= 0; --CandidateIndex)
	{
		if (Candidates[CandidateIndex].RoomType != ERoguelikeRoomType::Shop)
		{
			continue;
		}

		if (!bHasReservedShopRoom)
		{
			ReservedShopRoom = Candidates[CandidateIndex];
			bHasReservedShopRoom = true;
		}
		else
		{
			UE_LOG(LogRoguelikeRunFlow, Warning,
				TEXT("MajorStage=%d has multiple Shop rooms; only RoomId=%s is reserved for the fixed final Shop slot."),
				MajorStageIndex, *ReservedShopRoom.RoomId.ToString());
		}

		Candidates.RemoveAtSwap(CandidateIndex);
	}

	const int32 MaxSequenceLength = Candidates.Num() + (bHasReservedShopRoom ? 1 : 0);
	const int32 SequenceLength = FMath::Min(RequestedLength, MaxSequenceLength);
	if (RequestedLength > MaxSequenceLength)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("RoomSequenceLength=%d exceeds usable room count=%d for MajorStage=%d; drawing without replacement."),
			RequestedLength, MaxSequenceLength, MajorStageIndex);
	}

	const int32 DrawCount = SequenceLength - (bHasReservedShopRoom ? 1 : 0);
	while (OutSequence.Num() < DrawCount)
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

	if (bHasReservedShopRoom)
	{
		const int32 ShopInsertIndex = FMath::Max(0, SequenceLength - 1);
		OutSequence.Insert(ReservedShopRoom, ShopInsertIndex);
	}

	for (int32 SequenceIndex = 0; SequenceIndex < OutSequence.Num(); ++SequenceIndex)
	{
		const FRoguelikeRoomDefinition& Room = OutSequence[SequenceIndex];
		UE_LOG(LogRoguelikeRunFlow, Log,
			TEXT("Room sequence entry. MajorStage=%d Index=%d RoomId=%s RoomType=%d EncounterTier=%d."),
			MajorStageIndex,
			SequenceIndex,
			*Room.RoomId.ToString(),
			static_cast<int32>(Room.RoomType),
			static_cast<int32>(Room.EncounterTier));
	}

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Built room sequence. MajorStage=%d RoomCount=%d ShopAtLast=%s."),
		MajorStageIndex,
		OutSequence.Num(),
		bHasReservedShopRoom ? TEXT("true") : TEXT("false"));
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

	const bool bConsumeCombatRoomDuration = bCapturePlayerData
		&& CurrentRunState == ERoguelikeRunState::InRoom
		&& GetCurrentRoomDefinition().RoomType == ERoguelikeRoomType::Combat;

	if (bCapturePlayerData && !CaptureCurrentPlayerRuntimeData())
	{
		return false;
	}

	if (!TransitionRunState(ERoguelikeRunState::LoadingRoom, Reason))
	{
		return false;
	}

	if (bConsumeCombatRoomDuration)
	{
		if (URoguelikeRuntimeDataSubsystem* RuntimeData =
			GameInstance->GetSubsystem<URoguelikeRuntimeDataSubsystem>())
		{
			RuntimeData->ConsumeCombatRoomDurations();
		}
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

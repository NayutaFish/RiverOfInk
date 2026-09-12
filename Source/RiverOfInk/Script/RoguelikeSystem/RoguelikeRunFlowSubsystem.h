// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RoguelikeSystem/RoguelikeRunTypes.h"
#include "RoguelikeRunFlowSubsystem.generated.h"

class ARoguelikeExitTrigger;
class URoguelikeEconomySubsystem;
class ULevelDataAsset;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogRoguelikeRunFlow, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnRoguelikeRunStateChanged,
	ERoguelikeRunState,
	PreviousState,
	ERoguelikeRunState,
	NewState,
	ERoguelikeRunTransitionReason,
	Reason
);

/**
 * Owns the persistent flow of one roguelike run.
 *
 * This subsystem holds the active major-stage / room sequence, run state,
 * reset boundary, and map travel. It deliberately does not create room UI,
 * spawn enemies, or own player data; those remain map-local systems or the
 * runtime-data subsystem.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API URoguelikeRunFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 FirstMajorStageIndex = 0;
	static constexpr int32 LastMajorStageIndex = 2;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Configure the preparation-room map shown before a run starts. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow|Configuration")
	void SetPreparationRoomMap(TSoftObjectPtr<UWorld> InPreparationRoomMap);

	/** Configure one fixed-order major stage and its weighted room pool. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow|Configuration")
	void SetMajorStageDefinition(int32 MajorStageIndex, const FMajorStageDefinition& InDefinition);

	/** Return to the preparation room and clear all current-run progress and player data. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow")
	bool LoadPreparationRoom();

	/** Ensure the preparation room has an immediately usable start exit. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow")
	bool EnsurePreparationStartExit();

	/** Clear the previous run, draw MajorStage 0, and load its first room. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow")
	bool StartNewRun();

	/** Restart from the result state using the same reset path as a fresh run. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow")
	bool RestartRun();

	/** Advance from an active exit: preparation starts a run; a room advances the run sequence. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow")
	bool RequestAdvanceFromExit();

	/** Load the next selected room in the current major stage. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow")
	bool AdvanceToNextRoom();

	/** Draw and load the first room of the next major stage, or finish the run. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow")
	bool AdvanceToNextMajorStage();

	/** Called by a map GameMode after its world has begun play. */
	bool NotifyRoomLoaded(UWorld* LoadedWorld);

	/** Return whether a world matches the configured preparation-room map. */
	bool IsPreparationRoomMap(const UWorld* World) const;

	/** 当前房间是不是本局的最后一个关卡（顺序关卡列表的终点 / 最后一大关的最后一件）。 */
	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	bool IsCurrentRoomFinalLevel() const;

	/**
	 * 通关：置 Victory、广播 FGameClearedEvent、切到 Result（幂等）。
	 * 顺序关卡模式由"终点关卡清场"调用；终点是商店（没有清场）时由它的出口调用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Run Flow")
	bool CompleteRunFromFinalRoomClear();

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	ERoguelikeRunState GetRunState() const { return CurrentRunState; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	ERoguelikeRunTransitionReason GetLastTransitionReason() const { return LastTransitionReason; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	ERoguelikeRunOutcome GetRunOutcome() const { return RunOutcome; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	int32 GetCurrentMajorStageIndex() const { return CurrentMajorStageIndex; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	int32 GetCurrentRoomIndex() const { return CurrentRoomIndex; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	int32 GetActiveRoomCount() const { return ActiveRoomSequence.Num(); }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	bool HasNextRoom() const;

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	bool HasNextMajorStage() const;

	UFUNCTION(BlueprintPure, Category = "Roguelike|Run Flow")
	FRoguelikeRoomDefinition GetCurrentRoomDefinition() const;

	/** Immutable setup data. A Blueprint subclass may override these values. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Run Flow|Configuration")
	TSoftObjectPtr<UWorld> PreparationRoomMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Run Flow|Configuration")
	TMap<int32, FMajorStageDefinition> MajorStageDefinitions;

	/** Zero means a new seed is chosen whenever the subsystem initializes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Run Flow|Configuration")
	int32 RandomSeed = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Run Flow|Configuration")
	TSubclassOf<ARoguelikeExitTrigger> PreparationExitTriggerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Run Flow|Configuration")
	FVector PreparationExitSpawnOffset = FVector(500.0f, 0.0f, 120.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Run Flow|Runtime")
	ERoguelikeRunState CurrentRunState = ERoguelikeRunState::MainMenu;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Run Flow|Runtime")
	ERoguelikeRunTransitionReason LastTransitionReason = ERoguelikeRunTransitionReason::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Run Flow|Runtime")
	ERoguelikeRunOutcome RunOutcome = ERoguelikeRunOutcome::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Run Flow|Runtime")
	int32 CurrentMajorStageIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Run Flow|Runtime")
	int32 CurrentRoomIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Run Flow|Runtime")
	TArray<FRoguelikeRoomDefinition> ActiveRoomSequence;

	/** 本局是否在使用关卡数据资产的顺序关卡列表（否则为白盒大关 / 房间池模式）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Run Flow|Runtime")
	bool bOrderedLevelListMode = false;

	/** Broadcast after state storage is updated. Subscribers observe; they do not own transitions. */
	UPROPERTY(BlueprintAssignable, Category = "Roguelike|Run Flow|Events")
	FOnRoguelikeRunStateChanged OnRunStateChanged;

private:
	void ConfigureDefaultWhiteboxRoomsIfUnset();

	/** 按固定资产路径解析关卡数据资产（幂等，只会在第一次或失败后重试）。 */
	void ResolveLevelDataAsset();

	/** 用关卡数据资产的顺序列表填 OutSequence；没有可用数据时返回 false。 */
	bool BuildRoomSequenceFromLevelData(TArray<FRoguelikeRoomDefinition>& OutSequence);

	void ResetRunProgress();
	bool BeginNewRun(ERoguelikeRunTransitionReason Reason);
	bool BuildRoomSequence(int32 MajorStageIndex, TArray<FRoguelikeRoomDefinition>& OutSequence);
	bool RequestMapTravel(
		const TSoftObjectPtr<UWorld>& MapAsset,
		ERoguelikeRunTransitionReason Reason,
		const TCHAR* TransitionDescription,
		bool bCapturePlayerData
	);
	bool TransitionRunState(ERoguelikeRunState NextState, ERoguelikeRunTransitionReason Reason);
	bool IsTransitionAllowed(ERoguelikeRunState NextState) const;
	bool ResetPlayerRuntimeData();
	bool ResetEconomyData();
	bool CaptureCurrentPlayerRuntimeData();

	FRandomStream RoomRandomStream;

	/** 关卡数据资产（顺序关卡列表）；为空表示走白盒兜底配置。 */
	UPROPERTY(Transient)
	TObjectPtr<ULevelDataAsset> RunLevelData;
};

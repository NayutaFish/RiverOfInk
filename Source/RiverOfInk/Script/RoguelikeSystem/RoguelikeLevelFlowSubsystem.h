// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RoguelikeLevelFlowSubsystem.generated.h"

class UWorld;
class ARoguelikeExitTrigger;

/**
 * One major level's pool of minor level maps.
 *
 * The pool is configuration only. The subsystem copies a random, non-repeating
 * sequence from this pool when the major level starts.
 */
USTRUCT(BlueprintType)
struct FRoguelikeMinorLevelPool
{
	GENERATED_BODY()

	/** Number of minor levels to draw for this major level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Flow")
	int32 SequenceLength = 1;

	/** Candidate maps. A map is selected at most once in the generated sequence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Flow")
	TArray<TSoftObjectPtr<UWorld>> LevelPool;
};

DECLARE_LOG_CATEGORY_EXTERN(LogRoguelikeLevelFlow, Log, All);

/**
 * Owns the run's level order and level travel only.
 *
 * This subsystem intentionally does not own player snapshots, rewards, run
 * outcome, or restart decisions. It survives OpenLevel because its lifetime is
 * tied to UGameInstance rather than the current UWorld.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API URoguelikeLevelFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Preparation scene sentinel. The three playable major levels are 0, 1, 2. */
	static constexpr int32 PreparationLevelId = -1;
	static constexpr int32 FirstMajorLevelId = 0;
	static constexpr int32 LastMajorLevelId = 2;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Configure the preparation map before the first transition. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Level Flow|Configuration")
	void SetPreparationLevel(TSoftObjectPtr<UWorld> InPreparationLevel);

	/** Configure one fixed-order major level's random minor-level pool. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Level Flow|Configuration")
	void SetMajorLevelPool(int32 MajorLevelId, const FRoguelikeMinorLevelPool& InPool);

	/** Load the preparation map and clear the active minor-level sequence. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Level Flow")
	bool LoadPreparationLevel();

	/**
	 * Ensure the preparation scene has an immediately usable exit. This is a
	 * scene bootstrap operation; it does not decide whether a run is restarted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Level Flow")
	bool EnsurePreparationExit();

	/** Move to the next fixed-order major level and load its first minor level. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Level Flow")
	bool AdvanceToNextMajorLevel();

	/** Load the next minor level in the current major level's generated sequence. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Level Flow")
	bool AdvanceToNextMinorLevel();

	/**
	 * Generic exit operation: continue the current minor sequence, or advance to
	 * the next major level when that sequence is exhausted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Level Flow")
	bool AdvanceToNextLevel();

	UFUNCTION(BlueprintPure, Category = "Roguelike|Level Flow")
	int32 GetCurrentMajorLevelId() const { return CurrentMajorLevelId; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Level Flow")
	int32 GetCurrentMinorLevelIndex() const { return CurrentMinorLevelIndex; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Level Flow")
	int32 GetActiveMinorLevelCount() const { return ActiveMinorLevelSequence.Num(); }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Level Flow")
	bool HasNextMinorLevel() const;

	UFUNCTION(BlueprintPure, Category = "Roguelike|Level Flow")
	bool HasNextMajorLevel() const;

	/** Level-pool configuration. These values can be set by a Blueprint subclass or by SetMajorLevelPool. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Level Flow|Configuration")
	TSoftObjectPtr<UWorld> PreparationLevel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Level Flow|Configuration")
	TMap<int32, FRoguelikeMinorLevelPool> MajorLevelPools;

	/** Zero means a new seed is chosen when the subsystem initializes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Level Flow|Configuration")
	int32 RandomSeed = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Level Flow|Configuration")
	TSubclassOf<ARoguelikeExitTrigger> ExitTriggerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roguelike|Level Flow|Configuration")
	FVector PreparationExitSpawnOffset = FVector(500.0f, 0.0f, 120.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Level Flow|Runtime")
	int32 CurrentMajorLevelId = PreparationLevelId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Level Flow|Runtime")
	int32 CurrentMinorLevelIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Level Flow|Runtime")
	TArray<TSoftObjectPtr<UWorld>> ActiveMinorLevelSequence;

private:
	void ConfigureDefaultWhiteboxMapsIfUnset();
	bool BuildMinorLevelSequence(int32 MajorLevelId);
	bool RequestLevelTravel(const TSoftObjectPtr<UWorld>& LevelAsset, const TCHAR* TransitionReason);

	FRandomStream LevelRandomStream;
};

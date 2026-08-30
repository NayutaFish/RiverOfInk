// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RiverOfInkPlayerController.generated.h"

UCLASS()
class RIVEROFINK_API ARiverOfInkPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARiverOfInkPlayerController();

	virtual void Tick(float DeltaSeconds) override;

	/** Development-only PIE helper used to drive a room clear when input automation is unavailable. */
	UFUNCTION(exec, Category = "Debug")
	void DebugKillAllEnemies();

	/** Development-only PIE helper used to open the generic reward HUD directly. */
	UFUNCTION(exec, Category = "Debug")
	void DebugShowRewardSelection();

	/** Development-only PIE helper used to select the first currently shown reward. */
	UFUNCTION(exec, Category = "Debug")
	void DebugSelectFirstReward();

	/**
	 * Development-only PIE helper: show one legal reward by title or modifier identifier.
	 * StackCount applies that many modifier stacks in one debug reward; form rewards require 1.
	 */
	UFUNCTION(exec, Category = "Debug")
	void DebugShowSpecificReward(const FString& RewardIdentifier, int32 StackCount = 1);

	/**
	 * Development-only PIE helper: show and immediately select one legal reward.
	 * StackCount applies that many modifier stacks in one debug reward; form rewards require 1.
	 */
	UFUNCTION(exec, Category = "Debug")
	void DebugSelectSpecificReward(const FString& RewardIdentifier, int32 StackCount = 1);

	/** Development-only PIE helper: apply the current homing build to the nearest living enemy. */
	UFUNCTION(exec, Category = "Debug")
	void DebugApplyHomingMark(float Duration = 4.0f);

	/** Development-only PIE helper: place the nearest living enemy inside the E arc. */
	UFUNCTION(exec, Category = "Debug")
	void DebugPrepareTwoStageArc(float ForwardDistance = 140.0f);

	/** Set this in a PIE instance to invoke the helper on the next controller tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugKillAllEnemiesOnNextTick = false;

	/** Optional PIE-only follow-up: choose the first generated reward after the kill helper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugSelectFirstRewardAfterKill = false;
};

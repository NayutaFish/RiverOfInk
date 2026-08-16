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

	/** Set this in a PIE instance to invoke the helper on the next controller tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugKillAllEnemiesOnNextTick = false;

	/** Optional PIE-only follow-up: choose the first generated reward after the kill helper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugSelectFirstRewardAfterKill = false;
};

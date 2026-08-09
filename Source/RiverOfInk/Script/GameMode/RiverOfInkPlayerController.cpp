// Copyright Our Copyright notice in the Description page of Project Settings.

#include "GameMode/RiverOfInkPlayerController.h"

#include "Enemy/EnemyBase/EnemyBase.h"
#include "EngineUtils.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "RiverOfInk.h"

ARiverOfInkPlayerController::ARiverOfInkPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	// 鼠标全程显示，进入游戏后不自动隐藏
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ARiverOfInkPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDebugKillAllEnemiesOnNextTick)
	{
		bDebugKillAllEnemiesOnNextTick = false;
		DebugKillAllEnemies();
	}
}

void ARiverOfInkPlayerController::DebugKillAllEnemies()
{
	if (!GetWorld())
	{
		return;
	}

	int32 KilledCount = 0;
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		if (IsValid(Enemy) && !Enemy->bIsDead)
		{
			Enemy->TestDie();
			++KilledCount;
		}
	}

	UE_LOG(LogRoguelike, Log, TEXT("DebugKillAllEnemies executed. Killed=%d."), KilledCount);

	if (bDebugSelectFirstRewardAfterKill)
	{
		bDebugSelectFirstRewardAfterKill = false;
		for (TActorIterator<ARoguelikeRewardManager> It(GetWorld()); It; ++It)
		{
			if (ARoguelikeRewardManager* RewardManager = *It)
			{
				RewardManager->SelectReward(0);
				UE_LOG(LogRoguelike, Log, TEXT("Debug selected reward option 0 after room clear."));
				break;
			}
		}
	}
}

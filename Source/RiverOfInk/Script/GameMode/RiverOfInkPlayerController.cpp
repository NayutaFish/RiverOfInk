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

void ARiverOfInkPlayerController::DebugShowRewardSelection()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ARoguelikeRewardManager> It(GetWorld()); It; ++It)
	{
		if (ARoguelikeRewardManager* RewardManager = *It)
		{
			RewardManager->ShowRewardAfterRoomClear();
			UE_LOG(LogRoguelike, Log, TEXT("DebugShowRewardSelection requested the reward HUD."));
			return;
		}
	}

	UE_LOG(LogRoguelike, Warning, TEXT("DebugShowRewardSelection found no RoguelikeRewardManager."));
}

void ARiverOfInkPlayerController::DebugSelectFirstReward()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ARoguelikeRewardManager> It(GetWorld()); It; ++It)
	{
		if (ARoguelikeRewardManager* RewardManager = *It)
		{
			RewardManager->SelectReward(0);
			UE_LOG(LogRoguelike, Log, TEXT("DebugSelectFirstReward requested reward option 0."));
			return;
		}
	}

	UE_LOG(LogRoguelike, Warning, TEXT("DebugSelectFirstReward found no RoguelikeRewardManager."));
}

void ARiverOfInkPlayerController::DebugShowSpecificReward(const FString& RewardIdentifier)
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ARoguelikeRewardManager> It(GetWorld()); It; ++It)
	{
		if (ARoguelikeRewardManager* RewardManager = *It)
		{
			RewardManager->DebugShowSpecificReward(RewardIdentifier);
			UE_LOG(LogRoguelike, Log,
				TEXT("DebugShowSpecificReward requested identifier '%s'."),
				*RewardIdentifier);
			return;
		}
	}

	UE_LOG(LogRoguelike, Warning, TEXT("DebugShowSpecificReward found no RoguelikeRewardManager."));
}

void ARiverOfInkPlayerController::DebugSelectSpecificReward(const FString& RewardIdentifier)
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ARoguelikeRewardManager> It(GetWorld()); It; ++It)
	{
		if (ARoguelikeRewardManager* RewardManager = *It)
		{
			if (RewardManager->DebugSelectSpecificReward(RewardIdentifier))
			{
				UE_LOG(LogRoguelike, Log,
					TEXT("DebugSelectSpecificReward selected identifier '%s'."),
					*RewardIdentifier);
			}
			return;
		}
	}

	UE_LOG(LogRoguelike, Warning, TEXT("DebugSelectSpecificReward found no RoguelikeRewardManager."));
}

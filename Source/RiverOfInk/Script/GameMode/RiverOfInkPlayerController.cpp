// Copyright Our Copyright notice in the Description page of Project Settings.

#include "GameMode/RiverOfInkPlayerController.h"

#include "Enemy/EnemyBase/EnemyBase.h"
#include "EngineUtils.h"
#include "Player/PlayerCharacter.h"
#include "Player/ProjectileTargetingComponent.h"
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

void ARiverOfInkPlayerController::DebugApplyHomingMark(float Duration)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerCharacter || !GetWorld())
	{
		UE_LOG(LogRoguelike, Warning, TEXT("DebugApplyHomingMark requires a possessed PlayerCharacter."));
		return;
	}

	UProjectileTargetingComponent* Targeting = PlayerCharacter->GetProjectileTargetingComponent();
	if (!Targeting || !Targeting->HasHomingBuild())
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugApplyHomingMark requires the ProjectileHoming build first."));
		return;
	}

	AEnemyBase* NearestEnemy = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		if (!IsValid(Enemy) || Enemy->bIsDead)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			PlayerCharacter->GetActorLocation(),
			Enemy->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestEnemy = Enemy;
		}
	}

	if (!NearestEnemy)
	{
		UE_LOG(LogRoguelike, Warning, TEXT("DebugApplyHomingMark found no living enemy."));
		return;
	}

	const float AppliedDuration = FMath::Max(0.01f, Duration);
	const FCombatEffectHandle Handle = Targeting->ApplyOrTransferHomingMark(
		NearestEnemy,
		AppliedDuration);
	UE_LOG(LogRoguelike, Log,
		TEXT("DebugApplyHomingMark applied: Target=%s Duration=%.3f Handle=%d Distance=%.1f."),
		*GetNameSafe(NearestEnemy),
		AppliedDuration,
		Handle.Id,
		FMath::Sqrt(NearestDistanceSquared));
}

void ARiverOfInkPlayerController::DebugPrepareTwoStageArc(float ForwardDistance)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerCharacter || !GetWorld())
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugPrepareTwoStageArc requires a possessed PlayerCharacter."));
		return;
	}

	AEnemyBase* NearestEnemy = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		AEnemyBase* Enemy = *It;
		if (!IsValid(Enemy) || Enemy->bIsDead)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(
			PlayerCharacter->GetActorLocation(),
			Enemy->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestEnemy = Enemy;
		}
	}

	if (!NearestEnemy)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugPrepareTwoStageArc found no living enemy."));
		return;
	}

	FVector Forward = PlayerCharacter->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	const float Distance = FMath::Clamp(ForwardDistance, 40.0f, 190.0f);
	FVector TargetLocation = PlayerCharacter->GetActorLocation() + Forward * Distance;
	TargetLocation.Z = NearestEnemy->GetActorLocation().Z;
	NearestEnemy->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogRoguelike, Display,
		TEXT("DebugPrepareTwoStageArc moved %s to player front: Distance=%.1f."),
		*GetNameSafe(NearestEnemy),
		Distance);
}

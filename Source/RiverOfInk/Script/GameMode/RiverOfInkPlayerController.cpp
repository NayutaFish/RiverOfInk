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

void ARiverOfInkPlayerController::DebugApplySpecificRewards(const FString& RewardIdentifiers)
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ARoguelikeRewardManager> It(GetWorld()); It; ++It)
	{
		if (ARoguelikeRewardManager* RewardManager = *It)
		{
			if (RewardManager->DebugApplySpecificRewards(RewardIdentifiers))
			{
				UE_LOG(LogRoguelike, Log,
					TEXT("DebugApplySpecificRewards applied identifiers '%s'."),
					*RewardIdentifiers);
			}
			return;
		}
	}

	UE_LOG(LogRoguelike, Warning, TEXT("DebugApplySpecificRewards found no RoguelikeRewardManager."));
}

void ARiverOfInkPlayerController::DebugMarkNearestHomingTarget(float Duration)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetPawn());
	UProjectileTargetingComponent* Targeting = PlayerCharacter
		? PlayerCharacter->GetProjectileTargetingComponent()
		: nullptr;
	if (!GetWorld() || !PlayerCharacter || !Targeting || Duration <= 0.0f)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugMarkNearestHomingTarget rejected: Player, targeting component, world, or duration is invalid."));
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
		UE_LOG(LogRoguelike, Warning, TEXT("DebugMarkNearestHomingTarget found no live enemy."));
		return;
	}

	const FCombatEffectHandle MarkHandle = Targeting->ApplyOrTransferHomingMark(NearestEnemy, Duration);
	UE_LOG(LogRoguelike, Log,
		TEXT("DebugMarkNearestHomingTarget applied: Target=%s Duration=%.2f Handle=%d."),
		*GetNameSafe(NearestEnemy),
		Duration,
		MarkHandle.Id);
}

void ARiverOfInkPlayerController::DebugMoveNearNearestHomingTarget(float Distance)
{
	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetPawn());
	if (!GetWorld() || !PlayerCharacter || Distance <= 0.0f)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("DebugMoveNearNearestHomingTarget rejected: Player, world, or distance is invalid."));
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
		UE_LOG(LogRoguelike, Warning, TEXT("DebugMoveNearNearestHomingTarget found no live enemy."));
		return;
	}

	const FVector PlayerLocation = PlayerCharacter->GetActorLocation();
	const FVector TargetLocation = NearestEnemy->GetActorLocation();
	FVector FromTarget = PlayerLocation - TargetLocation;
	FromTarget.Z = 0.0f;
	if (!FromTarget.Normalize())
	{
		FromTarget = -PlayerCharacter->GetActorForwardVector();
		FromTarget.Z = 0.0f;
		FromTarget.Normalize();
	}

	FVector NewLocation = TargetLocation + FromTarget * FMath::Max(50.0f, Distance);
	NewLocation.Z = TargetLocation.Z;
	const bool bMoved = PlayerCharacter->SetActorLocation(
		NewLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	UE_LOG(LogRoguelike, Log,
		TEXT("DebugMoveNearNearestHomingTarget %s: Target=%s BeforeDistance=%.1f AfterDistance=%.1f Player=%s TargetLocation=%s."),
		bMoved ? TEXT("succeeded") : TEXT("failed"),
		*GetNameSafe(NearestEnemy),
		FMath::Sqrt(NearestDistanceSquared),
		FVector::Dist2D(NewLocation, TargetLocation),
		*NewLocation.ToString(),
		*TargetLocation.ToString());
}

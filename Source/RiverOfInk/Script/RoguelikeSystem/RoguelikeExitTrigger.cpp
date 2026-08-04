// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeExitTrigger.h"

#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Player/PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RoguelikeSystem/RoguelikeLevelFlowSubsystem.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"

ARoguelikeExitTrigger::ARoguelikeExitTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(220.0f, 220.0f, 120.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	// PlayerCharacter uses the project's long-term PlayerHitbox channel.
	// Keep this explicit instead of relying on the built-in Pawn channel.
	TriggerBox->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->ShapeColor = FColor(40, 220, 90, 180);
}

void ARoguelikeExitTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerBox)
	{
		// Re-assert the project collision contract at runtime. The placed actor or
		// a derived Blueprint can serialize an older response table over constructor defaults.
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		TriggerBox->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
		TriggerBox->SetGenerateOverlapEvents(true);
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ARoguelikeExitTrigger::HandleBeginOverlap);
	}

	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeLevelFlowSubsystem* LevelFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeLevelFlowSubsystem>()
		: nullptr;
	const bool bIsPreparationLevel = LevelFlow
		&& LevelFlow->GetCurrentMajorLevelId() == URoguelikeLevelFlowSubsystem::PreparationLevelId;

	if (bIsPreparationLevel)
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Preparation exit does not require a reward manager; Major=%d."),
			LevelFlow->GetCurrentMajorLevelId());
	}
	else if (ResolveRewardManager())
	{
		RewardManager->OnRewardApplied.AddDynamic(this, &ARoguelikeExitTrigger::HandleRewardApplied);
	}
}

void ARoguelikeExitTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &ARoguelikeExitTrigger::HandleBeginOverlap);
	}

	if (IsValid(RewardManager))
	{
		RewardManager->OnRewardApplied.RemoveDynamic(this, &ARoguelikeExitTrigger::HandleRewardApplied);
	}

	Super::EndPlay(EndPlayReason);
}

bool ARoguelikeExitTrigger::ResolveRewardManager()
{
	if (IsValid(RewardManager))
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Exit trigger cannot resolve RewardManager: World is unavailable."));
		return false;
	}

	TArray<AActor*> RewardManagers;
	UGameplayStatics::GetAllActorsOfClass(World, ARoguelikeRewardManager::StaticClass(), RewardManagers);
	if (RewardManagers.Num() <= 0)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Exit trigger has no RoguelikeRewardManager in the current level."));
		return false;
	}

	RewardManager = Cast<ARoguelikeRewardManager>(RewardManagers[0]);
	return IsValid(RewardManager);
}

void ARoguelikeExitTrigger::HandleRewardApplied(const FRoguelikeRewardOption& Reward)
{
	UE_LOG(LogRoguelike, Log, TEXT("Exit reward event received: %s."), *Reward.Title.ToString());
	ActivateExit();
}

void ARoguelikeExitTrigger::ActivateExit()
{
	if (bIsActivated)
	{
		UE_LOG(LogRoguelike, Verbose, TEXT("Exit activation ignored; exit is already active."));
		return;
	}

	bIsActivated = true;
	UE_LOG(LogRoguelike, Log, TEXT("Roguelike exit activated; waiting for player overlap."));

	// If the player was already standing inside the whitebox volume when the
	// reward was chosen, no new BeginOverlap event is guaranteed to arrive.
	if (TriggerBox && !bHasTriggered)
	{
		TArray<AActor*> OverlappingPlayers;
		TriggerBox->GetOverlappingActors(OverlappingPlayers, APlayerCharacter::StaticClass());
		for (AActor* Actor : OverlappingPlayers)
		{
			if (APlayerCharacter* Player = Cast<APlayerCharacter>(Actor))
			{
				HandlePlayerEntered(Player);
				break;
			}
		}
	}
}

void ARoguelikeExitTrigger::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	if (!bIsActivated || bHasTriggered)
	{
		return;
	}

	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!IsValid(Player))
	{
		return;
	}

	HandlePlayerEntered(Player);
}

void ARoguelikeExitTrigger::HandlePlayerEntered(APlayerCharacter* Player)
{
	if (bHasTriggered || !IsValid(Player))
	{
		return;
	}

	bHasTriggered = true;
	UE_LOG(LogRoguelike, Log, TEXT("Player entered roguelike exit: %s."), *Player->GetName());

	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeLevelFlowSubsystem* LevelFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeLevelFlowSubsystem>()
		: nullptr;
	if (!LevelFlow)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Exit cannot request level travel: level-flow subsystem is unavailable."));
		return;
	}

	if (!LevelFlow->AdvanceToNextLevel())
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Exit did not transition. Major=%d MinorIndex=%d; outcome/restart remains external."),
			LevelFlow->GetCurrentMajorLevelId(), LevelFlow->GetCurrentMinorLevelIndex());
	}
}

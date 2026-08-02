// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeExitTrigger.h"

#include "Components/BoxComponent.h"
#include "Player/PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"

ARoguelikeExitTrigger::ARoguelikeExitTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(220.0f, 220.0f, 120.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->ShapeColor = FColor(40, 220, 90, 180);
}

void ARoguelikeExitTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ARoguelikeExitTrigger::HandleBeginOverlap);
	}

	if (ResolveRewardManager())
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
	UE_LOG(LogRoguelike, Log, TEXT("Roguelike exit activated; level transition is not implemented yet."));

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
				bHasTriggered = true;
				UE_LOG(LogRoguelike, Log, TEXT("Player was already inside roguelike exit: %s."), *Player->GetName());
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

	bHasTriggered = true;
	UE_LOG(LogRoguelike, Log, TEXT("Player entered roguelike exit: %s."), *Player->GetName());
}

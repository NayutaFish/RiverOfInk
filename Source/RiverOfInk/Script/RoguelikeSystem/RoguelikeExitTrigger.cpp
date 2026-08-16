// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeExitTrigger.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/PlayerCharacter.h"
#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "UObject/ConstructorHelpers.h"

ARoguelikeExitTrigger::ARoguelikeExitTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	RootComponent = TriggerSphere;
	TriggerSphere->SetSphereRadius(TriggerRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	// PlayerCharacter uses the project's long-term PlayerHitbox channel.
	// Keep this explicit instead of relying on the built-in Pawn channel.
	TriggerSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	TriggerSphere->SetGenerateOverlapEvents(true);
	TriggerSphere->ShapeColor = FColor(40, 220, 90, 180);

	ExitMarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitMarkerMesh"));
	ExitMarkerMesh->SetupAttachment(TriggerSphere);
	ExitMarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExitMarkerMesh->SetGenerateOverlapEvents(false);
	ExitMarkerMesh->SetRelativeScale3D(FVector(3.0f, 3.0f, 2.0f));
	ExitMarkerMesh->SetCastShadow(false);
	ExitMarkerMesh->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMeshAsset(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (MarkerMeshAsset.Succeeded())
	{
		ExitMarkerMesh->SetStaticMesh(MarkerMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarkerMaterialAsset(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MarkerMaterialAsset.Succeeded())
	{
		ExitMarkerMesh->SetMaterial(0, MarkerMaterialAsset.Object);
	}
}

void ARoguelikeExitTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerSphere)
	{
		// Re-assert the project collision contract at runtime. The placed actor or
		// a derived Blueprint can serialize an older response table over constructor defaults.
		TriggerSphere->SetSphereRadius(TriggerRadius);
		TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		TriggerSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
		TriggerSphere->SetGenerateOverlapEvents(true);
		TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &ARoguelikeExitTrigger::HandleBeginOverlap);
	}

	if (ExitVisualClass)
	{
		// 生成美术配置的传送门蓝图（替代代码圆柱）
		FActorSpawnParameters Params;
		Params.Owner = this;
		ExitVisualActor = GetWorld()->SpawnActor<AActor>(ExitVisualClass, GetActorLocation(), GetActorRotation(), Params);
		if (ExitVisualActor)
		{
			ExitVisualActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			ExitVisualActor->SetActorHiddenInGame(!bIsActivated);
		}
	}
	else if (ExitMarkerMesh)
	{
		ExitMarkerMesh->SetHiddenInGame(!bIsActivated);
		if (UMaterialInterface* BaseMaterial = ExitMarkerMesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* MarkerMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			MarkerMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 1.0f, 0.25f));
			ExitMarkerMesh->SetMaterial(0, MarkerMaterial);
		}
	}

	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;
	const bool bIsPreparationRoom = RunFlow
		&& RunFlow->GetRunState() == ERoguelikeRunState::Preparation;
	const bool bIsShopRoom = RunFlow
		&& RunFlow->GetCurrentRoomDefinition().RoomType == ERoguelikeRoomType::Shop;

	if (bIsPreparationRoom)
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Preparation start exit does not require a reward manager."));
	}
	else if (bIsShopRoom)
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Shop exit does not require a reward manager; activating on room entry."));
		ActivateExit();
	}
	else if (ResolveRewardManager())
	{
		RewardManager->OnRewardApplied.AddDynamic(this, &ARoguelikeExitTrigger::HandleRewardApplied);
	}
}

void ARoguelikeExitTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TriggerSphere)
	{
		TriggerSphere->OnComponentBeginOverlap.RemoveDynamic(this, &ARoguelikeExitTrigger::HandleBeginOverlap);
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
	if (ExitVisualActor)
	{
		ExitVisualActor->SetActorHiddenInGame(false);
	}
	else if (ExitMarkerMesh)
	{
		ExitMarkerMesh->SetHiddenInGame(false);
	}
	UE_LOG(LogRoguelike, Log, TEXT("Roguelike exit activated; waiting for player overlap."));

	// If the player was already standing inside the whitebox volume when the
	// reward was chosen, no new BeginOverlap event is guaranteed to arrive.
	if (TriggerSphere && !bHasTriggered)
	{
		TArray<AActor*> OverlappingPlayers;
		TriggerSphere->GetOverlappingActors(OverlappingPlayers, APlayerCharacter::StaticClass());
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
	URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;
	if (!RunFlow)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Exit cannot request room travel: run-flow subsystem is unavailable."));
		return;
	}

	if (!RunFlow->RequestAdvanceFromExit())
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Exit did not transition. RunState=%d MajorStage=%d RoomIndex=%d."),
			static_cast<int32>(RunFlow->GetRunState()),
			RunFlow->GetCurrentMajorStageIndex(),
			RunFlow->GetCurrentRoomIndex());
	}
}

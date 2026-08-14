// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/RiverOfInkGameMode.h"
#include "Engine/GameInstance.h"
#include "GameMode/RiverOfInkPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelRoomManager/DemoRoomManager.h"
#include "Player/PlayerCharacter.h"
#include "CameraManager/CameraManager.h"
#include "Camera/PlayerCameraManager.h"
#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ARiverOfInkGameMode::ARiverOfInkGameMode()
{
	// 使用玩家蓝图；MaxHealth 和抗性在其 HealthComponent 子对象中配置。
	static ConstructorHelpers::FClassFinder<APlayerCharacter> PlayerBlueprintAsset(
		TEXT("/Game/Blueprint/GamePlay/Player/BP_PlayerCharacter.BP_PlayerCharacter_C"));
	if (PlayerBlueprintAsset.Succeeded())
	{
		DefaultPawnClass = PlayerBlueprintAsset.Class;
	}
	else
	{
		// 蓝图缺失时回退到纯 C++ 类
		DefaultPawnClass = APlayerCharacter::StaticClass();
	}
	// 自带的 PlayerController：鼠标全程显示
	PlayerControllerClass = ARiverOfInkPlayerController::StaticClass();
}

void ARiverOfInkGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URoguelikeRunFlowSubsystem* RunFlow = GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>())
		{
			RunFlow->OnRunStateChanged.AddDynamic(this, &ARiverOfInkGameMode::HandleRunStateChanged);
			RunFlow->NotifyRoomLoaded(GetWorld());
			RunFlow->EnsurePreparationStartExit();
		}
	}

	BindRoomActors();

	// 生成纯 C++ 相机管理器（玩家生成后由它自动接管并跟随）
	GetWorld()->SpawnActor<ACameraManager>(ACameraManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);

	// RunFlow travels from the black screen created by the Stage 1 intro.
	// PlayerCameraManager survives the map transition, while the old menu
	// camera does not, so every gameplay room owns this minimal fade-in.
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (IsValid(PlayerController->PlayerCameraManager))
		{
			PlayerController->PlayerCameraManager->StartCameraFade(
				1.0f,
				0.0f,
				0.45f,
				FLinearColor::Black,
				false,
				true);
		}
	}
}

void ARiverOfInkGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URoguelikeRunFlowSubsystem* RunFlow = GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>())
		{
			RunFlow->OnRunStateChanged.RemoveDynamic(this, &ARiverOfInkGameMode::HandleRunStateChanged);
		}
	}

	if (IsValid(BoundRoomManager))
	{
		BoundRoomManager->OnRoomStarted.RemoveDynamic(this, &ARiverOfInkGameMode::HandleRoomStarted);
		BoundRoomManager->OnRoomCleared.RemoveDynamic(this, &ARiverOfInkGameMode::HandleRoomCleared);
	}

	if (IsValid(BoundRewardManager))
	{
		BoundRewardManager->OnRewardApplied.RemoveDynamic(this, &ARiverOfInkGameMode::HandleRewardApplied);
	}

	Super::EndPlay(EndPlayReason);
}

void ARiverOfInkGameMode::BindRoomActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> RoomManagers;
	UGameplayStatics::GetAllActorsOfClass(World, ADemoRoomManager::StaticClass(), RoomManagers);
	if (RoomManagers.Num() > 0)
	{
		BoundRoomManager = Cast<ADemoRoomManager>(RoomManagers[0]);
		if (IsValid(BoundRoomManager))
		{
			BoundRoomManager->OnRoomStarted.AddDynamic(this, &ARiverOfInkGameMode::HandleRoomStarted);
			BoundRoomManager->OnRoomCleared.AddDynamic(this, &ARiverOfInkGameMode::HandleRoomCleared);
		}
	}

	TArray<AActor*> RewardManagers;
	UGameplayStatics::GetAllActorsOfClass(World, ARoguelikeRewardManager::StaticClass(), RewardManagers);
	if (RewardManagers.Num() > 0)
	{
		BoundRewardManager = Cast<ARoguelikeRewardManager>(RewardManagers[0]);
		if (IsValid(BoundRewardManager))
		{
			BoundRewardManager->OnRewardApplied.AddDynamic(this, &ARiverOfInkGameMode::HandleRewardApplied);
		}
	}

	UE_LOG(LogRoguelikeRunFlow, Verbose,
		TEXT("Room flow actor binding complete. RoomManager=%s RewardManager=%s."),
		*GetNameSafe(BoundRoomManager), *GetNameSafe(BoundRewardManager));
}

void ARiverOfInkGameMode::HandleRunStateChanged(
	ERoguelikeRunState PreviousState,
	ERoguelikeRunState NewState,
	ERoguelikeRunTransitionReason Reason
)
{
	(void)PreviousState;
	(void)Reason;

	if (NewState == ERoguelikeRunState::InRoom)
	{
		TransitionRoomState(ERoguelikeRoomState::Entering);
	}
	else if (NewState == ERoguelikeRunState::LoadingRoom
		&& CurrentRoomState == ERoguelikeRoomState::Completed)
	{
		TransitionRoomState(ERoguelikeRoomState::Exiting);
	}
}

void ARiverOfInkGameMode::HandleRoomStarted()
{
	if (CurrentRoomState != ERoguelikeRoomState::Entering)
	{
		UE_LOG(LogRoguelikeRunFlow, Verbose,
			TEXT("Room start event ignored in room state %d."),
			static_cast<int32>(CurrentRoomState));
		return;
	}

	TransitionRoomState(ERoguelikeRoomState::Ready);
	TransitionRoomState(ERoguelikeRoomState::Combat);
}

void ARiverOfInkGameMode::HandleRoomCleared()
{
	if (CurrentRoomState != ERoguelikeRoomState::Combat)
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Room clear event rejected in room state %d."),
			static_cast<int32>(CurrentRoomState));
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		URoguelikeRunFlowSubsystem* RunFlow = GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>();
		URoguelikeEconomySubsystem* Economy = GameInstance->GetSubsystem<URoguelikeEconomySubsystem>();
		if (RunFlow && Economy && IsValid(BoundRoomManager))
		{
			Economy->GrantRoomResult(
				RunFlow->GetCurrentMajorStageIndex(),
				RunFlow->GetCurrentRoomIndex(),
				BoundRoomManager->PureInkRoomResultReward);
		}
		else
		{
			UE_LOG(LogRoguelike, Warning,
				TEXT("Room result Pure Ink could not be granted: RunFlow=%s Economy=%s RoomManager=%s."),
				RunFlow ? TEXT("valid") : TEXT("null"),
				Economy ? TEXT("valid") : TEXT("null"),
				IsValid(BoundRoomManager) ? TEXT("valid") : TEXT("null"));
		}
	}

	TransitionRoomState(ERoguelikeRoomState::Reward);
}

void ARiverOfInkGameMode::HandleRewardApplied(const FRoguelikeRewardOption& Reward)
{
	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Room reward applied: %s."), *Reward.Title.ToString());

	if (CurrentRoomState == ERoguelikeRoomState::Reward)
	{
		TransitionRoomState(ERoguelikeRoomState::Completed);
	}
}

bool ARiverOfInkGameMode::TransitionRoomState(ERoguelikeRoomState NextState)
{
	if (CurrentRoomState == NextState)
	{
		return true;
	}

	if (!IsRoomTransitionAllowed(NextState))
	{
		UE_LOG(LogRoguelikeRunFlow, Warning,
			TEXT("Rejected room-state transition: %d -> %d."),
			static_cast<int32>(CurrentRoomState), static_cast<int32>(NextState));
		return false;
	}

	const ERoguelikeRoomState PreviousState = CurrentRoomState;
	CurrentRoomState = NextState;
	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Room state changed: %d -> %d."),
		static_cast<int32>(PreviousState), static_cast<int32>(CurrentRoomState));
	OnRoomStateChanged.Broadcast(PreviousState, CurrentRoomState);
	return true;
}

bool ARiverOfInkGameMode::IsRoomTransitionAllowed(ERoguelikeRoomState NextState) const
{
	switch (CurrentRoomState)
	{
	case ERoguelikeRoomState::Initializing:
		return NextState == ERoguelikeRoomState::Entering;
	case ERoguelikeRoomState::Entering:
		return NextState == ERoguelikeRoomState::Ready;
	case ERoguelikeRoomState::Ready:
		return NextState == ERoguelikeRoomState::Combat;
	case ERoguelikeRoomState::Combat:
		return NextState == ERoguelikeRoomState::Reward;
	case ERoguelikeRoomState::Reward:
		return NextState == ERoguelikeRoomState::Completed;
	case ERoguelikeRoomState::Completed:
		return NextState == ERoguelikeRoomState::Exiting;
	case ERoguelikeRoomState::Exiting:
		return false;
	default:
		return false;
	}
}

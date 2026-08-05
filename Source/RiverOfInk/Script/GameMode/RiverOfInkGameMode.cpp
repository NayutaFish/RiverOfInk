// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/RiverOfInkGameMode.h"
#include "Engine/GameInstance.h"
#include "GameMode/RiverOfInkPlayerController.h"
#include "Player/PlayerCharacter.h"
#include "CameraManager/CameraManager.h"
#include "RoguelikeSystem/RoguelikeLevelFlowSubsystem.h"
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
		if (URoguelikeLevelFlowSubsystem* LevelFlow = GameInstance->GetSubsystem<URoguelikeLevelFlowSubsystem>())
		{
			LevelFlow->EnsurePreparationExit();
		}
	}

	// 生成纯 C++ 相机管理器（玩家生成后由它自动接管并跟随）
	GetWorld()->SpawnActor<ACameraManager>(ACameraManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
}

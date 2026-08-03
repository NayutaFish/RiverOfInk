// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/RiverOfInkGameMode.h"
#include "GameMode/RiverOfInkPlayerController.h"
#include "Player/PlayerCharacter.h"
#include "CameraManager/CameraManager.h"
#include "Engine/World.h"

ARiverOfInkGameMode::ARiverOfInkGameMode()
{
	// 纯 C++ 方案：直接使用 C++ 玩家类，不依赖任何蓝图
	DefaultPawnClass = APlayerCharacter::StaticClass();
	// 自带的 PlayerController：鼠标全程显示
	PlayerControllerClass = ARiverOfInkPlayerController::StaticClass();
}

void ARiverOfInkGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 生成纯 C++ 相机管理器（玩家生成后由它自动接管并跟随）
	GetWorld()->SpawnActor<ACameraManager>(ACameraManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
}

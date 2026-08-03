// Copyright Our Copyright notice in the Description page of Project Settings.

#include "GameMode/RiverOfInkPlayerController.h"

ARiverOfInkPlayerController::ARiverOfInkPlayerController()
{
	// 鼠标全程显示，进入游戏后不自动隐藏
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}
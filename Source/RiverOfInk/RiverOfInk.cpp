// Copyright Epic Games, Inc. All Rights Reserved.

#include "RiverOfInk.h"
#include "FreezeFrameManager/FreezeFrameManager.h"
#include "Modules/ModuleManager.h"

/**
 * 主游戏模块
 * 在模块启动时注册全局事件订阅（如顿帧的敌人死亡事件）。
 */
class FRiverOfInkModule : public FDefaultGameModuleImpl
{
	virtual void StartupModule() override
	{
		FFreezeFrameManager::EnsureSubscribed();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FRiverOfInkModule, RiverOfInk, "RiverOfInk");

DEFINE_LOG_CATEGORY(LogRiverOfInk);

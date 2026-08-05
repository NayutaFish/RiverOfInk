// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class UWorld;

/**
 * 顿帧管理器（纯静态类，非 UObject）
 *
 * 用法（一行触发顿帧，冻结世界时间 flow 0.1 秒后自动恢复）：
 *     FFreezeFrameManager::Trigger(GetWorld(), 0.1f);
 *
 * 特性：
 *   - 顿帧期间再次调用无效（首个顿帧结束后才接受下一次）
 *   - 仅冻结世界（SetGlobalTimeDilation），UI 不受影响
 *   - 恢复计时用 FTimerManager（不受时间膨胀影响），时间缩放回 1 前自动兜底
 */
class RIVEROFINK_API FFreezeFrameManager
{
public:
	/**
	 * 触发顿帧
	 * @param InWorld      世界上下文（UWorld*）
	 * @param FreezeTime   顿帧时长（秒），默认 0.1
	 */
	static void Trigger(UWorld* InWorld, float FreezeTime = 0.1f);

	/** 订阅敌人死亡事件（模块启动时调用一次；重复调用安全） */
	static void EnsureSubscribed();

private:
	/** 敌人死亡事件回调：直接性攻击 + 近战 AttackArea 时顿帧 */
	static void HandleEnemyDied(const struct FNonPlayerDiedEvent& InEvent);

	/** 引擎核心 Ticker 回调：按真实帧时间累计，到期恢复时间流速 */
	static bool OnResumeTick(float DeltaTime);

	/** 是否为顿帧中（期间重复调用无效） */
	static bool bIsFreezing;

	/** 是否已订阅敌人死亡事件 */
	static bool bSubscribed;

	/** 恢复用 Ticker 句柄 */
	static FTSTicker::FDelegateHandle ResumeTickerHandle;

	/** 被冻结的世界（弱引用，世界销毁后自动失效） */
	static TWeakObjectPtr<UWorld> FrozenWorld;

	/** 剩余冻结时间（秒） */
	static float FreezeRemaining;
};

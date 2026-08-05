// Fill out your copyright notice in the Description page of Project Settings.

#include "FreezeFrameManager/FreezeFrameManager.h"
#include "RiverOfInk.h"
#include "Common/AttackAreaBase.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

/** 击杀事件后到真正顿帧之间的延迟（秒） */
static constexpr float KillHitStopDelay = 0.08f;

/** 击杀顿帧时长（秒） */
static constexpr float KillFreezeTime = 0.2f;

bool FFreezeFrameManager::bIsFreezing = false;
bool FFreezeFrameManager::bSubscribed = false;
FTSTicker::FDelegateHandle FFreezeFrameManager::ResumeTickerHandle;
TWeakObjectPtr<UWorld> FFreezeFrameManager::FrozenWorld;
float FFreezeFrameManager::FreezeRemaining = 0.0f;

void FFreezeFrameManager::EnsureSubscribed()
{
	if (bSubscribed)
	{
		return;
	}

	bSubscribed = true;
	FEventBus::Subscribe<FNonPlayerDiedEvent>([](const FNonPlayerDiedEvent& InEvent)
	{
		HandleEnemyDied(InEvent);
	});
	UE_LOG(LogRiverOfInk, Log, TEXT("FreezeFrame: Subscribed to enemy death events."));
}

void FFreezeFrameManager::HandleEnemyDied(const FNonPlayerDiedEvent& InEvent)
{
	// 仅直接性攻击触发的死亡才顿帧
	if (!InEvent.DamageInfo.bIsDirectDamage)
	{
		return;
	}

	// 仅近战 AttackArea 触发的击杀才顿帧
	if (!InEvent.AttackArea || !InEvent.AttackArea->bIsMeleeAttack)
	{
		return;
	}

	// 世界上下文：优先攻击区域，兜底死亡敌人
	UWorld* World = InEvent.AttackArea ? InEvent.AttackArea->GetWorld() : nullptr;
	if (!World && InEvent.Victim)
	{
		World = InEvent.Victim->GetWorld();
	}
	if (!World)
	{
		return;
	}

	// 延迟 KillHitStopDelay 秒后再真正执行顿帧（世界弱引用保护，销毁后自动失效）
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		Trigger(World, KillFreezeTime);
	}), KillHitStopDelay, false);
}

void FFreezeFrameManager::Trigger(UWorld* InWorld, float FreezeTime)
{
	if (!InWorld)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("FreezeFrame: World is null, skipped."));
		return;
	}

	// 顿帧中重复调用无效（第一个顿帧结束后才接受下一次）
	// 自愈：若 bIsFreezing 为 true 但世界 dilation 已恢复（脏状态，如热重载残留），强制重置
	if (bIsFreezing)
	{
		if (UGameplayStatics::GetGlobalTimeDilation(InWorld) >= 1.0f)
		{
			bIsFreezing = false;
			FrozenWorld.Reset();
			FreezeRemaining = 0.0f;
			if (ResumeTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(ResumeTickerHandle);
				ResumeTickerHandle.Reset();
			}
		}
		else
		{
			return;
		}
	}

	if (FreezeTime <= 0.0f)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("FreezeFrame: FreezeTime must be > 0, skipped."));
		return;
	}

	bIsFreezing = true;
	FrozenWorld = InWorld;
	FreezeRemaining = FreezeTime;

	// 冻结全局时间流速（只冻世界，UI 不受影响）
	UGameplayStatics::SetGlobalTimeDilation(InWorld, 0.0f);
	UE_LOG(LogRiverOfInk, Log, TEXT("FreezeFrame: Frozen for %.2f s."), FreezeTime);

	// 用引擎核心 Ticker 恢复：它基于真实帧时间，不受世界时间膨胀影响。
	// 注意：不能用世界 FTimerManager（其计时依赖 TimeSeconds，dilation=0 时永不触发）。
	ResumeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateStatic(&FFreezeFrameManager::OnResumeTick));
}

bool FFreezeFrameManager::OnResumeTick(float DeltaTime)
{
	FreezeRemaining -= DeltaTime;
	if (FreezeRemaining > 0.0f)
	{
		return true; // 顿帧未结束，继续等待
	}

	// 恢复时间流速（世界若已销毁则无需恢复，新世界流速正常）
	if (UWorld* World = FrozenWorld.Get())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}

	bIsFreezing = false;
	FrozenWorld.Reset();
	ResumeTickerHandle.Reset();
	UE_LOG(LogRiverOfInk, Log, TEXT("FreezeFrame: Resumed."));
	return false; // 移除 Ticker
}

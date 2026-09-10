// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/SettlementDataRecorder.h"

#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Engine/Engine.h"
#include "RiverOfInk.h"

// ──────────────────────────────
// 静态成员定义
// ──────────────────────────────

int32 USettlementDataRecorder::NonPlayerDeathCount = 0;
int32 USettlementDataRecorder::EliteEnemyDeathCount = 0;
float USettlementDataRecorder::TotalDamageDealtToNonPlayer = 0.0f;
int32 USettlementDataRecorder::DamageDealtHitCount = 0;
float USettlementDataRecorder::HighestSingleHitDamage = 0.0f;
int32 USettlementDataRecorder::TotalShopCurrencyGained = 0;
int32 USettlementDataRecorder::ShopPurchaseCount = 0;
bool USettlementDataRecorder::bScreenDebugEnabled = true;

bool USettlementDataRecorder::bInitialized = false;
FDelegateHandle USettlementDataRecorder::NonPlayerDiedHandle;
FDelegateHandle USettlementDataRecorder::EliteEnemyDiedHandle;
FDelegateHandle USettlementDataRecorder::NonPlayerTakeDamageHandle;
FDelegateHandle USettlementDataRecorder::ShopCurrencyGainedHandle;
FDelegateHandle USettlementDataRecorder::ShopPurchaseCompletedHandle;

namespace
{
	/** 屏幕调试消息的固定 Key：相同 Key 会原地覆盖，保证屏幕只占一行。 */
	constexpr uint64 SettlementScreenDebugKey = 0x5E7711;

	/**
	 * 模块加载时自动完成订阅：静态类没有实例，用一个文件级静态对象做一次性初始化。
	 * 只订阅、不在此对象的析构里退订——模块卸载时的静态析构顺序不可控，
	 * 去动 EventBus 的函数内静态委托反而不安全（委托链本身会随模块一起消失）。
	 */
	struct FSettlementDataRecorderBootstrap
	{
		FSettlementDataRecorderBootstrap()
		{
			USettlementDataRecorder::Initialize();
		}
	};

	const FSettlementDataRecorderBootstrap GSettlementDataRecorderBootstrap;
}

// ──────────────────────────────
// 订阅管理
// ──────────────────────────────

void USettlementDataRecorder::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	NonPlayerDiedHandle = FEventBus::Subscribe<FNonPlayerDiedEvent>(
		[](const FNonPlayerDiedEvent& Event) { HandleNonPlayerDied(Event); });

	EliteEnemyDiedHandle = FEventBus::Subscribe<FOnEliteEnemyDiedEvent>(
		[](const FOnEliteEnemyDiedEvent& Event) { HandleEliteEnemyDied(Event); });

	NonPlayerTakeDamageHandle = FEventBus::Subscribe<FOnNonPlayerTakeDamageFromPlayer>(
		[](const FOnNonPlayerTakeDamageFromPlayer& Event) { HandleNonPlayerTakeDamageFromPlayer(Event); });

	ShopCurrencyGainedHandle = FEventBus::Subscribe<FShopCurrencyGainedEvent>(
		[](const FShopCurrencyGainedEvent& Event) { HandleShopCurrencyGained(Event); });

	ShopPurchaseCompletedHandle = FEventBus::Subscribe<FShopPurchaseCompletedEvent>(
		[](const FShopPurchaseCompletedEvent& Event) { HandleShopPurchaseCompleted(Event); });

	bInitialized = true;

	UE_LOG(LogRiverOfInk, Log,
		TEXT("SettlementDataRecorder initialized: subscribed to combat and shop events."));
}

void USettlementDataRecorder::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	FEventBus::Unsubscribe<FNonPlayerDiedEvent>(NonPlayerDiedHandle);
	FEventBus::Unsubscribe<FOnEliteEnemyDiedEvent>(EliteEnemyDiedHandle);
	FEventBus::Unsubscribe<FOnNonPlayerTakeDamageFromPlayer>(NonPlayerTakeDamageHandle);
	FEventBus::Unsubscribe<FShopCurrencyGainedEvent>(ShopCurrencyGainedHandle);
	FEventBus::Unsubscribe<FShopPurchaseCompletedEvent>(ShopPurchaseCompletedHandle);

	bInitialized = false;
}

void USettlementDataRecorder::Reset()
{
	NonPlayerDeathCount = 0;
	EliteEnemyDeathCount = 0;
	TotalDamageDealtToNonPlayer = 0.0f;
	DamageDealtHitCount = 0;
	HighestSingleHitDamage = 0.0f;
	TotalShopCurrencyGained = 0;
	ShopPurchaseCount = 0;

	UE_LOG(LogRiverOfInk, Log, TEXT("SettlementDataRecorder reset."));

	// 重置也算一次数据更新，同步刷新屏幕快照（归零）
	PrintSnapshotToScreen();
}

void USettlementDataRecorder::PrintSnapshotToScreen()
{
#if !UE_BUILD_SHIPPING
	if (!bScreenDebugEnabled)
	{
		return;
	}

	// 标签用 ASCII：屏幕调试消息的画布字体不一定带中文字形，中文会显示成方块
	const FString Snapshot = FString::Printf(
		TEXT("[Settlement] Kills=%d (Elite=%d) Damage=%.0f InkGained=%d Purchases=%d"),
		NonPlayerDeathCount,
		EliteEnemyDeathCount,
		TotalDamageDealtToNonPlayer,
		TotalShopCurrencyGained,
		ShopPurchaseCount);

	if (GEngine)
	{
		// 固定 Key + 5 秒存活：数据更新时原地刷新同一条消息，不会把屏幕刷满
		GEngine->AddOnScreenDebugMessage(SettlementScreenDebugKey, 5.0f, FColor::Cyan, Snapshot, false);
	}
#endif
}

float USettlementDataRecorder::GetAverageDamagePerHit()
{
	return DamageDealtHitCount > 0
		? TotalDamageDealtToNonPlayer / static_cast<float>(DamageDealtHitCount)
		: 0.0f;
}

// ──────────────────────────────
// 事件处理
// ──────────────────────────────

void USettlementDataRecorder::HandleNonPlayerDied(const FNonPlayerDiedEvent& Event)
{
	(void)Event;

	++NonPlayerDeathCount;
	PrintSnapshotToScreen();
}

void USettlementDataRecorder::HandleEliteEnemyDied(const FOnEliteEnemyDiedEvent& Event)
{
	(void)Event;

	++EliteEnemyDeathCount;
	PrintSnapshotToScreen();
}

void USettlementDataRecorder::HandleNonPlayerTakeDamageFromPlayer(const FOnNonPlayerTakeDamageFromPlayer& Event)
{
	// 0 伤害的命中不计入统计（只统计真正掉血的命中）
	if (Event.FinalDamage <= 0.0f)
	{
		return;
	}

	TotalDamageDealtToNonPlayer += Event.FinalDamage;
	++DamageDealtHitCount;
	HighestSingleHitDamage = FMath::Max(HighestSingleHitDamage, Event.FinalDamage);

	PrintSnapshotToScreen();
}

void USettlementDataRecorder::HandleShopCurrencyGained(const FShopCurrencyGainedEvent& Event)
{
	if (Event.Amount <= 0)
	{
		return;
	}

	TotalShopCurrencyGained += Event.Amount;

	PrintSnapshotToScreen();
}

void USettlementDataRecorder::HandleShopPurchaseCompleted(const FShopPurchaseCompletedEvent& Event)
{
	(void)Event;

	++ShopPurchaseCount;
	PrintSnapshotToScreen();
}

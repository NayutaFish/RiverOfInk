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
int32 USettlementDataRecorder::TotalShopCurrencyGained = 0;
int32 USettlementDataRecorder::ShopPurchaseCount = 0;
TArray<FSettlementRewardPick> USettlementDataRecorder::RewardPicks;
bool USettlementDataRecorder::bScreenDebugEnabled = true;

bool USettlementDataRecorder::bInitialized = false;
FDelegateHandle USettlementDataRecorder::NonPlayerDiedHandle;
FDelegateHandle USettlementDataRecorder::EliteEnemyDiedHandle;
FDelegateHandle USettlementDataRecorder::NonPlayerTakeDamageHandle;
FDelegateHandle USettlementDataRecorder::ShopCurrencyGainedHandle;
FDelegateHandle USettlementDataRecorder::ShopPurchaseCompletedHandle;
FDelegateHandle USettlementDataRecorder::RewardSelectedHandle;

namespace
{
	/** 屏幕调试消息的固定 Key：相同 Key 会原地覆盖，保证屏幕只占一行。 */
	constexpr uint64 SettlementScreenDebugKey = 0x5E7711;

	/** 增益顺序列表的固定 Key（与上面那条分开，屏幕共占两行）。 */
	constexpr uint64 SettlementRewardPicksScreenDebugKey = 0x5E7712;

	/** 屏幕上最多展示多少条增益（超出的仍会记录，只是不打印）。 */
	constexpr int32 MaxRewardPicksOnScreen = 12;

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

	RewardSelectedHandle = FEventBus::Subscribe<FRewardSelectedEvent>(
		[](const FRewardSelectedEvent& Event) { HandleRewardSelected(Event); });

	bInitialized = true;

	UE_LOG(LogRiverOfInk, Log,
		TEXT("SettlementDataRecorder initialized: subscribed to combat, shop and reward events."));
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
	FEventBus::Unsubscribe<FRewardSelectedEvent>(RewardSelectedHandle);

	bInitialized = false;
}

void USettlementDataRecorder::Reset()
{
	NonPlayerDeathCount = 0;
	EliteEnemyDeathCount = 0;
	TotalDamageDealtToNonPlayer = 0.0f;
	TotalShopCurrencyGained = 0;
	ShopPurchaseCount = 0;
	RewardPicks.Reset();

	UE_LOG(LogRiverOfInk, Log, TEXT("SettlementDataRecorder reset."));

	// 重置也算一次数据更新，同步刷新屏幕快照（归零）
	PrintSnapshotToScreen();
	PrintRewardPicksToScreen();
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

void USettlementDataRecorder::PrintRewardPicksToScreen()
{
#if !UE_BUILD_SHIPPING
	if (!bScreenDebugEnabled)
	{
		return;
	}

	if (!GEngine)
	{
		return;
	}

	// 标签依然是 ASCII（屏幕调试字体不一定带中文字形）；奖励名用 Title，中文可能显示成方块，
	// 但 EventBus/结算界面读的是 FString DisplayName，不受影响。
	const int32 ShownCount = FMath::Min(RewardPicks.Num(), MaxRewardPicksOnScreen);

	FString PicksText;
	for (int32 Index = 0; Index < ShownCount; ++Index)
	{
		const FSettlementRewardPick& Pick = RewardPicks[Index];

		if (Index > 0)
		{
			PicksText += TEXT(" > ");
		}

		PicksText += Pick.RewardId.IsEmpty() ? FString(TEXT("<unknown>")) : Pick.RewardId;

		if (Pick.StackCount > 1)
		{
			PicksText += FString::Printf(TEXT("x%d"), Pick.StackCount);
		}
	}

	if (RewardPicks.Num() > ShownCount)
	{
		PicksText += FString::Printf(TEXT(" ... (+%d)"), RewardPicks.Num() - ShownCount);
	}

	const FString Message = FString::Printf(
		TEXT("[Settlement] Picks(%d): %s"),
		RewardPicks.Num(),
		PicksText.IsEmpty() ? TEXT("-") : *PicksText);

	GEngine->AddOnScreenDebugMessage(
		SettlementRewardPicksScreenDebugKey, 5.0f, FColor::Yellow, Message, false);
#endif
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
	// 只累计真正掉血的伤害
	if (Event.FinalDamage <= 0.0f)
	{
		return;
	}

	TotalDamageDealtToNonPlayer += Event.FinalDamage;

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

void USettlementDataRecorder::HandleRewardSelected(const FRewardSelectedEvent& Event)
{
	// 只追加、不合并也不排序：结算界面要还原玩家实际的清场选择顺序
	FSettlementRewardPick& NewPick = RewardPicks.AddDefaulted_GetRef();
	NewPick.RewardId = Event.RewardId;
	NewPick.DisplayName = Event.Title.ToString();
	NewPick.StackCount = FMath::Max(1, Event.StackCount);

	UE_LOG(LogRiverOfInk, Log,
		TEXT("SettlementDataRecorder reward pick #%d recorded: id=%s title=%s stack=%d"),
		RewardPicks.Num(),
		*NewPick.RewardId,
		*NewPick.DisplayName,
		NewPick.StackCount);

	PrintSnapshotToScreen();
	PrintRewardPicksToScreen();
}

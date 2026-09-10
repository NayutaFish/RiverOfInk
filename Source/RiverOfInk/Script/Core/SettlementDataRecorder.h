// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SettlementDataRecorder.generated.h"

struct FNonPlayerDiedEvent;
struct FOnEliteEnemyDiedEvent;
struct FOnNonPlayerTakeDamageFromPlayer;
struct FShopCurrencyGainedEvent;
struct FShopPurchaseCompletedEvent;
struct FRewardSelectedEvent;

/**
 * 一次清场奖励（增益）选择记录。
 */
USTRUCT(BlueprintType)
struct FSettlementRewardPick
{
	GENERATED_BODY()

	/** 稳定标识（由枚举名组成，例如 "Modifier.ProjectileHoming"） */
	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FString RewardId;

	/** 卡面标题（显示名，例如 "引墨"） */
	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	FString DisplayName;

	/** 本次叠加层数 */
	UPROPERTY(BlueprintReadOnly, Category = "Settlement")
	int32 StackCount = 1;
};

/**
 * 结算数据记录器（静态类）。
 *
 * 模块加载时自动订阅以下战斗/经济事件，把数据累计到静态变量中，供结算界面/外部系统通过静态方法读取：
 *   - FNonPlayerDiedEvent                → 非玩家单位死亡总数
 *   - FOnEliteEnemyDiedEvent             → 精英敌人死亡数
 *   - FOnNonPlayerTakeDamageFromPlayer   → 玩家对非玩家单位造成的伤害总和
 *   - FShopCurrencyGainedEvent           → 累计获得的商店货币（纯墨收入）
 *   - FShopPurchaseCompletedEvent        → 累计完成购买的次数
 *   - FRewardSelectedEvent               → 清场增益选择的顺序记录（按选择先后追加，不合并、不排序）
 *
 * 用法：
 *   - 读取：USettlementDataRecorder::GetTotalDamageDealtToNonPlayer() 等（蓝图里也能直接调）
 *   - 顺序增益：GetRewardPicks()（每项含 RewardId / DisplayName / StackCount，按选择先后）
 *   - 新一轮/新房间开始前清空：USettlementDataRecorder::Reset()
 *   - 订阅是幂等的；默认在模块加载时已自动完成，无需手动调用 Initialize()
 *
 * 屏幕调试：任意一项数据更新（含 Reset）时，会把当前 5 项指标刷新到屏幕上的同一条调试消息里，
 * 并用第二个固定 Key 打印已选增益的顺序列表，两者都原地更新，不会刷屏；
 * 可用 SetScreenDebugEnabled(false) 关闭（Shipping 构建下不打印）。
 */
UCLASS()
class RIVEROFINK_API USettlementDataRecorder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 订阅全部战斗事件（幂等；模块加载时已自动调用过一次）。 */
	UFUNCTION(BlueprintCallable, Category = "Settlement")
	static void Initialize();

	/** 取消订阅并标记为未初始化（一般只在模块卸载时调用）。 */
	UFUNCTION(BlueprintCallable, Category = "Settlement")
	static void Shutdown();

	/** 清空所有累计数据（保留订阅）。 */
	UFUNCTION(BlueprintCallable, Category = "Settlement")
	static void Reset();

	/** 是否已完成事件订阅。 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	static bool IsInitialized() { return bInitialized; }

	/** 非玩家单位（含精英）死亡总数。 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	static int32 GetNonPlayerDeathCount() { return NonPlayerDeathCount; }

	/** 精英敌人死亡数。 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	static int32 GetEliteEnemyDeathCount() { return EliteEnemyDeathCount; }

	/** 玩家对非玩家单位造成的最终伤害总和。 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	static float GetTotalDamageDealtToNonPlayer() { return TotalDamageDealtToNonPlayer; }

	/** 累计获得的商店货币（纯墨收入总和，不含商店退款）。 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	static int32 GetTotalShopCurrencyGained() { return TotalShopCurrencyGained; }

	/** 累计完成商店购买的次数。 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	static int32 GetShopPurchaseCount() { return ShopPurchaseCount; }

	// ── 清场增益选择（按选择顺序） ──

	/** 已选增益数量。 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	static int32 GetRewardPickCount() { return RewardPicks.Num(); }

	/** 按选择顺序取全部增益记录（返回副本，蓝图可用）。 */
	UFUNCTION(BlueprintPure, Category = "Settlement")
	static TArray<FSettlementRewardPick> GetRewardPicks() { return RewardPicks; }

	/** 是否把数据快照打印到屏幕上（默认开启）。 */
	UFUNCTION(BlueprintCallable, Category = "Settlement")
	static void SetScreenDebugEnabled(bool bEnabled) { bScreenDebugEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Settlement")
	static bool IsScreenDebugEnabled() { return bScreenDebugEnabled; }

private:
	/** 把当前 5 项指标（对应 5 个事件）刷新到屏幕上的同一条调试消息里。 */
	static void PrintSnapshotToScreen();

	/** 把已选增益的顺序列表刷新到屏幕上的第二条调试消息里。 */
	static void PrintRewardPicksToScreen();

	// ── 事件处理 ──

	static void HandleNonPlayerDied(const FNonPlayerDiedEvent& Event);
	static void HandleEliteEnemyDied(const FOnEliteEnemyDiedEvent& Event);
	static void HandleNonPlayerTakeDamageFromPlayer(const FOnNonPlayerTakeDamageFromPlayer& Event);
	static void HandleShopCurrencyGained(const FShopCurrencyGainedEvent& Event);
	static void HandleShopPurchaseCompleted(const FShopPurchaseCompletedEvent& Event);
	static void HandleRewardSelected(const FRewardSelectedEvent& Event);

	// ── 累计数据 ──

	/** 非玩家单位死亡总数 */
	static int32 NonPlayerDeathCount;

	/** 精英敌人死亡数 */
	static int32 EliteEnemyDeathCount;

	/** 玩家造成的最终伤害总和 */
	static float TotalDamageDealtToNonPlayer;

	/** 累计获得的商店货币（纯墨） */
	static int32 TotalShopCurrencyGained;

	/** 累计完成商店购买的次数 */
	static int32 ShopPurchaseCount;

	/** 清场增益选择记录（按选择顺序追加） */
	static TArray<FSettlementRewardPick> RewardPicks;

	/** 屏幕调试开关 */
	static bool bScreenDebugEnabled;

	// ── 订阅状态 ──

	static bool bInitialized;

	static FDelegateHandle NonPlayerDiedHandle;
	static FDelegateHandle EliteEnemyDiedHandle;
	static FDelegateHandle NonPlayerTakeDamageHandle;
	static FDelegateHandle ShopCurrencyGainedHandle;
	static FDelegateHandle ShopPurchaseCompletedHandle;
	static FDelegateHandle RewardSelectedHandle;
};

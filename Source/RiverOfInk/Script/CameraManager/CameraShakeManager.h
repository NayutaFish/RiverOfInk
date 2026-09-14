// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "CameraManager/CameraShakeTypes.h"

class UWorld;
class ACameraManager;

/**
 * 相机震动管理器（纯静态类，非 UObject）
 *
 * 用法（一行触发震动）：
 *     FCameraShakeManager::Trigger(GetWorld(), 0.3f, 40.0f);
 *
 * 特性：
 *   - 模块启动时自动订阅玩家受直接性攻击事件，受击震动 0.3 秒
 *   - 玩家造成有效伤害时，可由攻击区域的反馈绑定请求预设震动
 *   - 基于引擎核心 Ticker（真实帧时间），不受顿帧/时间膨胀影响
 *   - 震动强度随时间衰减，结束时偏移归零，不残留
 *   - 震动期间再次触发：重新开始（强度取新值，时长取新值）
 */
class RIVEROFINK_API FCameraShakeManager
{
public:
	/**
	 * 触发相机震动。
	 * @param InWorld       世界上下文（UWorld*）
	 * @param Duration      震动时长（秒），默认 0.3
	 * @param Intensity     未缩放的震动强度（偏移幅度，单位），默认 75
	 * @param Scale         强度倍率；所有调用均会经过项目设置中的顶层钳制
	 */
	static void Trigger(UWorld* InWorld, float Duration = 0.3f, float Intensity = 150.0f, float Scale = 1.0f);

	/**
	 * Resolve one player attack's configured hit feedback from its final damage.
	 * Returns true only when this request reached the camera shake runtime.
	 */
	static bool TriggerAttackHit(UWorld* InWorld, const FAttackHitShakeBinding& Binding, float FinalDamage);

	/** 订阅玩家受直接性攻击事件（模块启动时调用一次；重复调用安全） */
	static void EnsureSubscribed();

private:
	/** 玩家受直接性攻击事件回调：震动 0.3 秒 */
	static void HandlePlayerTookDirectDamage(const struct FPlayerTookDirectDamageEvent& InEvent);

	/** 引擎 Ticker 回调：每帧生成衰减随机偏移 */
	static bool OnShakeTick(float DeltaTime);

	/** 获取场景中的相机管理器（弱引用缓存，避免每帧查找） */
	static ACameraManager* GetCameraManager(UWorld* World);

	/** Finds a reusable attack-hit preset in Camera Shake project settings. */
	static const FCameraShakePreset* FindAttackHitPreset(FName PresetId);

	/** Applies damage fallback then the project-wide maximum scale clamp. */
	static float ResolveAttackHitScale(const FAttackHitShakeBinding& Binding, const FCameraShakePreset& Preset, float FinalDamage);

	/** Applies the project-wide maximum scale clamp to every shake path. */
	static float ClampGlobalScale(float Scale);

	/** 是否正在震动中 */
	static bool bIsShaking;

	/** 是否已订阅玩家受击事件 */
	static bool bSubscribed;

	/** 震动 Ticker 句柄 */
	static FTSTicker::FDelegateHandle ShakeTickerHandle;

	/** 相机管理器弱引用 */
	static TWeakObjectPtr<ACameraManager> CachedCamera;

	/** 震动所在世界（弱引用，避免 PIE 下 GWorld 指向编辑器世界） */
	static TWeakObjectPtr<UWorld> ShakeWorld;

	/** 震动剩余时间（秒） */
	static float ShakeRemaining;

	/** 震动强度（偏移幅度，单位） */
	static float ShakeIntensity;

	/** Real-time timestamp of the last outgoing player-attack shake. */
	static double LastPlayerAttackShakeTime;
};

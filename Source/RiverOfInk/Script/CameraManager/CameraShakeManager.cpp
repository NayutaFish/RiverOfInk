// Fill out your copyright notice in the Description page of Project Settings.

#include "CameraManager/CameraShakeManager.h"
#include "CameraManager/CameraManager.h"
#include "RiverOfInk.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"

/** 玩家受击震动的默认时长（秒） */
static constexpr float PlayerHitShakeDuration = 0.3f;

/** 玩家受击震动的默认强度 */
static constexpr float PlayerHitShakeIntensity = 75.0f;

bool FCameraShakeManager::bIsShaking = false;
bool FCameraShakeManager::bSubscribed = false;
FTSTicker::FDelegateHandle FCameraShakeManager::ShakeTickerHandle;
TWeakObjectPtr<ACameraManager> FCameraShakeManager::CachedCamera;
TWeakObjectPtr<UWorld> FCameraShakeManager::ShakeWorld;
float FCameraShakeManager::ShakeRemaining = 0.0f;
float FCameraShakeManager::ShakeIntensity = 0.0f;

void FCameraShakeManager::EnsureSubscribed()
{
	if (bSubscribed)
	{
		return;
	}

	bSubscribed = true;
	FEventBus::Subscribe<FPlayerTookDirectDamageEvent>([](const FPlayerTookDirectDamageEvent& InEvent)
	{
		HandlePlayerTookDirectDamage(InEvent);
	});
	UE_LOG(LogRiverOfInk, Log, TEXT("CameraShake: Subscribed to player direct damage events."));
}

void FCameraShakeManager::HandlePlayerTookDirectDamage(const FPlayerTookDirectDamageEvent& InEvent)
{
	UE_LOG(LogRiverOfInk, Log, TEXT("CameraShake: Player direct damage event received."));

	// 世界上下文：优先受击信息里的攻击者，兜底玩家 Pawn
	UWorld* World = nullptr;
	if (InEvent.TakeDamageInfo.Attacker)
	{
		World = InEvent.TakeDamageInfo.Attacker->GetWorld();
	}
	if (!World)
	{
		World = GWorld;
	}

	Trigger(World, PlayerHitShakeDuration, PlayerHitShakeIntensity);
}

void FCameraShakeManager::Trigger(UWorld* InWorld, float Duration, float Intensity)
{
	if (!InWorld)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("CameraShake: World is null, skipped."));
		return;
	}

	if (Duration <= 0.0f)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("CameraShake: Duration must be > 0, skipped."));
		return;
	}

	// 重置相机引用（新世界或新一局时重新查找）
	CachedCamera.Reset();
	ShakeWorld = InWorld;

	ShakeRemaining = Duration;
	ShakeIntensity = Intensity;
	bIsShaking = true;

	// 引擎核心 Ticker：真实帧时间，不受世界时间膨胀（顿帧）影响
	if (!ShakeTickerHandle.IsValid())
	{
		ShakeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&FCameraShakeManager::OnShakeTick));
	}

	UE_LOG(LogRiverOfInk, Log, TEXT("CameraShake: Shaking for %.2f s, intensity %.1f."), Duration, Intensity);
}

bool FCameraShakeManager::OnShakeTick(float DeltaTime)
{
	ShakeRemaining -= DeltaTime;

	// 使用 Trigger 时保存的世界（PIE 下 GWorld 指向编辑器世界，不能依赖）
	UWorld* World = ShakeWorld.Get();
	if (!World)
	{
		World = GWorld;
	}

	if (ShakeRemaining <= 0.0f || !World)
	{
		// 结束：偏移归零
		if (ACameraManager* Camera = GetCameraManager(World))
		{
			Camera->CurrentShakeOffset = FVector::ZeroVector;
		}
		bIsShaking = false;
		CachedCamera.Reset();
		ShakeWorld.Reset();
		ShakeTickerHandle.Reset();
		return false; // 移除 Ticker
	}

	// 强度随时间线性衰减
	const float CurrentIntensity = ShakeIntensity * FMath::Max(ShakeRemaining, 0.0f) / (ShakeRemaining + DeltaTime * 10.0f);

	// 随机偏移（X/Y 平面为主，Z 轻微）
	FVector RandomOffset(
		FMath::FRandRange(-1.0f, 1.0f) * CurrentIntensity,
		FMath::FRandRange(-1.0f, 1.0f) * CurrentIntensity,
		FMath::FRandRange(-0.3f, 0.3f) * CurrentIntensity);

	ACameraManager* Camera = GetCameraManager(World);
	if (Camera)
	{
		Camera->CurrentShakeOffset = RandomOffset;
		UE_LOG(LogRiverOfInk, Verbose, TEXT("CameraShake: Offset=%s Intensity=%.1f"), *RandomOffset.ToString(), CurrentIntensity);
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("CameraShake: CameraManager not found in world, no shake applied."));
	}

	return true; // 继续震动
}

ACameraManager* FCameraShakeManager::GetCameraManager(UWorld* World)
{
	if (ACameraManager* Cached = CachedCamera.Get())
	{
		if (IsValid(Cached))
		{
			return Cached;
		}
	}

	if (!World)
	{
		return nullptr;
	}

	// 遍历世界查找相机管理器（GameMode 动态 Spawn 生成，非场景摆放，
	// 因此不能使用 GetActorOfClass，需遍历 TActorIterator）
	ACameraManager* Found = nullptr;
	for (TActorIterator<ACameraManager> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Found = *It;
			break;
		}
	}

	if (Found)
	{
		CachedCamera = Found;
		UE_LOG(LogRiverOfInk, Log, TEXT("CameraShake: CameraManager found: %s"), *Found->GetName());
	}
	return Found;
}

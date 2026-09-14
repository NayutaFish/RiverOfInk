// Fill out your copyright notice in the Description page of Project Settings.

#include "CameraManager/CameraShakeManager.h"
#include "CameraManager/CameraManager.h"
#include "CameraManager/CameraShakeSettings.h"
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
double FCameraShakeManager::LastPlayerAttackShakeTime = -1.0e30;

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

void FCameraShakeManager::Trigger(UWorld* InWorld, float Duration, float Intensity, float Scale)
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

	const float ClampedScale = ClampGlobalScale(Scale);
	const float ScaledIntensity = FMath::Max(0.0f, Intensity) * ClampedScale;
	if (ScaledIntensity <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 重置相机引用（新世界或新一局时重新查找）
	CachedCamera.Reset();
	ShakeWorld = InWorld;

	ShakeRemaining = Duration;
	ShakeIntensity = ScaledIntensity;
	bIsShaking = true;

	// 引擎核心 Ticker：真实帧时间，不受世界时间膨胀（顿帧）影响
	if (!ShakeTickerHandle.IsValid())
	{
		ShakeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&FCameraShakeManager::OnShakeTick));
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("CameraShake: Shaking for %.2f s, intensity %.1f, scale %.2f."),
		Duration,
		ShakeIntensity,
		ClampedScale);
}

bool FCameraShakeManager::TriggerAttackHit(
	UWorld* InWorld,
	const FAttackHitShakeBinding& Binding,
	float FinalDamage)
{
	if (!InWorld || !Binding.IsEnabled() || FinalDamage <= 0.0f)
	{
		return false;
	}

	const FCameraShakePreset* Preset = FindAttackHitPreset(Binding.PresetId);
	if (!Preset)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("CameraShake: Attack preset '%s' is missing; hit feedback skipped."),
			*Binding.PresetId.ToString());
		return false;
	}

	const UCameraShakeSettings* Settings = GetDefault<UCameraShakeSettings>();
	const double CurrentTime = FPlatformTime::Seconds();
	const double GlobalMinimumInterval = Settings
		? static_cast<double>(FMath::Max(0.0f, Settings->PlayerAttackGlobalMinimumInterval))
		: 0.0;
	if (CurrentTime - LastPlayerAttackShakeTime < GlobalMinimumInterval)
	{
		return false;
	}

	const float Scale = ResolveAttackHitScale(Binding, *Preset, FinalDamage);
	if (Scale <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	Trigger(InWorld, Preset->Duration, Preset->BaseIntensity, Scale);
	LastPlayerAttackShakeTime = CurrentTime;
	return true;
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

	ACameraManager* Camera = GetCameraManager(World);
	if (Camera && Camera->IsExitGuideActive())
	{
		// Combat shake is suppressed while the route-guide camera owns the view.
		Camera->CurrentShakeOffset = FVector::ZeroVector;
		return true;
	}
	// 强度随时间线性衰减
	const float CurrentIntensity = ShakeIntensity * FMath::Max(ShakeRemaining, 0.0f) / (ShakeRemaining + DeltaTime * 10.0f);

	// 随机偏移（X/Y 平面为主，Z 轻微）
	FVector RandomOffset(
		FMath::FRandRange(-1.0f, 1.0f) * CurrentIntensity,
		FMath::FRandRange(-1.0f, 1.0f) * CurrentIntensity,
		FMath::FRandRange(-0.3f, 0.3f) * CurrentIntensity);

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

const FCameraShakePreset* FCameraShakeManager::FindAttackHitPreset(FName PresetId)
{
	if (PresetId == NAME_None)
	{
		return nullptr;
	}

	const UCameraShakeSettings* Settings = GetDefault<UCameraShakeSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	return Settings->AttackHitPresets.FindByPredicate(
		[PresetId](const FCameraShakePreset& Candidate)
		{
			return Candidate.PresetId == PresetId;
		});
}

float FCameraShakeManager::ResolveAttackHitScale(
	const FAttackHitShakeBinding& Binding,
	const FCameraShakePreset& Preset,
	float FinalDamage)
{
	float Scale = 0.0f;
	if (Binding.ScaleMode == EAttackHitShakeScaleMode::Fixed)
	{
		Scale = Binding.FixedScale;
	}
	else if (Binding.ScaleMode == EAttackHitShakeScaleMode::DamageFallback)
	{
		const FDamageShakeFallbackScale& Rule = Preset.DamageFallback;
		const float DamageAtMax = FMath::Max(KINDA_SMALL_NUMBER, Rule.DamageAtMaxScale);
		const float DamageAlpha = FMath::Clamp(FinalDamage / DamageAtMax, 0.0f, 1.0f);
		const float CurvedAlpha = FMath::Pow(DamageAlpha, FMath::Max(KINDA_SMALL_NUMBER, Rule.CurveExponent));
		Scale = FMath::Lerp(Rule.MinScale, Rule.MaxScale, CurvedAlpha);
	}

	return ClampGlobalScale(Scale);
}

float FCameraShakeManager::ClampGlobalScale(float Scale)
{
	const UCameraShakeSettings* Settings = GetDefault<UCameraShakeSettings>();
	const float GlobalMaxScale = Settings
		? FMath::Max(0.0f, Settings->GlobalMaxScale)
		: 1.0f;
	return FMath::Clamp(Scale, 0.0f, GlobalMaxScale);
}

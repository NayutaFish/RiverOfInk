// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CameraShakeTypes.generated.h"

/** Selects how one attack derives its camera-shake scale. */
UENUM(BlueprintType)
enum class EAttackHitShakeScaleMode : uint8
{
	/** This attack does not request a camera shake. */
	Disabled UMETA(DisplayName = "Disabled"),

	/** Use FixedScale, then apply the global maximum scale clamp. */
	Fixed UMETA(DisplayName = "Fixed"),

	/** Convert final resolved damage with the selected preset's fallback curve. */
	DamageFallback UMETA(DisplayName = "Damage Fallback")
};

/** Damage-to-scale rule owned by a reusable camera-shake preset. */
USTRUCT(BlueprintType)
struct FDamageShakeFallbackScale
{
	GENERATED_BODY()

	/** Lowest scale produced by a valid hit with positive final damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Fallback", meta = (ClampMin = "0.0"))
	float MinScale = 0.20f;

	/** Highest scale this preset may produce before the global clamp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Fallback", meta = (ClampMin = "0.0"))
	float MaxScale = 1.00f;

	/** Final resolved damage that reaches MaxScale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Fallback", meta = (ClampMin = "0.01"))
	float DamageAtMaxScale = 100.0f;

	/** Values greater than one keep low-damage hits deliberately subtle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Fallback", meta = (ClampMin = "0.01"))
	float CurveExponent = 1.35f;
};

/** Reusable temporal and amplitude definition for one family of hit feedback. */
USTRUCT(BlueprintType)
struct FCameraShakePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FName PresetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0.01", Units = "s"))
	float Duration = 0.08f;

	/** Base world-space camera offset before an attack-specific scale is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0.0"))
	float BaseIntensity = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FDamageShakeFallbackScale DamageFallback;
};

/** Per-attack binding. Disabled is the safe default for every existing attack asset. */
USTRUCT(BlueprintType)
struct FAttackHitShakeBinding
{
	GENERATED_BODY()

	/** Matches a preset in Camera Shake project settings. None means no shake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Feedback")
	FName PresetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Feedback")
	EAttackHitShakeScaleMode ScaleMode = EAttackHitShakeScaleMode::Disabled;

	/** Used only for Fixed scale mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Feedback", meta = (EditCondition = "ScaleMode == EAttackHitShakeScaleMode::Fixed", ClampMin = "0.0"))
	float FixedScale = 1.0f;

	/** One multi-target attack area normally produces one shake, not one shake per target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Feedback")
	bool bOnlyTriggerOncePerAttackArea = true;

	/** Per attack-area throttle used only when multiple triggers are allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Feedback", meta = (ClampMin = "0.0", Units = "s"))
	float MinimumInterval = 0.0f;

	bool IsEnabled() const
	{
		return PresetId != NAME_None && ScaleMode != EAttackHitShakeScaleMode::Disabled;
	}
};

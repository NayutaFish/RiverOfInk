// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraManager/CameraShakeSettings.h"

namespace
{
	FCameraShakePreset MakePreset(
		const FName Id,
		const float Duration,
		const float Intensity,
		const float MinScale,
		const float MaxScale,
		const float DamageAtMaxScale,
		const float CurveExponent)
	{
		FCameraShakePreset Preset;
		Preset.PresetId = Id;
		Preset.Duration = Duration;
		Preset.BaseIntensity = Intensity;
		Preset.DamageFallback.MinScale = MinScale;
		Preset.DamageFallback.MaxScale = MaxScale;
		Preset.DamageFallback.DamageAtMaxScale = DamageAtMaxScale;
		Preset.DamageFallback.CurveExponent = CurveExponent;
		return Preset;
	}
}

UCameraShakeSettings::UCameraShakeSettings()
{
	AttackHitPresets = {
		MakePreset(TEXT("Player.LightHit"), 0.075f, 24.0f, 0.20f, 0.55f, 40.0f, 1.35f),
		MakePreset(TEXT("Player.HeavyHit"), 0.110f, 42.0f, 0.35f, 0.90f, 70.0f, 1.25f),
		MakePreset(TEXT("Player.SkillHit"), 0.090f, 32.0f, 0.25f, 0.70f, 60.0f, 1.30f),
		MakePreset(TEXT("Player.Explosion"), 0.150f, 58.0f, 0.45f, 1.00f, 100.0f, 1.20f)
	};
}

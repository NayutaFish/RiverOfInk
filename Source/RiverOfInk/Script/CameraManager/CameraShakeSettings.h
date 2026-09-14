// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CameraManager/CameraShakeTypes.h"
#include "CameraShakeSettings.generated.h"

/** Project-wide tuning for every camera shake emitted by combat feedback. */
UCLASS(config = Game, defaultconfig, BlueprintType, meta = (DisplayName = "Camera Shake"))
class RIVEROFINK_API UCameraShakeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCameraShakeSettings();

	/**
	 * Final upper bound for all camera-shake scale requests, including fixed
	 * attack values, damage fallback values, and legacy direct-hit shakes.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Global", meta = (ClampMin = "0.0"))
	float GlobalMaxScale = 1.0f;

	/** Prevents several separate projectiles from repeatedly restarting the shake in the same instant. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Global", meta = (ClampMin = "0.0", Units = "s"))
	float PlayerAttackGlobalMinimumInterval = 0.06f;

	/** Reusable attack-hit feedback presets. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Attack Hit Presets", meta = (TitleProperty = "PresetId"))
	TArray<FCameraShakePreset> AttackHitPresets;
};

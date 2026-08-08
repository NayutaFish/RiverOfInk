// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CombatDamageSettings.generated.h"

/**
 * Test-only damage formula selector.
 *
 * The formula lives in CombatDamageCalculator rather than in each health
 * owner. Keeping the selector in project settings lets the test formula be
 * replaced without changing every damage source.
 */
UENUM(BlueprintType)
enum class EDamageFormulaMode : uint8
{
	SubtractDefenseRoundNearest UMETA(DisplayName = "Damage - Defense (Nearest Integer)")
};

/** Project-wide settings for the single damage model. */
UCLASS(config = Game, defaultconfig, BlueprintType)
class RIVEROFINK_API UCombatDamageSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Formula")
	EDamageFormulaMode Formula = EDamageFormulaMode::SubtractDefenseRoundNearest;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Formula", meta = (ClampMin = "1"))
	int32 MinimumDamage = 1;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Formula", meta = (ClampMin = "0.0"))
	float DefenseMultiplier = 1.0f;
};

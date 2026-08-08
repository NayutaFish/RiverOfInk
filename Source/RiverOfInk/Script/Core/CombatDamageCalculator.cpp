// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CombatDamageCalculator.h"

#include "Core/CombatDamageSettings.h"

namespace RiverOfInkDamage
{
	int32 CalculateFinalDamage(float Damage, float Defense)
	{
		const UCombatDamageSettings* Settings = GetDefault<UCombatDamageSettings>();
		const int32 MinimumDamage = FMath::Max(1, Settings ? Settings->MinimumDamage : 1);
		const float SafeDamage = FMath::IsFinite(Damage) ? FMath::Max(0.0f, Damage) : 0.0f;
		const float SafeDefense = FMath::IsFinite(Defense) ? FMath::Max(0.0f, Defense) : 0.0f;
		const float DefenseMultiplier = Settings
			? FMath::Max(0.0f, Settings->DefenseMultiplier)
			: 1.0f;

		float AdjustedDamage = SafeDamage;
		if (!Settings || Settings->Formula == EDamageFormulaMode::SubtractDefenseRoundNearest)
		{
			AdjustedDamage = SafeDamage - SafeDefense * DefenseMultiplier;
		}

		// Equivalent to the requested test formula for finite, non-negative
		// inputs, while keeping the minimum-damage rule explicit.
		const int32 RoundedDamage = FMath::FloorToInt(AdjustedDamage + 0.5f);
		return FMath::Max(MinimumDamage, RoundedDamage);
	}

	int32 ResolveLegacyDefense(int32 Defense, int32 PhysicalResistance, int32 MagicResistance)
	{
		if (Defense > 0)
		{
			return Defense;
		}

		// Old snapshots and Blueprint instances may still only have one of the
		// two resistance fields populated. Use the stronger value once and avoid
		// accidentally stacking both legacy systems during migration.
		return FMath::Max(0, FMath::Max(PhysicalResistance, MagicResistance));
	}
}

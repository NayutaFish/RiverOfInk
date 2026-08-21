// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CombatDamageCalculator.h"

#include "Common/CombatEffectComponent.h"
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

	FDamageResult ResolveDamage(
		const FDamageContext& Context,
		const UCombatEffectComponent* SourceEffects,
		const UCombatEffectComponent* TargetEffects,
		float Defense)
	{
		FDamageResult Result;
		Result.Context = Context;

		if (!FMath::IsFinite(Context.BaseDamage) || Context.BaseDamage <= KINDA_SMALL_NUMBER)
		{
			Result.bNoDamage = true;
			return Result;
		}

		if (TargetEffects
			&& !Context.bIgnoreInvulnerability
			&& TargetEffects->IsInvulnerable())
		{
			Result.bBlockedByInvulnerability = true;
			Result.bNoDamage = true;
			return Result;
		}

		float ModifiedDamage = Context.BaseDamage;
		if (SourceEffects)
		{
			ModifiedDamage = SourceEffects->ModifyOutgoingDamage(ModifiedDamage, Context.DamageTags);
		}
		if (TargetEffects)
		{
			ModifiedDamage = TargetEffects->ModifyIncomingDamage(ModifiedDamage, Context.DamageTags);
		}

		Result.ModifiedDamage = FMath::IsFinite(ModifiedDamage)
			? FMath::Max(0.0f, ModifiedDamage)
			: 0.0f;
		if (Result.ModifiedDamage <= KINDA_SMALL_NUMBER)
		{
			Result.bNoDamage = true;
			return Result;
		}

		Result.FinalDamage = CalculateFinalDamage(Result.ModifiedDamage, Defense);
		Result.bDamageApplied = Result.FinalDamage > 0;
		Result.bNoDamage = !Result.bDamageApplied;
		return Result;
	}
}

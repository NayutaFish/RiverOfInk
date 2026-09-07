// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CombatDamageCalculator.h"

#include "Common/CombatEffectComponent.h"
#include "Core/CombatDamageSettings.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"

namespace RiverOfInkDamage
{
	float ResolveBaseAttackPower(const AActor* SourceActor)
	{
		if (const APlayerCharacter* Player = Cast<APlayerCharacter>(SourceActor))
		{
			return Player->GetBaseAttackPower();
		}

		if (const AEnemyBase* Enemy = Cast<AEnemyBase>(SourceActor))
		{
			return Enemy->GetBaseAttackPower();
		}

		return 0.0f;
	}

	float ResolveDefaultBaseAttackPower(const AActor* SourceActor)
	{
		if (const APlayerCharacter* Player = Cast<APlayerCharacter>(SourceActor))
		{
			const APlayerCharacter* DefaultPlayer =
				Player->GetClass()->GetDefaultObject<APlayerCharacter>();
			return DefaultPlayer ? DefaultPlayer->GetBaseAttackPower() : 0.0f;
		}

		if (const AEnemyBase* Enemy = Cast<AEnemyBase>(SourceActor))
		{
			const AEnemyBase* DefaultEnemy =
				Enemy->GetClass()->GetDefaultObject<AEnemyBase>();
			return DefaultEnemy ? DefaultEnemy->GetBaseAttackPower() : 0.0f;
		}

		return 0.0f;
	}

	float ResolveAttackDamage(
		const AActor* SourceActor,
		const FAttackDamageProfile& DamageProfile,
		float LegacyDamageValue)
	{
		const float SafeLegacyDamage = FMath::IsFinite(LegacyDamageValue)
			? FMath::Max(0.0f, LegacyDamageValue)
			: 0.0f;
		if (!DamageProfile.bUseBaseAttackPower)
		{
			return SafeLegacyDamage;
		}

		const float CurrentBaseAttackPower = ResolveBaseAttackPower(SourceActor);
		if (!FMath::IsFinite(CurrentBaseAttackPower)
			|| CurrentBaseAttackPower <= KINDA_SMALL_NUMBER)
		{
			return SafeLegacyDamage;
		}

		float AttackMultiplier = DamageProfile.AttackMultiplier;
		if (DamageProfile.bDeriveMultiplierFromLegacyDamage)
		{
			const float DefaultBaseAttackPower = ResolveDefaultBaseAttackPower(SourceActor);
			if (!FMath::IsFinite(DefaultBaseAttackPower)
				|| DefaultBaseAttackPower <= KINDA_SMALL_NUMBER)
			{
				return SafeLegacyDamage;
			}

			AttackMultiplier = SafeLegacyDamage / DefaultBaseAttackPower;
		}

		if (!FMath::IsFinite(AttackMultiplier))
		{
			return SafeLegacyDamage;
		}

		return FMath::Max(0.0f, CurrentBaseAttackPower * AttackMultiplier);
	}

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

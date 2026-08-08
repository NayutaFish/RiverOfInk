// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace RiverOfInkDamage
{
	/**
	 * Calculates final damage for the current single-type test model.
	 *
	 * Formula selected by UCombatDamageSettings:
	 * int((float)Damage - (float)Defense + 0.5f), clamped to MinimumDamage.
	 */
	RIVEROFINK_API int32 CalculateFinalDamage(float Damage, float Defense);

	/** Resolve a legacy pair of resistance fields into the new single defense. */
	RIVEROFINK_API int32 ResolveLegacyDefense(int32 Defense, int32 PhysicalResistance, int32 MagicResistance);
}

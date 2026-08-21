// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/** Native tags shared by the effect runtime and future damage/projectile processors. */
namespace RiverOfInkCombatEffectTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Buff_Invulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Buff_DamageUp);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Debuff_Slow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Debuff_Vulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Debuff_HomingMark);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Proc_NextHitBonusDamage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Buff_ControlResist);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attribute_Damage_OutgoingMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attribute_Damage_IncomingMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attribute_Movement_SpeedMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attribute_Control_ResistMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Projectile_Homing);
}

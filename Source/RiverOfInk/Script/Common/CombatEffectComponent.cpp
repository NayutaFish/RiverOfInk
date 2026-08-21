// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/CombatEffectComponent.h"

#include "Common/CombatEffectTags.h"
#include "Engine/World.h"

UCombatEffectComponent::UCombatEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetComponentTickEnabled(false);
}

void UCombatEffectComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateComponentTickState();
}

void UCombatEffectComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (int32 Index = ActiveEffects.Num() - 1; Index >= 0; --Index)
	{
		FActiveCombatEffect& ActiveEffect = ActiveEffects[Index];
		if (!ActiveEffect.HasFiniteDuration())
		{
			continue;
		}

		ActiveEffect.RemainingTime = FMath::Max(0.0f, ActiveEffect.RemainingTime - DeltaTime);
		if (ActiveEffect.RemainingTime <= 0.0f)
		{
			RemoveEffect(ActiveEffect.Handle);
		}
	}

	UpdateComponentTickState();
}

FCombatEffectHandle UCombatEffectComponent::ApplyEffect(const FCombatEffectSpec& Spec)
{
	if (!IsValidSpec(Spec))
	{
		UE_LOG(LogTemp, Warning, TEXT("CombatEffectComponent rejected an invalid effect spec on %s."), *GetNameSafe(GetOwner()));
		return FCombatEffectHandle();
	}

	const int32 ExistingIndex = FindMatchingEffectIndex(Spec);
	if (ExistingIndex == INDEX_NONE)
	{
		const FCombatEffectHandle NewHandle = MakeHandle();
		ActiveEffects.Add(MakeActiveEffect(Spec, NewHandle));
		OnEffectAdded.Broadcast(ActiveEffects.Last());
		UpdateComponentTickState();
		return NewHandle;
	}

	FActiveCombatEffect& ExistingEffect = ActiveEffects[ExistingIndex];
	switch (Spec.StackPolicy)
	{
	case ECombatEffectStackPolicy::Ignore:
		return ExistingEffect.Handle;

	case ECombatEffectStackPolicy::Replace:
		ExistingEffect = MakeActiveEffect(Spec, ExistingEffect.Handle);
		OnEffectChanged.Broadcast(ExistingEffect);
		UpdateComponentTickState();
		return ExistingEffect.Handle;

	case ECombatEffectStackPolicy::RefreshDuration:
		ExistingEffect.Spec = Spec;
		if (UsesDuration(Spec.DurationPolicy))
		{
			ExistingEffect.RemainingTime = FMath::Max(0.0f, Spec.Duration);
		}
		OnEffectChanged.Broadcast(ExistingEffect);
		UpdateComponentTickState();
		return ExistingEffect.Handle;

	case ECombatEffectStackPolicy::AddStack:
	case ECombatEffectStackPolicy::AddStackAndRefresh:
		ExistingEffect.CurrentStackCount = FMath::Min(
			ExistingEffect.CurrentStackCount + FMath::Max(1, Spec.StackCount),
			FMath::Max(1, Spec.MaxStacks));
		ExistingEffect.Spec.Magnitude = Spec.Magnitude;
		if (UsesCharges(Spec.DurationPolicy) && ExistingEffect.HasCharges())
		{
			ExistingEffect.RemainingCharges += FMath::Max(0, Spec.Charges);
			const int32 MaxCharges = Spec.MaxCharges > 0
				? Spec.MaxCharges
				: ExistingEffect.Spec.MaxCharges;
			if (MaxCharges > 0)
			{
				ExistingEffect.RemainingCharges = FMath::Min(
					ExistingEffect.RemainingCharges,
					MaxCharges);
			}
		}
		ExistingEffect.Spec.MaxCharges = Spec.MaxCharges > 0
			? Spec.MaxCharges
			: ExistingEffect.Spec.MaxCharges;
		if (Spec.StackPolicy == ECombatEffectStackPolicy::AddStackAndRefresh && UsesDuration(Spec.DurationPolicy))
		{
			ExistingEffect.RemainingTime = FMath::Max(0.0f, Spec.Duration);
		}
		OnEffectChanged.Broadcast(ExistingEffect);
		UpdateComponentTickState();
		return ExistingEffect.Handle;

	case ECombatEffectStackPolicy::KeepStrongest:
		if (Spec.Magnitude > ExistingEffect.Spec.Magnitude)
		{
			const FCombatEffectHandle ExistingHandle = ExistingEffect.Handle;
			ExistingEffect = MakeActiveEffect(Spec, ExistingHandle);
			OnEffectChanged.Broadcast(ExistingEffect);
			UpdateComponentTickState();
		}
		return ExistingEffect.Handle;
	}

	return ExistingEffect.Handle;
}

bool UCombatEffectComponent::RemoveEffect(FCombatEffectHandle Handle)
{
	const int32 EffectIndex = FindEffectIndex(Handle);
	if (EffectIndex == INDEX_NONE)
	{
		return false;
	}

	const FActiveCombatEffect RemovedEffect = ActiveEffects[EffectIndex];
	ActiveEffects.RemoveAt(EffectIndex);
	OnEffectRemoved.Broadcast(RemovedEffect);
	UpdateComponentTickState();
	return true;
}

bool UCombatEffectComponent::RefreshEffect(FCombatEffectHandle Handle, float NewDuration)
{
	const int32 EffectIndex = FindEffectIndex(Handle);
	if (EffectIndex == INDEX_NONE || !ActiveEffects[EffectIndex].HasFiniteDuration() || NewDuration <= 0.0f)
	{
		return false;
	}

	FActiveCombatEffect& ActiveEffect = ActiveEffects[EffectIndex];
	ActiveEffect.RemainingTime = NewDuration;
	OnEffectChanged.Broadcast(ActiveEffect);
	UpdateComponentTickState();
	return true;
}

bool UCombatEffectComponent::ConsumeEffectCharge(FCombatEffectHandle Handle, int32 ChargeCount)
{
	const int32 EffectIndex = FindEffectIndex(Handle);
	if (EffectIndex == INDEX_NONE || ChargeCount <= 0)
	{
		return false;
	}

	FActiveCombatEffect& ActiveEffect = ActiveEffects[EffectIndex];
	if (!ActiveEffect.HasCharges() || ActiveEffect.RemainingCharges < ChargeCount)
	{
		return false;
	}

	ActiveEffect.RemainingCharges -= ChargeCount;
	if (ActiveEffect.RemainingCharges <= 0)
	{
		return RemoveEffect(Handle);
	}

	OnEffectChanged.Broadcast(ActiveEffect);
	return true;
}

bool UCombatEffectComponent::HasEffect(FGameplayTag EffectTag) const
{
	if (!EffectTag.IsValid())
	{
		return false;
	}

	return ActiveEffects.ContainsByPredicate([EffectTag](const FActiveCombatEffect& ActiveEffect)
	{
		return EffectCarriesTag(ActiveEffect, EffectTag);
	});
}

int32 UCombatEffectComponent::GetEffectStackCount(FGameplayTag EffectTag) const
{
	if (!EffectTag.IsValid())
	{
		return 0;
	}

	int32 TotalStacks = 0;
	for (const FActiveCombatEffect& ActiveEffect : ActiveEffects)
	{
		if (EffectCarriesTag(ActiveEffect, EffectTag))
		{
			TotalStacks += ActiveEffect.CurrentStackCount;
		}
	}
	return TotalStacks;
}

bool UCombatEffectComponent::TryGetEffect(FGameplayTag EffectTag, FActiveCombatEffect& OutEffect) const
{
	if (!EffectTag.IsValid())
	{
		return false;
	}

	for (const FActiveCombatEffect& ActiveEffect : ActiveEffects)
	{
		if (EffectCarriesTag(ActiveEffect, EffectTag))
		{
			OutEffect = ActiveEffect;
			return true;
		}
	}
	return false;
}

bool UCombatEffectComponent::TryGetEffectFromSource(
	FGameplayTag EffectTag,
	AActor* SourceActor,
	FActiveCombatEffect& OutEffect) const
{
	const int32 EffectIndex = FindEffectIndex(EffectTag, SourceActor);
	if (EffectIndex == INDEX_NONE)
	{
		return false;
	}

	OutEffect = ActiveEffects[EffectIndex];
	return true;
}

bool UCombatEffectComponent::ConsumeEffectChargeFromSource(
	FGameplayTag EffectTag,
	AActor* SourceActor,
	int32 ChargeCount)
{
	const int32 EffectIndex = FindEffectIndex(EffectTag, SourceActor);
	return EffectIndex != INDEX_NONE
		&& ConsumeEffectCharge(ActiveEffects[EffectIndex].Handle, ChargeCount);
}

bool UCombatEffectComponent::IsInvulnerable() const
{
	return HasEffect(RiverOfInkCombatEffectTags::Effect_Buff_Invulnerable);
}

float UCombatEffectComponent::GetOutgoingDamageMultiplier() const
{
	const float TaggedMultiplier = 1.0f
		+ GetTagMagnitude(RiverOfInkCombatEffectTags::Effect_Buff_DamageUp);
	return FMath::Clamp(
		TaggedMultiplier * GetAttributeMultiplier(RiverOfInkCombatEffectTags::Attribute_Damage_OutgoingMultiplier),
		0.0f,
		100.0f);
}

float UCombatEffectComponent::GetIncomingDamageMultiplier() const
{
	const float TaggedMultiplier = 1.0f
		+ GetTagMagnitude(RiverOfInkCombatEffectTags::Effect_Debuff_Vulnerable);
	return FMath::Clamp(
		TaggedMultiplier * GetAttributeMultiplier(RiverOfInkCombatEffectTags::Attribute_Damage_IncomingMultiplier),
		0.0f,
		100.0f);
}

float UCombatEffectComponent::ModifyOutgoingDamage(
	float BaseDamage,
	const FGameplayTagContainer& DamageTags) const
{
	const float SafeDamage = FMath::IsFinite(BaseDamage) ? FMath::Max(0.0f, BaseDamage) : 0.0f;
	const float TaggedMultiplier = 1.0f
		+ GetTagMagnitudeForDamage(RiverOfInkCombatEffectTags::Effect_Buff_DamageUp, DamageTags);
	return SafeDamage * FMath::Clamp(
		TaggedMultiplier * GetAttributeMultiplier(RiverOfInkCombatEffectTags::Attribute_Damage_OutgoingMultiplier),
		0.0f,
		100.0f);
}

float UCombatEffectComponent::ModifyIncomingDamage(
	float BaseDamage,
	const FGameplayTagContainer& DamageTags) const
{
	const float SafeDamage = FMath::IsFinite(BaseDamage) ? FMath::Max(0.0f, BaseDamage) : 0.0f;
	const float TaggedMultiplier = 1.0f
		+ GetTagMagnitudeForDamage(RiverOfInkCombatEffectTags::Effect_Debuff_Vulnerable, DamageTags);
	return SafeDamage * FMath::Clamp(
		TaggedMultiplier * GetAttributeMultiplier(RiverOfInkCombatEffectTags::Attribute_Damage_IncomingMultiplier),
		0.0f,
		100.0f);
}

float UCombatEffectComponent::GetMoveSpeedMultiplier() const
{
	const float SlowStrength = GetTagMagnitude(RiverOfInkCombatEffectTags::Effect_Debuff_Slow);
	const float TaggedMultiplier = 1.0f - SlowStrength;
	return FMath::Clamp(
		TaggedMultiplier * GetAttributeMultiplier(RiverOfInkCombatEffectTags::Attribute_Movement_SpeedMultiplier),
		0.0f,
		1.0f);
}

float UCombatEffectComponent::GetControlResistMultiplier() const
{
	const float ResistStrength = GetTagMagnitude(RiverOfInkCombatEffectTags::Effect_Buff_ControlResist);
	const float TaggedMultiplier = 1.0f - ResistStrength;
	return FMath::Clamp(
		TaggedMultiplier * GetAttributeMultiplier(RiverOfInkCombatEffectTags::Attribute_Control_ResistMultiplier),
		0.0f,
		1.0f);
}

float UCombatEffectComponent::GetModifierValue(FGameplayTag AttributeTag) const
{
	return AttributeTag.IsValid() ? GetAttributeMultiplier(AttributeTag) : 1.0f;
}

bool UCombatEffectComponent::ConsumeNextHitBonusDamage(FTakeDamageInfo& OutBonusDamage)
{
	OutBonusDamage = FTakeDamageInfo();

	const FGameplayTag NextHitTag = RiverOfInkCombatEffectTags::Effect_Proc_NextHitBonusDamage;
	for (const FActiveCombatEffect& ActiveEffect : ActiveEffects)
	{
		if (!EffectCarriesTag(ActiveEffect, NextHitTag))
		{
			continue;
		}

		const FCombatEffectDamagePayload& Payload = ActiveEffect.Spec.DamagePayload;
		OutBonusDamage.Attacker = ActiveEffect.Spec.SourceActor;
		OutBonusDamage.DamageType = Payload.DamageType;
		OutBonusDamage.DamageValue = Payload.DamageValue > KINDA_SMALL_NUMBER
			? Payload.DamageValue
			: ActiveEffect.Spec.Magnitude;
		OutBonusDamage.HardDamageValue = Payload.HardDamageValue;
		OutBonusDamage.bCanCauseDeath = Payload.bCanCauseDeath;
		OutBonusDamage.bIsDirectDamage = Payload.bIsDirectDamage;
		OutBonusDamage.bIgnoreInvincible = Payload.bIgnoreInvulnerability;

		// A stacked proc represents one payload per remaining charge. Keep the
		// handle stable and consume exactly one charge for this hit.
		OutBonusDamage.DamageValue *= FMath::Max(1, ActiveEffect.CurrentStackCount);
		OutBonusDamage.HardDamageValue *= FMath::Max(1, ActiveEffect.CurrentStackCount);
		if (ActiveEffect.HasCharges())
		{
			ConsumeEffectCharge(ActiveEffect.Handle);
		}
		else
		{
			// A manually authored Timed proc is still one-hit by definition.
			RemoveEffect(ActiveEffect.Handle);
		}
		return OutBonusDamage.DamageValue > KINDA_SMALL_NUMBER;
	}

	return false;
}

int32 UCombatEffectComponent::FindEffectIndex(FCombatEffectHandle Handle) const
{
	if (!Handle.IsValid())
	{
		return INDEX_NONE;
	}

	return ActiveEffects.IndexOfByPredicate([Handle](const FActiveCombatEffect& ActiveEffect)
	{
		return ActiveEffect.Handle == Handle;
	});
}

int32 UCombatEffectComponent::FindEffectIndex(FGameplayTag EffectTag, AActor* SourceActor) const
{
	if (!EffectTag.IsValid())
	{
		return INDEX_NONE;
	}

	return ActiveEffects.IndexOfByPredicate(
		[EffectTag, SourceActor](const FActiveCombatEffect& ActiveEffect)
		{
			return EffectCarriesTag(ActiveEffect, EffectTag)
				&& ActiveEffect.Spec.SourceActor == SourceActor;
		});
}

int32 UCombatEffectComponent::FindMatchingEffectIndex(const FCombatEffectSpec& Spec) const
{
	return ActiveEffects.IndexOfByPredicate([&Spec](const FActiveCombatEffect& ActiveEffect)
	{
		return ActiveEffect.Spec.EffectTag == Spec.EffectTag
			&& ActiveEffect.Spec.SourceActor == Spec.SourceActor;
	});
}

bool UCombatEffectComponent::UsesDuration(ECombatEffectDurationPolicy Policy)
{
	return Policy == ECombatEffectDurationPolicy::Timed
		|| Policy == ECombatEffectDurationPolicy::TimedAndCharges;
}

bool UCombatEffectComponent::UsesCharges(ECombatEffectDurationPolicy Policy)
{
	return Policy == ECombatEffectDurationPolicy::Charges
		|| Policy == ECombatEffectDurationPolicy::TimedAndCharges;
}

bool UCombatEffectComponent::IsValidSpec(const FCombatEffectSpec& Spec)
{
	if (!Spec.EffectTag.IsValid() || Spec.MaxStacks < 1 || Spec.StackCount < 1)
	{
		return false;
	}

	if (UsesDuration(Spec.DurationPolicy) && Spec.Duration <= 0.0f)
	{
		return false;
	}

	if (UsesCharges(Spec.DurationPolicy) && Spec.Charges <= 0)
	{
		return false;
	}

	if (UsesCharges(Spec.DurationPolicy)
		&& Spec.MaxCharges > 0
		&& Spec.MaxCharges < Spec.Charges)
	{
		return false;
	}

	return true;
}

bool UCombatEffectComponent::EffectCarriesTag(
	const FActiveCombatEffect& ActiveEffect,
	FGameplayTag EffectTag)
{
	return ActiveEffect.Spec.EffectTag.MatchesTag(EffectTag)
		|| ActiveEffect.Spec.GrantedTags.HasTag(EffectTag);
}

float UCombatEffectComponent::GetTagMagnitude(FGameplayTag EffectTag) const
{
	if (!EffectTag.IsValid())
	{
		return 0.0f;
	}

	float TotalMagnitude = 0.0f;
	for (const FActiveCombatEffect& ActiveEffect : ActiveEffects)
	{
		if (EffectCarriesTag(ActiveEffect, EffectTag))
		{
			TotalMagnitude += ActiveEffect.Spec.Magnitude
				* static_cast<float>(FMath::Max(1, ActiveEffect.CurrentStackCount));
		}
	}
	return TotalMagnitude;
}

float UCombatEffectComponent::GetTagMagnitudeForDamage(
	FGameplayTag EffectTag,
	const FGameplayTagContainer& DamageTags) const
{
	if (!EffectTag.IsValid())
	{
		return 0.0f;
	}

	float TotalMagnitude = 0.0f;
	for (const FActiveCombatEffect& ActiveEffect : ActiveEffects)
	{
		if (!ActiveEffect.Spec.EffectTag.MatchesTag(EffectTag))
		{
			continue;
		}

		// An empty AffectsTags list is global. If the request carries no tags,
		// retain the legacy/global behavior instead of silently disabling the
		// effect while callers migrate to FDamageContext.
		if (!ActiveEffect.Spec.AffectsTags.IsEmpty()
			&& !DamageTags.IsEmpty()
			&& !ActiveEffect.Spec.AffectsTags.HasAny(DamageTags))
		{
			continue;
		}

		TotalMagnitude += ActiveEffect.Spec.Magnitude
			* static_cast<float>(FMath::Max(1, ActiveEffect.CurrentStackCount));
	}
	return TotalMagnitude;
}

float UCombatEffectComponent::GetAttributeMultiplier(FGameplayTag AttributeTag) const
{
	if (!AttributeTag.IsValid())
	{
		return 1.0f;
	}

	float Value = 1.0f;
	for (const FActiveCombatEffect& ActiveEffect : ActiveEffects)
	{
		const int32 StackCount = FMath::Max(1, ActiveEffect.CurrentStackCount);
		for (const FCombatEffectModifier& Modifier : ActiveEffect.Spec.Modifiers)
		{
			if (!Modifier.AttributeTag.IsValid()
				|| !Modifier.AttributeTag.MatchesTag(AttributeTag))
			{
				continue;
			}

			switch (Modifier.Operation)
			{
			case ECombatEffectModifierOperation::Add:
				Value += Modifier.Magnitude * static_cast<float>(StackCount);
				break;
			case ECombatEffectModifierOperation::Multiply:
				Value *= FMath::Pow(FMath::Max(0.0f, Modifier.Magnitude), StackCount);
				break;
			case ECombatEffectModifierOperation::Override:
				Value = Modifier.Magnitude;
				break;
			default:
				break;
			}
		}
	}

	return FMath::Max(0.0f, Value);
}

FActiveCombatEffect UCombatEffectComponent::MakeActiveEffect(
	const FCombatEffectSpec& Spec,
	FCombatEffectHandle Handle)
{
	FActiveCombatEffect ActiveEffect;
	ActiveEffect.Handle = Handle;
	ActiveEffect.Spec = Spec;
	ActiveEffect.RemainingTime = UsesDuration(Spec.DurationPolicy)
		? FMath::Max(0.0f, Spec.Duration)
		: -1.0f;
	ActiveEffect.RemainingCharges = UsesCharges(Spec.DurationPolicy)
		? FMath::Max(0, Spec.Charges)
		: -1;
	ActiveEffect.CurrentStackCount = FMath::Clamp(Spec.StackCount, 1, FMath::Max(1, Spec.MaxStacks));
	return ActiveEffect;
}

FCombatEffectHandle UCombatEffectComponent::MakeHandle()
{
	FCombatEffectHandle Handle;
	Handle.Id = NextHandleId++;
	if (Handle.Id == INDEX_NONE)
	{
		Handle.Id = NextHandleId++;
	}
	return Handle;
}

void UCombatEffectComponent::UpdateComponentTickState()
{
	const bool bNeedsTick = ActiveEffects.ContainsByPredicate([](const FActiveCombatEffect& ActiveEffect)
	{
		return ActiveEffect.HasFiniteDuration();
	});
	SetComponentTickEnabled(bNeedsTick);
}

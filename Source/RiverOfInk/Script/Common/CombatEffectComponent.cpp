// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/CombatEffectComponent.h"

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
		}
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
		return ActiveEffect.Spec.EffectTag == EffectTag;
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
		if (ActiveEffect.Spec.EffectTag == EffectTag)
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
		if (ActiveEffect.Spec.EffectTag == EffectTag)
		{
			OutEffect = ActiveEffect;
			return true;
		}
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

	return true;
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

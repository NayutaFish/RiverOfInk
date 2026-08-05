// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/HealthComponent.h"

#include "RiverOfInk.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeHealth();
}

void UHealthComponent::InitializeHealth()
{
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	CurrentHealth = MaxHealth;
	bIsDead = false;
	BroadcastHealthChanged();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Health component initialized: Owner=%s HP=%.0f/%.0f."),
		*GetNameSafe(GetOwner()),
		CurrentHealth,
		MaxHealth);
}

void UHealthComponent::TakeDamage(const FTakeDamageInfo& InInfo)
{
	if (bIsDead || InInfo.DamageValue <= 0.0f)
	{
		return;
	}

	if (InInfo.bIsDirectDamage)
	{
		OnTakeDirectDamage.Broadcast(InInfo);
	}

	float FinalDamage = InInfo.DamageValue;
	switch (InInfo.DamageType)
	{
	case EDamageType::Physical:
		FinalDamage = FMath::Max(InInfo.DamageValue * 0.05f, InInfo.DamageValue - PhysicalResistance);
		break;
	case EDamageType::Magic:
		FinalDamage = FMath::Max(
			InInfo.DamageValue * 0.05f,
			static_cast<float>(FMath::FloorToInt(InInfo.DamageValue * (1.0f - MagicResistance / 100.0f))));
		break;
	default:
		break;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - FinalDamage);
	UE_LOG(LogRiverOfInk, Log,
		TEXT("Health component damage: Owner=%s Damage=%.1f CurrentHealth=%.1f."),
		*GetNameSafe(GetOwner()),
		FinalDamage,
		CurrentHealth);

	if (CurrentHealth <= 0.0f)
	{
		if (InInfo.bCanCauseDeath)
		{
			Die();
		}
		else
		{
			CurrentHealth = 1.0f;
			BroadcastHealthChanged();
		}
		return;
	}

	BroadcastHealthChanged();
}

void UHealthComponent::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	CurrentHealth = 0.0f;
	BroadcastHealthChanged();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Health component died: Owner=%s."),
		*GetNameSafe(GetOwner()));
	OnDeath.Broadcast(GetOwner());
}

void UHealthComponent::SetRuntimeHealthData(
	float InMaxHealth,
	float InCurrentHealth,
	int32 InPhysicalResistance,
	int32 InMagicResistance)
{
	MaxHealth = FMath::Max(1.0f, InMaxHealth);
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);
	PhysicalResistance = FMath::Max(0, InPhysicalResistance);
	MagicResistance = FMath::Clamp(InMagicResistance, 0, 100);
	BroadcastHealthChanged();
}

void UHealthComponent::CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const
{
	OutRuntimeData.Stats.MaxHealth = FMath::Max(1.0f, MaxHealth);
	OutRuntimeData.Stats.CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, OutRuntimeData.Stats.MaxHealth);
	OutRuntimeData.Stats.PhysicalResistance = FMath::Max(0, PhysicalResistance);
	OutRuntimeData.Stats.MagicResistance = FMath::Clamp(MagicResistance, 0, 100);

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Health runtime data captured: Owner=%s HP=%.0f/%.0f PhysicalResistance=%d MagicResistance=%d."),
		*GetNameSafe(GetOwner()),
		OutRuntimeData.Stats.CurrentHealth,
		OutRuntimeData.Stats.MaxHealth,
		OutRuntimeData.Stats.PhysicalResistance,
		OutRuntimeData.Stats.MagicResistance);
}

void UHealthComponent::ApplyRuntimeData(const FPlayerRuntimeData& InRuntimeData)
{
	SetRuntimeHealthData(
		InRuntimeData.Stats.MaxHealth,
		InRuntimeData.Stats.CurrentHealth,
		InRuntimeData.Stats.PhysicalResistance,
		InRuntimeData.Stats.MagicResistance);

	UE_LOG(LogRiverOfInk, Verbose,
		TEXT("Health runtime data applied: Owner=%s HP=%.0f/%.0f PhysicalResistance=%d MagicResistance=%d."),
		*GetNameSafe(GetOwner()),
		CurrentHealth,
		MaxHealth,
		PhysicalResistance,
		MagicResistance);
}

void UHealthComponent::SetCurrentHealth(float InCurrentHealth)
{
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);
	BroadcastHealthChanged();
}

void UHealthComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

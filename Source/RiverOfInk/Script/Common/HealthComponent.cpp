// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/HealthComponent.h"

#include "Core/CombatDamageCalculator.h"
#include "RiverOfInk.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	NormalizeDefenseFromLegacy();
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

	const int32 FinalDamage = RiverOfInkDamage::CalculateFinalDamage(InInfo.DamageValue, Defense);

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - static_cast<float>(FinalDamage));
	UE_LOG(LogRiverOfInk, Log,
		TEXT("Health component damage: Owner=%s Damage=%d Defense=%d CurrentHealth=%.1f."),
		*GetNameSafe(GetOwner()),
		FinalDamage,
		Defense,
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
	SetRuntimeDefenseData(
		InMaxHealth,
		InCurrentHealth,
		RiverOfInkDamage::ResolveLegacyDefense(0, InPhysicalResistance, InMagicResistance));
}

void UHealthComponent::SetRuntimeDefenseData(
	float InMaxHealth,
	float InCurrentHealth,
	int32 InDefense)
{
	MaxHealth = FMath::Max(1.0f, InMaxHealth);
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);
	Defense = FMath::Max(0, InDefense);
	PhysicalResistance = Defense;
	MagicResistance = Defense;
	BroadcastHealthChanged();
}

void UHealthComponent::CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const
{
	OutRuntimeData.Stats.MaxHealth = FMath::Max(1.0f, MaxHealth);
	OutRuntimeData.Stats.CurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, OutRuntimeData.Stats.MaxHealth);
	OutRuntimeData.Stats.Defense = FMath::Max(0, Defense);
	// Keep legacy snapshot fields synchronized for older readers.
	OutRuntimeData.Stats.PhysicalResistance = OutRuntimeData.Stats.Defense;
	OutRuntimeData.Stats.MagicResistance = OutRuntimeData.Stats.Defense;

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Health runtime data captured: Owner=%s HP=%.0f/%.0f Defense=%d."),
		*GetNameSafe(GetOwner()),
		OutRuntimeData.Stats.CurrentHealth,
		OutRuntimeData.Stats.MaxHealth,
		OutRuntimeData.Stats.Defense);
}

void UHealthComponent::ApplyRuntimeData(const FPlayerRuntimeData& InRuntimeData)
{
	const int32 RuntimeDefense = RiverOfInkDamage::ResolveLegacyDefense(
		InRuntimeData.Stats.Defense,
		InRuntimeData.Stats.PhysicalResistance,
		InRuntimeData.Stats.MagicResistance);
	SetRuntimeDefenseData(
		InRuntimeData.Stats.MaxHealth,
		InRuntimeData.Stats.CurrentHealth,
		RuntimeDefense);

	UE_LOG(LogRiverOfInk, Verbose,
		TEXT("Health runtime data applied: Owner=%s HP=%.0f/%.0f Defense=%d."),
		*GetNameSafe(GetOwner()),
		CurrentHealth,
		MaxHealth,
		Defense);
}

void UHealthComponent::SetCurrentHealth(float InCurrentHealth)
{
	CurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, MaxHealth);
	BroadcastHealthChanged();
}

void UHealthComponent::NormalizeDefenseFromLegacy()
{
	Defense = RiverOfInkDamage::ResolveLegacyDefense(Defense, PhysicalResistance, MagicResistance);
	PhysicalResistance = Defense;
	MagicResistance = Defense;
}

void UHealthComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

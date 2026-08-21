// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/HealthComponent.h"

#include "Common/CombatEffectComponent.h"
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
	ApplyDamageContext(FDamageContext(InInfo));
}

void UHealthComponent::ApplyDamageContext(const FDamageContext& InContext)
{
	if (bIsDead)
	{
		return;
	}

	FDamageContext Context = InContext;
	Context.TargetActor = GetOwner();
	if (Context.BaseDamage <= 0.0f)
	{
		FDamageResult ZeroDamageResult;
		ZeroDamageResult.Context = Context;
		ZeroDamageResult.bNoDamage = true;
		OnDamageResolved.Broadcast(ZeroDamageResult);
		return;
	}

	const UCombatEffectComponent* TargetEffects = GetOwner()
		? GetOwner()->FindComponentByClass<UCombatEffectComponent>()
		: nullptr;
	const UCombatEffectComponent* SourceEffects = Context.SourceActor
		? Context.SourceActor->FindComponentByClass<UCombatEffectComponent>()
		: nullptr;
	const FDamageResult DamageResult = RiverOfInkDamage::ResolveDamage(
		Context,
		SourceEffects,
		TargetEffects,
		static_cast<float>(Defense));
	OnDamageResolved.Broadcast(DamageResult);

	if (DamageResult.bBlockedByInvulnerability)
	{
		UE_LOG(LogRiverOfInk, Verbose,
			TEXT("Health component damage blocked by invulnerability: Owner=%s Source=%s."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Context.SourceActor));
		return;
	}

	if (!DamageResult.bDamageApplied)
	{
		return;
	}

	FTakeDamageInfo EffectiveInfo = Context.ToLegacyDamageInfo();
	EffectiveInfo.DamageValue = DamageResult.ModifiedDamage;
	if (Context.bIsDirectDamage)
	{
		OnTakeDirectDamage.Broadcast(EffectiveInfo);
	}

	const int32 FinalDamage = DamageResult.FinalDamage;

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - static_cast<float>(FinalDamage));
	UE_LOG(LogRiverOfInk, Log,
		TEXT("Health component damage: Owner=%s Damage=%d Defense=%d CurrentHealth=%.1f."),
		*GetNameSafe(GetOwner()),
		FinalDamage,
		Defense,
		CurrentHealth);

	if (CurrentHealth <= 0.0f)
	{
		if (Context.bCanCauseDeath)
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

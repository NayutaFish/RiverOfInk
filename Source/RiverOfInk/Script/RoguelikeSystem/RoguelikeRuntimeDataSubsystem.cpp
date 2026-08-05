// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRuntimeDataSubsystem.h"

#include "Common/HealthComponent.h"
#include "Player/PlayerCharacter.h"

DEFINE_LOG_CATEGORY(LogRoguelikeRuntimeData);

void URoguelikeRuntimeDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetPlayerRuntimeData();

	UE_LOG(LogRoguelikeRuntimeData, Log, TEXT("Runtime data subsystem initialized."));
}

void URoguelikeRuntimeDataSubsystem::Deinitialize()
{
	ResetPlayerRuntimeData();
	Super::Deinitialize();
}

bool URoguelikeRuntimeDataSubsystem::CapturePlayerRuntimeData(const APlayerCharacter* Player)
{
	if (!IsValid(Player))
	{
		UE_LOG(LogRoguelikeRuntimeData, Warning,
			TEXT("Cannot capture player runtime data: Player is invalid."));
		return false;
	}

	FPlayerRuntimeData CapturedData;
	if (!Player->CaptureRuntimeData(CapturedData))
	{
		UE_LOG(LogRoguelikeRuntimeData, Warning,
			TEXT("Player runtime capture incomplete: Player=%s."),
			*Player->GetName());
		return false;
	}

	return RegisterPlayerRuntimeData(CapturedData);
}

bool URoguelikeRuntimeDataSubsystem::RegisterPlayerRuntimeData(const FPlayerRuntimeData& InRuntimeData)
{
	PlayerRuntimeData = InRuntimeData;
	PlayerRuntimeData.Stats.MaxHealth = FMath::Max(1.0f, PlayerRuntimeData.Stats.MaxHealth);
	PlayerRuntimeData.Stats.CurrentHealth = FMath::Clamp(
		PlayerRuntimeData.Stats.CurrentHealth,
		0.0f,
		PlayerRuntimeData.Stats.MaxHealth);
	bHasPlayerRuntimeData = true;

	UE_LOG(LogRoguelikeRuntimeData, Log,
		TEXT("Player runtime data registered: HP=%.0f/%.0f PhysicalResistance=%d MagicResistance=%d WalkSpeed=%.0f SprintSpeed=%.0f Skills=%d Upgrades=%d Buffs=%d."),
		PlayerRuntimeData.Stats.CurrentHealth,
		PlayerRuntimeData.Stats.MaxHealth,
		PlayerRuntimeData.Stats.PhysicalResistance,
		PlayerRuntimeData.Stats.MagicResistance,
		PlayerRuntimeData.Stats.WalkSpeed,
		PlayerRuntimeData.Stats.SprintSpeed,
		PlayerRuntimeData.SkillSlots.Num(),
		PlayerRuntimeData.SkillUpgradeStates.Num(),
		PlayerRuntimeData.RunBuffs.Num());

	return true;
}

bool URoguelikeRuntimeDataSubsystem::ApplyRegisteredPlayerRuntimeData(APlayerCharacter* Player) const
{
	if (!bHasPlayerRuntimeData)
	{
		UE_LOG(LogRoguelikeRuntimeData, Verbose,
			TEXT("No registered player runtime data to apply."));
		return false;
	}

	if (!IsValid(Player))
	{
		UE_LOG(LogRoguelikeRuntimeData, Warning,
			TEXT("Cannot apply player runtime data: Player is invalid."));
		return false;
	}

	if (!Player->ApplyRuntimeData(PlayerRuntimeData))
	{
		UE_LOG(LogRoguelikeRuntimeData, Warning,
			TEXT("Player runtime apply incomplete: Player=%s."),
			*Player->GetName());
		return false;
	}

	UE_LOG(LogRoguelikeRuntimeData, Log,
		TEXT("Player runtime data applied: Player=%s HP=%.0f/%.0f PhysicalResistance=%d MagicResistance=%d WalkSpeed=%.0f SprintSpeed=%.0f Skills=%d Upgrades=%d Buffs=%d."),
		*Player->GetName(),
		Player->GetHealthComponent()->GetCurrentHealth(),
		Player->GetHealthComponent()->GetMaxHealth(),
		Player->GetHealthComponent()->GetPhysicalResistance(),
		Player->GetHealthComponent()->GetMagicResistance(),
		Player->WalkSpeed,
		Player->SprintSpeed,
		PlayerRuntimeData.SkillSlots.Num(),
		PlayerRuntimeData.SkillUpgradeStates.Num(),
		PlayerRuntimeData.RunBuffs.Num());

	return true;
}

void URoguelikeRuntimeDataSubsystem::ResetPlayerRuntimeData()
{
	PlayerRuntimeData = FPlayerRuntimeData();
	bHasPlayerRuntimeData = false;
}

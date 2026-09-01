// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRuntimeDataSubsystem.h"

#include "Common/HealthComponent.h"
#include "Core/CombatDamageCalculator.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
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

	// RunBuffs are owned by this GameInstance subsystem rather than the Pawn.
	// Preserve the already registered list while refreshing transient HP/skill
	// values from the live player before a purchase or room transition.
	CapturedData.RunBuffs = PlayerRuntimeData.RunBuffs;

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
	PlayerRuntimeData.Stats.Defense = RiverOfInkDamage::ResolveLegacyDefense(
		PlayerRuntimeData.Stats.Defense,
		PlayerRuntimeData.Stats.PhysicalResistance,
		PlayerRuntimeData.Stats.MagicResistance);
	// Keep old readers and Blueprint snapshots compatible during migration.
	PlayerRuntimeData.Stats.PhysicalResistance = PlayerRuntimeData.Stats.Defense;
	PlayerRuntimeData.Stats.MagicResistance = PlayerRuntimeData.Stats.Defense;
	bHasPlayerRuntimeData = true;

	UE_LOG(LogRoguelikeRuntimeData, Log,
		TEXT("Player runtime data registered: HP=%.0f/%.0f Defense=%d WalkSpeed=%.0f SprintSpeed=%.0f Skills=%d Upgrades=%d Builds=%d Buffs=%d."),
		PlayerRuntimeData.Stats.CurrentHealth,
		PlayerRuntimeData.Stats.MaxHealth,
		PlayerRuntimeData.Stats.Defense,
		PlayerRuntimeData.Stats.WalkSpeed,
		PlayerRuntimeData.Stats.SprintSpeed,
		PlayerRuntimeData.SkillSlots.Num(),
		PlayerRuntimeData.SkillUpgradeStates.Num(),
		PlayerRuntimeData.BuildHistory.Num(),
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
		TEXT("Player runtime data applied: Player=%s HP=%.0f/%.0f Defense=%d WalkSpeed=%.0f SprintSpeed=%.0f Skills=%d Upgrades=%d Builds=%d Buffs=%d."),
		*Player->GetName(),
		Player->GetHealthComponent()->GetCurrentHealth(),
		Player->GetHealthComponent()->GetMaxHealth(),
		Player->GetHealthComponent()->GetDefense(),
		Player->WalkSpeed,
		Player->SprintSpeed,
		PlayerRuntimeData.SkillSlots.Num(),
		PlayerRuntimeData.SkillUpgradeStates.Num(),
		PlayerRuntimeData.BuildHistory.Num(),
		PlayerRuntimeData.RunBuffs.Num());

	return true;
}

bool URoguelikeRuntimeDataSubsystem::AddTemporaryPlayerStatBoost(
	APlayerCharacter* Player,
	const FRunBuffData& InBuff)
{
	if (!IsValid(Player)
		|| InBuff.BuffId.IsNone()
		|| InBuff.RemainCombatCount <= 0
		|| !FMath::IsFinite(InBuff.AdditiveValue)
		|| !FMath::IsFinite(InBuff.MultiplierValue)
		|| InBuff.MultiplierValue <= 0.0f)
	{
		UE_LOG(LogRoguelikeRuntimeData, Warning,
			TEXT("Temporary shop buff rejected: invalid player/data."));
		return false;
	}

	if (!CapturePlayerRuntimeData(Player))
	{
		return false;
	}

	PlayerRuntimeData.RunBuffs.Add(InBuff);
	if (!ApplyRegisteredPlayerRuntimeData(Player))
	{
		PlayerRuntimeData.RunBuffs.RemoveAt(PlayerRuntimeData.RunBuffs.Num() - 1);
		return false;
	}

	UE_LOG(LogRoguelikeRuntimeData, Log,
		TEXT("Temporary shop buff registered: BuffId=%s Stat=%d Add=%.2f Mult=%.3f Rooms=%d Active=%d."),
		*InBuff.BuffId.ToString(),
		static_cast<int32>(InBuff.StatType),
		InBuff.AdditiveValue,
		InBuff.MultiplierValue,
		InBuff.RemainCombatCount,
		PlayerRuntimeData.RunBuffs.Num());
	return true;
}

bool URoguelikeRuntimeDataSubsystem::ConsumeCombatRoomDurations()
{
	bool bChanged = false;
	for (int32 Index = PlayerRuntimeData.RunBuffs.Num() - 1; Index >= 0; --Index)
	{
		FRunBuffData& Buff = PlayerRuntimeData.RunBuffs[Index];
		if (Buff.RemainCombatCount <= 0)
		{
			PlayerRuntimeData.RunBuffs.RemoveAt(Index);
			bChanged = true;
			continue;
		}

		--Buff.RemainCombatCount;
		bChanged = true;
		if (Buff.RemainCombatCount <= 0)
		{
			PlayerRuntimeData.RunBuffs.RemoveAt(Index);
		}
	}

	if (!bChanged)
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	APlayerCharacter* Player = World
		? Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(World, 0))
		: nullptr;
	if (IsValid(Player) && bHasPlayerRuntimeData)
	{
		ApplyRegisteredPlayerRuntimeData(Player);
	}

	UE_LOG(LogRoguelikeRuntimeData, Log,
		TEXT("Temporary shop buff durations consumed: RemainingBuffs=%d."),
		PlayerRuntimeData.RunBuffs.Num());
	return true;
}

void URoguelikeRuntimeDataSubsystem::ResetPlayerRuntimeData()
{
	PlayerRuntimeData = FPlayerRuntimeData();
	bHasPlayerRuntimeData = false;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PlayerCheatCommands.h"

#include "Common/HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"

bool UPlayerCheatCommands::HealPlayer(const UObject* WorldContextObject, float Amount)
{
	const float HealAmount = Amount > 0.0f ? Amount : DefaultHealAmount;

	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	APlayerCharacter* Player = World
		? Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0))
		: nullptr;

	if (!IsValid(Player))
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Cheat heal failed: no player character for the supplied world context."));
		return false;
	}

	UHealthComponent* Health = Player->GetHealthComponent();
	if (!IsValid(Health))
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Cheat heal failed: %s has no HealthComponent."), *Player->GetName());
		return false;
	}

	const float MaxHealth = Health->GetMaxHealth();
	const float PreviousHealth = Health->GetCurrentHealth();
	Health->SetCurrentHealth(FMath::Min(MaxHealth, PreviousHealth + HealAmount));

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Cheat heal: Player=%s HP %.0f -> %.0f / %.0f (+%.0f)."),
		*Player->GetName(),
		PreviousHealth,
		Health->GetCurrentHealth(),
		MaxHealth,
		HealAmount);
	return true;
}

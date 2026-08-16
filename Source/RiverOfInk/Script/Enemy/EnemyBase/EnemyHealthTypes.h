// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyHealthTypes.generated.h"

/** Gameplay-layer reason for an enemy health value change. */
UENUM(BlueprintType)
enum class EEnemyHealthChangeReason : uint8
{
	Initialize UMETA(DisplayName = "Initialize"),
	Damage UMETA(DisplayName = "Damage"),
	Heal UMETA(DisplayName = "Heal"),
	Death UMETA(DisplayName = "Death"),
	ExternalSet UMETA(DisplayName = "External Set")
};

/** Gameplay-layer enemy rank used by shared presentation widgets. */
UENUM(BlueprintType)
enum class EEnemyRank : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	Elite UMETA(DisplayName = "Elite")
};

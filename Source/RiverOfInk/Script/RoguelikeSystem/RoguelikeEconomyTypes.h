// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoguelikeEconomyTypes.generated.h"

/** Reason attached to every Pure Ink balance change for logs and HUD events. */
UENUM(BlueprintType)
enum class EPureInkChangeReason : uint8
{
	EnemyDrop UMETA(DisplayName = "Enemy Drop"),
	RoomResult UMETA(DisplayName = "Room Result"),
	ShopPurchase UMETA(DisplayName = "Shop Purchase"),
	NewRunReset UMETA(DisplayName = "New Run Reset")
};

/** Effect categories reserved for the first Shop Room implementation. */
UENUM(BlueprintType)
enum class EShopItemEffectType : uint8
{
	RestoreHealth UMETA(DisplayName = "Restore Health"),
	TemporaryStatBoost UMETA(DisplayName = "Temporary Stat Boost"),
	ImmediateRewardChoice UMETA(DisplayName = "Immediate Reward Choice")
};

/** Runtime stat that a temporary Shop buff may modify. */
UENUM(BlueprintType)
enum class EPlayerRuntimeStat : uint8
{
	MaxHealth UMETA(DisplayName = "Max Health"),
	Defense UMETA(DisplayName = "Defense"),
	WalkSpeed UMETA(DisplayName = "Walk Speed"),
	SprintSpeed UMETA(DisplayName = "Sprint Speed")
};

/** Value data owned by the GameInstance-level economy subsystem. */
USTRUCT(BlueprintType)
struct FPureInkWallet
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Economy")
	int32 Balance = 0;
};

/** Data-only Shop offer contract. Purchase state belongs to the Shop manager. */
USTRUCT(BlueprintType)
struct FShopItemDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop", meta = (ClampMin = "0"))
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop")
	EShopItemEffectType EffectType = EShopItemEffectType::RestoreHealth;

	/** Fixed effect magnitude. Meaning is interpreted by the future Shop manager. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop")
	float EffectValue = 0.0f;

	/** Number of subsequent Combat Rooms for a temporary stat effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop", meta = (ClampMin = "0"))
	int32 CombatRoomDuration = 0;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoguelikeSystem/RoguelikeEconomyTypes.h"
#include "RoguelikeShopManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnRoguelikeShopPurchaseCompleted,
	FName,
	ItemId,
	int32,
	Cost,
	int32,
	NewBalance
);

/**
 * Map-local owner for Shop Room offers and one-purchase-per-item rules.
 *
 * The GameInstance economy subsystem remains the only balance owner. This
 * actor validates the current Shop Room, requests one atomic spend, and keeps
 * the sold-out set local to this room. UI can bind to the read-only offer and
 * purchase APIs without creating a second currency copy.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API ARoguelikeShopManager : public AActor
{
	GENERATED_BODY()

public:
	ARoguelikeShopManager();

	virtual void BeginPlay() override;

	/** Return the balance owned by the GameInstance economy subsystem. */
	UFUNCTION(BlueprintPure, Category = "Roguelike|Shop")
	int32 GetCurrentPureInkBalance() const;

	/** Return all configured offers; sold-out state is queried separately. */
	UFUNCTION(BlueprintPure, Category = "Roguelike|Shop")
	TArray<FShopItemDefinition> GetShopItems() const { return ShopItems; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Shop")
	bool IsItemPurchased(FName ItemId) const;

	/** Check room, offer, sold-out, and balance constraints without spending. */
	UFUNCTION(BlueprintPure, Category = "Roguelike|Shop")
	bool CanPurchaseItem(FName ItemId) const;

	/** Spend once and mark the item sold out for this Shop Room. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Shop")
	bool PurchaseItem(FName ItemId);

	/**
	 * Whitebox switch for data-only tests. Production Shop Rooms keep this true;
	 * a future test map may disable it without changing EconomySubsystem rules.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop|Validation")
	bool bRequireShopRoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roguelike|Shop")
	TArray<FShopItemDefinition> ShopItems;

	UPROPERTY(BlueprintAssignable, Category = "Roguelike|Shop|Events")
	FOnRoguelikeShopPurchaseCompleted OnPurchaseCompleted;

private:
	const FShopItemDefinition* FindItem(FName ItemId) const;
	bool IsShopRoomActive() const;
	void AddDefaultOffersIfUnset();
	void ApplyImmediateItemEffect(const FShopItemDefinition& Item);

	/** Map-local sold-out state; a new Shop Room creates a new manager. */
	TSet<FName> PurchasedItemIds;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeShopManager.h"

#include "Common/HealthComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"
#include "RiverOfInk.h"

ARoguelikeShopManager::ARoguelikeShopManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ARoguelikeShopManager::BeginPlay()
{
	Super::BeginPlay();
	AddDefaultOffersIfUnset();

	const UGameInstance* GameInstance = GetGameInstance();
	const URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;

	UE_LOG(LogRoguelike, Log,
		TEXT("Shop manager ready: RoomType=%d Offers=%d Balance=%d RequireShopRoom=%s."),
		RunFlow ? static_cast<int32>(RunFlow->GetCurrentRoomDefinition().RoomType) : -1,
		ShopItems.Num(),
		GetCurrentPureInkBalance(),
		bRequireShopRoom ? TEXT("true") : TEXT("false"));
}

int32 ARoguelikeShopManager::GetCurrentPureInkBalance() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const URoguelikeEconomySubsystem* Economy = GameInstance
		? GameInstance->GetSubsystem<URoguelikeEconomySubsystem>()
		: nullptr;
	return Economy ? Economy->GetPureInkBalance() : 0;
}

bool ARoguelikeShopManager::IsItemPurchased(FName ItemId) const
{
	return !ItemId.IsNone() && PurchasedItemIds.Contains(ItemId);
}

bool ARoguelikeShopManager::CanPurchaseItem(FName ItemId) const
{
	const FShopItemDefinition* Item = FindItem(ItemId);
	if (!Item || Item->Cost <= 0 || IsItemPurchased(ItemId))
	{
		return false;
	}

	if (bRequireShopRoom && !IsShopRoomActive())
	{
		return false;
	}

	return GetCurrentPureInkBalance() >= Item->Cost;
}

bool ARoguelikeShopManager::PurchaseItem(FName ItemId)
{
	const FShopItemDefinition* Item = FindItem(ItemId);
	if (!Item)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Shop purchase rejected: ItemId=%s Reason=UnknownItem."),
			*ItemId.ToString());
		return false;
	}

	if (IsItemPurchased(ItemId))
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Shop purchase rejected: ItemId=%s Reason=SoldOut."),
			*ItemId.ToString());
		return false;
	}

	if (bRequireShopRoom && !IsShopRoomActive())
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Shop purchase rejected: ItemId=%s Reason=NotShopRoom."),
			*ItemId.ToString());
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeEconomySubsystem* Economy = GameInstance
		? GameInstance->GetSubsystem<URoguelikeEconomySubsystem>()
		: nullptr;
	if (!Economy || !Economy->TrySpendPureInk(Item->Cost, EPureInkChangeReason::ShopPurchase))
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Shop purchase rejected: ItemId=%s Cost=%d Balance=%d Reason=InsufficientInk."),
			*ItemId.ToString(),
			Item->Cost,
			GetCurrentPureInkBalance());
		return false;
	}

	PurchasedItemIds.Add(ItemId);
	ApplyImmediateItemEffect(*Item);

	const int32 NewBalance = Economy->GetPureInkBalance();
	UE_LOG(LogRoguelike, Log,
		TEXT("Shop purchase succeeded: ItemId=%s Cost=%d Balance=%d SoldOut=true."),
		*ItemId.ToString(),
		Item->Cost,
		NewBalance);
	OnPurchaseCompleted.Broadcast(ItemId, Item->Cost, NewBalance);
	return true;
}

const FShopItemDefinition* ARoguelikeShopManager::FindItem(FName ItemId) const
{
	return ShopItems.FindByPredicate(
		[ItemId](const FShopItemDefinition& Item)
		{
			return Item.ItemId == ItemId;
		});
}

bool ARoguelikeShopManager::IsShopRoomActive() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;
	return RunFlow
		&& RunFlow->GetRunState() == ERoguelikeRunState::InRoom
		&& RunFlow->GetCurrentRoomDefinition().RoomType == ERoguelikeRoomType::Shop;
}

void ARoguelikeShopManager::AddDefaultOffersIfUnset()
{
	if (!ShopItems.IsEmpty())
	{
		return;
	}

	FShopItemDefinition RestoreHealth;
	RestoreHealth.ItemId = TEXT("shop_restore_health");
	RestoreHealth.Title = FText::FromString(TEXT("Pure Wash"));
	RestoreHealth.Description = FText::FromString(TEXT("Restore 500 HP."));
	RestoreHealth.Cost = 10;
	RestoreHealth.EffectType = EShopItemEffectType::RestoreHealth;
	RestoreHealth.EffectValue = 500.0f;
	ShopItems.Add(RestoreHealth);

	FShopItemDefinition RewardChoice;
	RewardChoice.ItemId = TEXT("shop_extra_reward");
	RewardChoice.Title = FText::FromString(TEXT("Ink Echo"));
	RewardChoice.Description = FText::FromString(TEXT("Open one additional two-option reward choice."));
	RewardChoice.Cost = 25;
	RewardChoice.EffectType = EShopItemEffectType::ImmediateRewardChoice;
	ShopItems.Add(RewardChoice);
}

void ARoguelikeShopManager::ApplyImmediateItemEffect(const FShopItemDefinition& Item)
{
	if (Item.EffectType == EShopItemEffectType::RestoreHealth)
	{
		APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		UHealthComponent* Health = Player ? Player->GetHealthComponent() : nullptr;
		if (Health)
		{
			Health->SetCurrentHealth(FMath::Min(
				Health->GetMaxHealth(),
				Health->GetCurrentHealth() + FMath::Max(0.0f, Item.EffectValue)));
			UE_LOG(LogRoguelike, Log,
				TEXT("Shop effect applied: ItemId=%s HP=%.0f/%.0f."),
				*Item.ItemId.ToString(),
				Health->GetCurrentHealth(),
				Health->GetMaxHealth());
		}
		return;
	}

	// Temporary buffs and immediate reward re-entry are intentionally only
	// reserved in this minimal slice. They will consume the existing RuntimeData
	// and RewardManager contracts in the dedicated Shop effect slice.
	UE_LOG(LogRoguelike, Log,
		TEXT("Shop effect reserved for follow-up slice: ItemId=%s EffectType=%d."),
		*Item.ItemId.ToString(),
		static_cast<int32>(Item.EffectType));
}

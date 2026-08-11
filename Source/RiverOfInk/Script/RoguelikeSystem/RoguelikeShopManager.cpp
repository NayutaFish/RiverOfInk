// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeShopManager.h"

#include "Common/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"
#include "RiverOfInk.h"
#include "UI/RoguelikeShopPromptWidget.h"
#include "UI/RoguelikeShopWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

ARoguelikeShopManager::ARoguelikeShopManager()
{
	PrimaryActorTick.bCanEverTick = false;

	ShopArea = CreateDefaultSubobject<UBoxComponent>(TEXT("ShopArea"));
	SetRootComponent(ShopArea);
	ShopArea->InitBoxExtent(FVector(220.0f, 220.0f, 180.0f));
	ShopArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ShopArea->SetCollisionObjectType(ECC_WorldDynamic);
	ShopArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	ShopArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// This project assigns the player capsule to GameTraceChannel3 rather than
	// the default Pawn object channel. Listen to both so Shop Area overlap is
	// robust for native and Blueprint player variants.
	ShopArea->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	ShopArea->SetGenerateOverlapEvents(true);

	TraderMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TraderMarker"));
	TraderMarker->SetupAttachment(ShopArea);
	TraderMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TraderMarker->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	TraderMarker->SetRelativeScale3D(FVector(1.45f, 1.45f, 2.0f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TraderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	TraderMarker->SetStaticMesh(TraderMesh.Object);

	TraderNameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TraderNameplate"));
	TraderNameplate->SetupAttachment(ShopArea);
	TraderNameplate->SetRelativeLocation(FVector(0.0f, 0.0f, 255.0f));
	TraderNameplate->SetHorizontalAlignment(EHTA_Center);
	TraderNameplate->SetTextRenderColor(FColor(112, 220, 255));
	TraderNameplate->SetWorldSize(28.0f);
	TraderNameplate->SetText(FText::FromString(TEXT("INK TRADER")));
}

void ARoguelikeShopManager::BeginPlay()
{
	Super::BeginPlay();
	AddDefaultOffersIfUnset();
	ShopArea->OnComponentBeginOverlap.AddDynamic(this, &ARoguelikeShopManager::HandleShopAreaBeginOverlap);
	ShopArea->OnComponentEndOverlap.AddDynamic(this, &ARoguelikeShopManager::HandleShopAreaEndOverlap);
	// Components may already overlap after a map transition, before the
	// delegate bindings above exist. Resolve that one initial state on the
	// following frame; normal enter/exit behavior remains overlap-driven.
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			RegisterPlayerIfAlreadyInsideShopArea();
		}));

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

void ARoguelikeShopManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseShop();
	HideInteractionPrompt();
	NearbyPlayer.Reset();
	Super::EndPlay(EndPlayReason);
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

	return CanApplyItemEffect(*Item) && GetCurrentPureInkBalance() >= Item->Cost;
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

	if (Item->Cost <= 0)
	{
		UE_LOG(LogRoguelike, Warning,
			TEXT("Shop purchase rejected: ItemId=%s Reason=InvalidCost."),
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

	if (!CanApplyItemEffect(*Item))
	{
		UE_LOG(LogRoguelike, Log,
			TEXT("Shop purchase rejected: ItemId=%s Reason=EffectUnavailableOrFullHealth."),
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

bool ARoguelikeShopManager::TryOpenShop(APlayerCharacter* InPlayer)
{
	if (!IsValid(InPlayer) || NearbyPlayer.Get() != InPlayer || !IsInteractionAvailable())
	{
		UE_LOG(LogRoguelike, Verbose,
			TEXT("Shop interaction ignored: Player=%s Nearby=%s Active=%s."),
			*GetNameSafe(InPlayer),
			*GetNameSafe(NearbyPlayer.Get()),
			IsInteractionAvailable() ? TEXT("true") : TEXT("false"));
		return false;
	}

	if (ActiveShopWidget && ActiveShopWidget->IsInViewport())
	{
		ActiveShopWidget->FocusFirstPurchase();
		return true;
	}

	APlayerController* PlayerController = Cast<APlayerController>(InPlayer->GetController());
	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	}
	if (!PlayerController)
	{
		UE_LOG(LogRoguelike, Warning, TEXT("Shop interaction failed: local PlayerController is unavailable."));
		return false;
	}

	ActiveShopWidget = CreateWidget<URoguelikeShopWidget>(PlayerController, URoguelikeShopWidget::StaticClass());
	if (!ActiveShopWidget)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Shop interaction failed: Shop HUD creation returned null."));
		return false;
	}

	HideInteractionPrompt();
	ActiveShopWidget->SetIsFocusable(true);
	ActiveShopWidget->SetVisibility(ESlateVisibility::Visible);
	ActiveShopWidget->AddToViewport(100);
	ActiveShopWidget->InitializeForShop(this);
	ActiveShopWidget->ForceLayoutPrepass();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(ActiveShopWidget->TakeWidget());
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	ActiveShopWidget->FocusFirstPurchase();

	UE_LOG(LogRoguelike, Log,
		TEXT("Shop HUD opened: Player=%s Offers=%d Balance=%d."),
		*GetNameSafe(InPlayer),
		ShopItems.Num(),
		GetCurrentPureInkBalance());
	return true;
}

void ARoguelikeShopManager::CloseShop()
{
	if (ActiveShopWidget)
	{
		ActiveShopWidget->RemoveFromParent();
		ActiveShopWidget = nullptr;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
	}

	if (NearbyPlayer.IsValid() && IsInteractionAvailable())
	{
		ShowInteractionPrompt(NearbyPlayer.Get());
	}
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

bool ARoguelikeShopManager::CanApplyItemEffect(const FShopItemDefinition& Item) const
{
	if (Item.EffectType != EShopItemEffectType::RestoreHealth)
	{
		return false;
	}

	const APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const UHealthComponent* Health = Player ? Player->GetHealthComponent() : nullptr;
	return Health
		&& Health->GetCurrentHealth() < Health->GetMaxHealth() - KINDA_SMALL_NUMBER
		&& Item.EffectValue > 0.0f;
}

bool ARoguelikeShopManager::IsInteractionAvailable() const
{
	return !bRequireShopRoom || IsShopRoomActive();
}

void ARoguelikeShopManager::AddDefaultOffersIfUnset()
{
	if (!ShopItems.IsEmpty())
	{
		return;
	}

	const auto AddRestoreOffer = [this](
		const TCHAR* ItemId,
		const TCHAR* Title,
		const TCHAR* Description,
		int32 Cost,
		float RestoreAmount)
	{
		FShopItemDefinition RestoreHealth;
		RestoreHealth.ItemId = ItemId;
		RestoreHealth.Title = FText::FromString(Title);
		RestoreHealth.Description = FText::FromString(Description);
		RestoreHealth.Cost = Cost;
		RestoreHealth.EffectType = EShopItemEffectType::RestoreHealth;
		RestoreHealth.EffectValue = RestoreAmount;
		ShopItems.Add(MoveTemp(RestoreHealth));
	};

	// Slice 4 exposes exactly three immediately usable offers. Reward and
	// temporary-buff effects remain reserved until their own transaction slices.
	AddRestoreOffer(TEXT("shop_restore_small"), TEXT("Quick Rinse"), TEXT("Restore 250 HP."), 5, 250.0f);
	AddRestoreOffer(TEXT("shop_restore_health"), TEXT("Pure Wash"), TEXT("Restore 500 HP."), 10, 500.0f);
	AddRestoreOffer(TEXT("shop_restore_full"), TEXT("Deep Cleanse"), TEXT("Restore 1000 HP."), 18, 1000.0f);
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

	UE_LOG(LogRoguelike, Warning,
		TEXT("Shop effect was not applied because the item passed purchase validation unexpectedly: ItemId=%s EffectType=%d."),
		*Item.ItemId.ToString(),
		static_cast<int32>(Item.EffectType));
}

void ARoguelikeShopManager::RegisterPlayerIfAlreadyInsideShopArea()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!IsValid(Player) || !IsInteractionAvailable() || !ShopArea->IsOverlappingActor(Player))
	{
		return;
	}

	NearbyPlayer = Player;
	Player->SetNearbyShopManager(this);
	ShowInteractionPrompt(Player);
	UE_LOG(LogRoguelike, Log, TEXT("Shop area initial overlap registered: Player=%s."), *GetNameSafe(Player));
}

void ARoguelikeShopManager::ShowInteractionPrompt(APlayerCharacter* InPlayer)
{
	if (!IsValid(InPlayer) || !InPlayer->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(InPlayer->GetController());
	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	}
	if (!PlayerController)
	{
		return;
	}

	if (!ActiveInteractionPrompt)
	{
		ActiveInteractionPrompt = CreateWidget<URoguelikeShopPromptWidget>(
			PlayerController,
			URoguelikeShopPromptWidget::StaticClass());
		if (!ActiveInteractionPrompt)
		{
			return;
		}
		ActiveInteractionPrompt->AddToViewport(50);
	}

	ActiveInteractionPrompt->SetPromptText(FText::Format(
		FText::FromString(TEXT("[ {0} ]  {1}")),
		InPlayer->GetShopInteractionKeyLabel(),
		InteractionPrompt));
	ActiveInteractionPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ARoguelikeShopManager::HideInteractionPrompt()
{
	if (ActiveInteractionPrompt)
	{
		ActiveInteractionPrompt->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ARoguelikeShopManager::HandleShopAreaBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!IsValid(Player) || !IsInteractionAvailable())
	{
		return;
	}

	NearbyPlayer = Player;
	Player->SetNearbyShopManager(this);
	if (!ActiveShopWidget)
	{
		ShowInteractionPrompt(Player);
	}

	UE_LOG(LogRoguelike, Log, TEXT("Shop area entered: Player=%s."), *GetNameSafe(Player));
}

void ARoguelikeShopManager::HandleShopAreaEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;

	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!IsValid(Player) || NearbyPlayer.Get() != Player)
	{
		return;
	}

	Player->ClearNearbyShopManager(this);
	NearbyPlayer.Reset();
	HideInteractionPrompt();
	UE_LOG(LogRoguelike, Log, TEXT("Shop area exited: Player=%s."), *GetNameSafe(Player));
}

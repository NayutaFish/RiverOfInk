// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "RoguelikeSystem/RoguelikeEconomyTypes.h"
#include "RoguelikeShopWidget.generated.h"

class ARoguelikeShopManager;
class UBorder;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UTextBlock;
class UTexture2D;
class URoguelikeEconomySubsystem;

/**
 * Native three-slot Shop HUD.
 *
 * The UI is fed by Economy/Shop events only: balance and sold-out state are
 * refreshed after a transaction, not from Tick. The first enabled BuyButton
 * receives keyboard focus whenever the screen opens.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API URoguelikeShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Shop|HUD")
	void InitializeForShop(ARoguelikeShopManager* InShopManager);

	void FocusFirstPurchase();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	static constexpr int32 VisibleOfferCount = 3;

	void BuildDefaultWidgetTree();
	void BindShopEvents();
	void UnbindShopEvents();
	void RefreshShop();
	void RefreshOfferSlot(int32 SlotIndex);
	void TryPurchaseSlot(int32 SlotIndex);
	void SetFeedbackText(const FText& InText, const FLinearColor& InColor);

	UFUNCTION()
	void HandleBuyFirst();

	UFUNCTION()
	void HandleBuySecond();

	UFUNCTION()
	void HandleBuyThird();

	UFUNCTION()
	void HandlePurchaseCompleted(FName ItemId, int32 Cost, int32 NewBalance);

	UFUNCTION()
	void HandlePureInkChanged(int32 PreviousBalance, int32 NewBalance, int32 Delta, EPureInkChangeReason Reason);

	UPROPERTY(Transient)
	TObjectPtr<ARoguelikeShopManager> ObservedShopManager;

	UPROPERTY(Transient)
	TObjectPtr<URoguelikeEconomySubsystem> ObservedEconomy;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BalanceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedbackText;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> InkIconTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> OfferCards;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ItemImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ItemTitles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DescriptionTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PureInkCostTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> BuyButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BuyButtonTexts;

	TArray<FName> DisplayedItemIds;
	bool bShopEventSubscribed = false;
	bool bEconomyEventSubscribed = false;
};

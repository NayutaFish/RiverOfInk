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
 * Native five-row Shop HUD.
 *
 * The UI is fed by Economy/Shop events only: balance and sold-out state are
 * refreshed after a transaction, not from Tick. The selected offer row and
 * the single global purchase button receive keyboard focus through the normal
 * UMG input flow.
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
	static constexpr int32 VisibleOfferCount = 5;

	void BuildDefaultWidgetTree();
	void BindShopEvents();
	void UnbindShopEvents();
	void RefreshShop();
	void RefreshOfferSlot(int32 SlotIndex, const TArray<FShopItemDefinition>& Offers);
	void RefreshSelectionState();
	void SelectOffer(int32 SlotIndex);
	void TryPurchaseSelected();
	int32 FindFirstSelectableOffer(const TArray<FShopItemDefinition>& Offers) const;
	void SetFeedbackText(const FText& InText, const FLinearColor& InColor);

	UFUNCTION()
	void HandleSelectFirst();

	UFUNCTION()
	void HandleSelectSecond();

	UFUNCTION()
	void HandleSelectThird();

	UFUNCTION()
	void HandleSelectFourth();

	UFUNCTION()
	void HandleSelectFifth();

	UFUNCTION()
	void HandlePurchaseSelected();

	UFUNCTION()
	void HandleCloseShop();

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
	TObjectPtr<UTexture2D> PanelTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PurchaseButtonTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SealTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RowDividerTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ItemTitles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DescriptionTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PureInkCostTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHorizontalBox>> CostRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SoldOutTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> OfferRowButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> OfferRowCards;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PurchaseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PurchaseButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CloseButtonText;

	TArray<FName> DisplayedItemIds;
	int32 SelectedOfferIndex = INDEX_NONE;
	bool bShopEventSubscribed = false;
	bool bEconomyEventSubscribed = false;
};

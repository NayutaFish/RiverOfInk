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
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	static constexpr int32 VisibleOfferCount = 5;

	/** 十字键选中商品时的缩放脉冲时长（秒）、峰值幅度、推进步长。 */
	static constexpr float OfferSelectionPulseDuration = 0.16f;
	static constexpr float OfferSelectionPulseScale = 0.05f;
	static constexpr float OfferSelectionPulseStep = 1.0f / 60.0f;

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

	// ── 手柄 / 键盘导航 ──

	/** 识别十字键、上下键、确认键、退出键；已处理返回 true（吃掉事件，避免 Slate 再自己导航一次）。 */
	bool HandleNavigationKey(const FKey& Key);

	/** 该行现在能不能选（有商品、没售罄、按钮可用）。 */
	bool IsOfferSelectable(int32 SlotIndex) const;

	/** 上下移动商品选择（跳过售罄/不可用行，到边界就停住）。 */
	bool MoveOfferSelection(int32 Delta);

	/** 给选中行一个短促的缩放脉冲（纯代码动画，不依赖 Tick）。 */
	void PlayOfferSelectionPulse(int32 SlotIndex);

	/** 脉冲推进：0 → 峰值 → 1.0，走完把计时器停掉。 */
	void HandleOfferSelectionPulseTick();

	FTimerHandle OfferSelectionPulseTimer;
	int32 OfferSelectionPulseIndex = INDEX_NONE;
	float OfferSelectionPulseElapsed = 0.0f;

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

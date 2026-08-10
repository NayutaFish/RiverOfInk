// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RoguelikeShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"
#include "RoguelikeSystem/RoguelikeShopManager.h"

namespace
{
	FLinearColor GetOfferColor(int32 SlotIndex)
	{
		static const FLinearColor OfferColors[] =
		{
			FLinearColor(0.12f, 0.68f, 0.78f, 1.0f),
			FLinearColor(0.42f, 0.36f, 0.92f, 1.0f),
			FLinearColor(0.86f, 0.42f, 0.16f, 1.0f)
		};
		return OfferColors[FMath::Clamp(SlotIndex, 0, UE_ARRAY_COUNT(OfferColors) - 1)];
	}

	void SetTextStyle(UTextBlock* TextBlock, int32 FontSize, const FLinearColor& Color, ETextJustify::Type Justification = ETextJustify::Center)
	{
		if (!TextBlock)
		{
			return;
		}

		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetJustification(Justification);
	}
}

TSharedRef<SWidget> URoguelikeShopWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void URoguelikeShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildDefaultWidgetTree();
	BindShopEvents();
	RefreshShop();
}

void URoguelikeShopWidget::NativeDestruct()
{
	UnbindShopEvents();
	Super::NativeDestruct();
}

FReply URoguelikeShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (ObservedShopManager)
		{
			ObservedShopManager->CloseShop();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void URoguelikeShopWidget::InitializeForShop(ARoguelikeShopManager* InShopManager)
{
	UnbindShopEvents();
	ObservedShopManager = InShopManager;
	UGameInstance* GameInstance = GetGameInstance();
	ObservedEconomy = GameInstance
		? GameInstance->GetSubsystem<URoguelikeEconomySubsystem>()
		: nullptr;
	BindShopEvents();
	RefreshShop();
}

void URoguelikeShopWidget::FocusFirstPurchase()
{
	for (UButton* BuyButton : BuyButtons)
	{
		if (BuyButton && BuyButton->GetIsEnabled() && BuyButton->GetVisibility() == ESlateVisibility::Visible)
		{
			BuyButton->SetKeyboardFocus();
			return;
		}
	}

	SetKeyboardFocus();
}

void URoguelikeShopWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.005f, 0.01f, 0.03f, 0.78f));
	if (UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop))
	{
		BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackdropSlot->SetOffsets(FMargin(0.0f));
	}

	UScaleBox* ScreenScaler = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ShopScreenScaler"));
	ScreenScaler->SetStretch(EStretch::ScaleToFit);
	if (UCanvasPanelSlot* ScaleSlot = RootCanvas->AddChildToCanvas(ScreenScaler))
	{
		ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ScaleSlot->SetOffsets(FMargin(24.0f));
	}

	USizeBox* ReferenceSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopReferenceSize"));
	ReferenceSize->SetWidthOverride(1280.0f);
	ReferenceSize->SetHeightOverride(720.0f);
	ScreenScaler->SetContent(ReferenceSize);

	UCanvasPanel* ReferenceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopReferenceCanvas"));
	ReferenceSize->SetContent(ReferenceCanvas);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPanel"));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.07f, 0.14f, 0.98f));
	Panel->SetPadding(FMargin(42.0f, 30.0f));
	if (UCanvasPanelSlot* PanelSlot = ReferenceCanvas->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetSize(FVector2D(1180.0f, 620.0f));
	}

	UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopPanelContent"));
	Panel->SetContent(PanelContent);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopHeader"));
	if (UVerticalBoxSlot* HeaderSlot = PanelContent->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopTitle"));
	Title->SetText(FText::FromString(TEXT("INK EXCHANGE")));
	SetTextStyle(Title, 38, FLinearColor(0.72f, 0.9f, 1.0f, 1.0f), ETextJustify::Left);
	if (UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	UHorizontalBox* BalanceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PureInkBalanceRow"));
	if (UHorizontalBoxSlot* BalanceSlot = Header->AddChildToHorizontalBox(BalanceRow))
	{
		BalanceSlot->SetVerticalAlignment(VAlign_Center);
		BalanceSlot->SetPadding(FMargin(24.0f, 0.0f));
	}

	InkIconTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	UImage* BalanceInkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PureInkImage"));
	BalanceInkImage->SetBrushFromTexture(InkIconTexture, true);
	BalanceInkImage->SetColorAndOpacity(FLinearColor(0.15f, 0.82f, 1.0f, 1.0f));
	USizeBox* BalanceInkSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PureInkImageSize"));
	BalanceInkSize->SetWidthOverride(28.0f);
	BalanceInkSize->SetHeightOverride(28.0f);
	BalanceInkSize->SetContent(BalanceInkImage);
	BalanceRow->AddChildToHorizontalBox(BalanceInkSize);

	BalanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PureInkBalanceText"));
	BalanceText->SetText(FText::FromString(TEXT("Pure Ink: 0")));
	SetTextStyle(BalanceText, 23, FLinearColor(0.86f, 0.95f, 1.0f, 1.0f));
	if (UHorizontalBoxSlot* BalanceTextSlot = BalanceRow->AddChildToHorizontalBox(BalanceText))
	{
		BalanceTextSlot->SetVerticalAlignment(VAlign_Center);
		BalanceTextSlot->SetPadding(FMargin(10.0f, 0.0f));
	}

	UTextBlock* CloseHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopCloseHint"));
	CloseHint->SetText(FText::FromString(TEXT("ESC  Close")));
	SetTextStyle(CloseHint, 18, FLinearColor(0.6f, 0.72f, 0.84f, 1.0f));
	if (UHorizontalBoxSlot* CloseSlot = Header->AddChildToHorizontalBox(CloseHint))
	{
		CloseSlot->SetVerticalAlignment(VAlign_Center);
	}

	UHorizontalBox* OfferRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopOfferRow"));
	if (UVerticalBoxSlot* OfferRowSlot = PanelContent->AddChildToVerticalBox(OfferRow))
	{
		OfferRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	for (int32 SlotIndex = 0; SlotIndex < VisibleOfferCount; ++SlotIndex)
	{
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dSize"), SlotIndex));
		CardSize->SetWidthOverride(340.0f);
		if (UHorizontalBoxSlot* CardSizeSlot = OfferRow->AddChildToHorizontalBox(CardSize))
		{
			CardSizeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CardSizeSlot->SetPadding(FMargin(9.0f));
		}

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("ShopItem%dCard"), SlotIndex));
		Card->SetBrushColor(FLinearColor(0.04f, 0.12f, 0.22f, 1.0f));
		Card->SetPadding(FMargin(20.0f, 18.0f));
		CardSize->SetContent(Card);
		OfferCards.Add(Card);

		UVerticalBox* CardContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dContent"), SlotIndex));
		Card->SetContent(CardContent);

		UImage* ItemImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("ShopItem%dImage"), SlotIndex));
		ItemImage->SetBrushFromTexture(InkIconTexture, true);
		ItemImage->SetColorAndOpacity(GetOfferColor(SlotIndex));
		USizeBox* ItemImageSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dImageSize"), SlotIndex));
		ItemImageSize->SetWidthOverride(92.0f);
		ItemImageSize->SetHeightOverride(92.0f);
		ItemImageSize->SetContent(ItemImage);
		if (UVerticalBoxSlot* ItemImageSlot = CardContent->AddChildToVerticalBox(ItemImageSize))
		{
			ItemImageSlot->SetHorizontalAlignment(HAlign_Center);
			ItemImageSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 12.0f));
		}
		ItemImages.Add(ItemImage);

		UTextBlock* ItemTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ShopItem%dTitle"), SlotIndex));
		SetTextStyle(ItemTitle, 23, FLinearColor::White);
		if (UVerticalBoxSlot* TitleSlot = CardContent->AddChildToVerticalBox(ItemTitle))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
		ItemTitles.Add(ItemTitle);

		UTextBlock* DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ShopItem%dDescriptionText"), SlotIndex));
		DescriptionText->SetAutoWrapText(true);
		DescriptionText->SetWrapTextAt(290.0f);
		SetTextStyle(DescriptionText, 16, FLinearColor(0.72f, 0.82f, 0.92f, 1.0f));
		if (UVerticalBoxSlot* DescriptionSlot = CardContent->AddChildToVerticalBox(DescriptionText))
		{
			DescriptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			DescriptionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		}
		DescriptionTexts.Add(DescriptionText);

		UHorizontalBox* CostRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dPureInkRow"), SlotIndex));
		if (UVerticalBoxSlot* CostRowSlot = CardContent->AddChildToVerticalBox(CostRow))
		{
			CostRowSlot->SetHorizontalAlignment(HAlign_Center);
			CostRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
		}

		UImage* PureInkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("ShopItem%dPureInkImage"), SlotIndex));
		PureInkImage->SetBrushFromTexture(InkIconTexture, true);
		PureInkImage->SetColorAndOpacity(FLinearColor(0.15f, 0.82f, 1.0f, 1.0f));
		USizeBox* PureInkImageSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dPureInkImageSize"), SlotIndex));
		PureInkImageSize->SetWidthOverride(24.0f);
		PureInkImageSize->SetHeightOverride(24.0f);
		PureInkImageSize->SetContent(PureInkImage);
		CostRow->AddChildToHorizontalBox(PureInkImageSize);

		UTextBlock* PureInkCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ShopItem%dPureInkCostText"), SlotIndex));
		SetTextStyle(PureInkCostText, 19, FLinearColor(0.72f, 0.92f, 1.0f, 1.0f));
		if (UHorizontalBoxSlot* CostTextSlot = CostRow->AddChildToHorizontalBox(PureInkCostText))
		{
			CostTextSlot->SetVerticalAlignment(VAlign_Center);
			CostTextSlot->SetPadding(FMargin(8.0f, 0.0f));
		}
		PureInkCostTexts.Add(PureInkCostText);

		UButton* BuyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("ShopItem%dBuyButton"), SlotIndex));
		BuyButton->SetBackgroundColor(GetOfferColor(SlotIndex));
		UTextBlock* BuyButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ShopItem%dBuyButtonText"), SlotIndex));
		SetTextStyle(BuyButtonText, 18, FLinearColor(0.02f, 0.04f, 0.08f, 1.0f));
		BuyButton->SetContent(BuyButtonText);
		if (UVerticalBoxSlot* BuyButtonSlot = CardContent->AddChildToVerticalBox(BuyButton))
		{
			BuyButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		BuyButtons.Add(BuyButton);
		BuyButtonTexts.Add(BuyButtonText);
	}

	if (BuyButtons.IsValidIndex(0))
	{
		BuyButtons[0]->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleBuyFirst);
	}
	if (BuyButtons.IsValidIndex(1))
	{
		BuyButtons[1]->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleBuySecond);
	}
	if (BuyButtons.IsValidIndex(2))
	{
		BuyButtons[2]->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleBuyThird);
	}

	FeedbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopFeedbackText"));
	FeedbackText->SetText(FText::FromString(TEXT("Choose an item. Purchased items remain sold out.")));
	SetTextStyle(FeedbackText, 17, FLinearColor(0.68f, 0.8f, 0.94f, 1.0f));
	if (UVerticalBoxSlot* FeedbackSlot = PanelContent->AddChildToVerticalBox(FeedbackText))
	{
		FeedbackSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
	}
}

void URoguelikeShopWidget::BindShopEvents()
{
	if (!bShopEventSubscribed && ObservedShopManager)
	{
		ObservedShopManager->OnPurchaseCompleted.AddDynamic(this, &URoguelikeShopWidget::HandlePurchaseCompleted);
		bShopEventSubscribed = true;
	}

	if (!bEconomyEventSubscribed && ObservedEconomy)
	{
		ObservedEconomy->OnPureInkChanged.AddDynamic(this, &URoguelikeShopWidget::HandlePureInkChanged);
		bEconomyEventSubscribed = true;
	}
}

void URoguelikeShopWidget::UnbindShopEvents()
{
	if (bShopEventSubscribed && ObservedShopManager)
	{
		ObservedShopManager->OnPurchaseCompleted.RemoveDynamic(this, &URoguelikeShopWidget::HandlePurchaseCompleted);
	}
	if (bEconomyEventSubscribed && ObservedEconomy)
	{
		ObservedEconomy->OnPureInkChanged.RemoveDynamic(this, &URoguelikeShopWidget::HandlePureInkChanged);
	}

	bShopEventSubscribed = false;
	bEconomyEventSubscribed = false;
}

void URoguelikeShopWidget::RefreshShop()
{
	if (BalanceText)
	{
		const int32 Balance = ObservedShopManager ? ObservedShopManager->GetCurrentPureInkBalance() : 0;
		BalanceText->SetText(FText::Format(FText::FromString(TEXT("Pure Ink: {0}")), Balance));
	}

	DisplayedItemIds.SetNum(VisibleOfferCount);
	for (int32 SlotIndex = 0; SlotIndex < VisibleOfferCount; ++SlotIndex)
	{
		RefreshOfferSlot(SlotIndex);
	}
}

void URoguelikeShopWidget::RefreshOfferSlot(int32 SlotIndex)
{
	if (!ItemTitles.IsValidIndex(SlotIndex) || !DescriptionTexts.IsValidIndex(SlotIndex)
		|| !PureInkCostTexts.IsValidIndex(SlotIndex) || !BuyButtons.IsValidIndex(SlotIndex)
		|| !BuyButtonTexts.IsValidIndex(SlotIndex))
	{
		return;
	}

	const TArray<FShopItemDefinition> Offers = ObservedShopManager
		? ObservedShopManager->GetShopItems()
		: TArray<FShopItemDefinition>();
	const bool bHasOffer = Offers.IsValidIndex(SlotIndex);
	const FShopItemDefinition* Offer = bHasOffer ? &Offers[SlotIndex] : nullptr;
	DisplayedItemIds[SlotIndex] = Offer ? Offer->ItemId : NAME_None;

	if (!Offer)
	{
		ItemTitles[SlotIndex]->SetText(FText::FromString(TEXT("Empty Offer")));
		DescriptionTexts[SlotIndex]->SetText(FText::FromString(TEXT("No item configured for this Shop slot.")));
		PureInkCostTexts[SlotIndex]->SetText(FText::FromString(TEXT("—")));
		BuyButtonTexts[SlotIndex]->SetText(FText::FromString(TEXT("UNAVAILABLE")));
		BuyButtons[SlotIndex]->SetIsEnabled(false);
		return;
	}

	const bool bSoldOut = ObservedShopManager->IsItemPurchased(Offer->ItemId);
	const bool bCanPurchase = ObservedShopManager->CanPurchaseItem(Offer->ItemId);
	const int32 Balance = ObservedShopManager->GetCurrentPureInkBalance();

	ItemTitles[SlotIndex]->SetText(Offer->Title);
	DescriptionTexts[SlotIndex]->SetText(Offer->Description);
	PureInkCostTexts[SlotIndex]->SetText(FText::Format(FText::FromString(TEXT("{0} Pure Ink")), Offer->Cost));
	BuyButtons[SlotIndex]->SetIsEnabled(bCanPurchase);
	BuyButtons[SlotIndex]->SetBackgroundColor(bCanPurchase
		? GetOfferColor(SlotIndex)
		: FLinearColor(0.18f, 0.22f, 0.28f, 1.0f));

	if (bSoldOut)
	{
		BuyButtonTexts[SlotIndex]->SetText(FText::FromString(TEXT("SOLD OUT")));
	}
	else if (Balance < Offer->Cost)
	{
		BuyButtonTexts[SlotIndex]->SetText(FText::FromString(TEXT("NEED INK")));
	}
	else if (!bCanPurchase)
	{
		BuyButtonTexts[SlotIndex]->SetText(FText::FromString(TEXT("UNAVAILABLE")));
	}
	else
	{
		BuyButtonTexts[SlotIndex]->SetText(FText::FromString(TEXT("BUY")));
	}
}

void URoguelikeShopWidget::TryPurchaseSlot(int32 SlotIndex)
{
	if (!ObservedShopManager || !DisplayedItemIds.IsValidIndex(SlotIndex) || DisplayedItemIds[SlotIndex].IsNone())
	{
		SetFeedbackText(FText::FromString(TEXT("This offer is unavailable.")), FLinearColor(1.0f, 0.58f, 0.38f, 1.0f));
		return;
	}

	const FName ItemId = DisplayedItemIds[SlotIndex];
	if (ObservedShopManager->PurchaseItem(ItemId))
	{
		SetFeedbackText(FText::FromString(TEXT("Purchase complete.")), FLinearColor(0.34f, 1.0f, 0.72f, 1.0f));
	}
	else
	{
		SetFeedbackText(FText::FromString(TEXT("Purchase unavailable: check your health, ink, or sold-out state.")), FLinearColor(1.0f, 0.58f, 0.38f, 1.0f));
	}
	RefreshShop();
}

void URoguelikeShopWidget::SetFeedbackText(const FText& InText, const FLinearColor& InColor)
{
	if (FeedbackText)
	{
		FeedbackText->SetText(InText);
		FeedbackText->SetColorAndOpacity(FSlateColor(InColor));
	}
}

void URoguelikeShopWidget::HandleBuyFirst()
{
	TryPurchaseSlot(0);
}

void URoguelikeShopWidget::HandleBuySecond()
{
	TryPurchaseSlot(1);
}

void URoguelikeShopWidget::HandleBuyThird()
{
	TryPurchaseSlot(2);
}

void URoguelikeShopWidget::HandlePurchaseCompleted(FName ItemId, int32 Cost, int32 NewBalance)
{
	SetFeedbackText(
		FText::Format(FText::FromString(TEXT("Purchased {0} for {1} Pure Ink. Balance: {2}.")), FText::FromName(ItemId), Cost, NewBalance),
		FLinearColor(0.34f, 1.0f, 0.72f, 1.0f));
	RefreshShop();
}

void URoguelikeShopWidget::HandlePureInkChanged(int32 PreviousBalance, int32 NewBalance, int32 Delta, EPureInkChangeReason Reason)
{
	(void)PreviousBalance;
	(void)NewBalance;
	(void)Delta;
	(void)Reason;
	RefreshShop();
}

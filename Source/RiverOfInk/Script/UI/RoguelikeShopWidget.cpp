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
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "RoguelikeSystem/RoguelikeEconomySubsystem.h"
#include "RoguelikeSystem/RoguelikeShopManager.h"

namespace
{
	const TCHAR* ShopUiFontPath = TEXT(
		"/Game/RawContent/UI/Fonts/AaGuDianKeBenSongYouMoBan_2_Font.AaGuDianKeBenSongYouMoBan_2_Font");

	UTexture2D* LoadShopTexture(const TCHAR* ObjectPath)
	{
		return LoadObject<UTexture2D>(nullptr, ObjectPath);
	}

	void SetTextStyle(
		UTextBlock* TextBlock,
		int32 FontSize,
		const FLinearColor& Color,
		ETextJustify::Type Justification = ETextJustify::Center)
	{
		if (!TextBlock)
		{
			return;
		}

		FSlateFontInfo Font = TextBlock->GetFont();
		if (UFont* ShopUiFont = LoadObject<UFont>(nullptr, ShopUiFontPath))
		{
			Font.FontObject = ShopUiFont;
		}
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetJustification(Justification);
	}

	FLinearColor GetRowColor(bool bSelected)
	{
		return bSelected
			? FLinearColor(0.20f, 0.13f, 0.08f, 0.16f)
			: FLinearColor(0.10f, 0.07f, 0.04f, 0.035f);
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
	SelectedOfferIndex = INDEX_NONE;
	BindShopEvents();
	RefreshShop();
}

void URoguelikeShopWidget::FocusFirstPurchase()
{
	if (SelectedOfferIndex == INDEX_NONE)
	{
		RefreshShop();
	}

	if (OfferRowButtons.IsValidIndex(SelectedOfferIndex))
	{
		UButton* SelectedButton = OfferRowButtons[SelectedOfferIndex];
		if (SelectedButton && SelectedButton->GetIsEnabled() && SelectedButton->GetVisibility() == ESlateVisibility::Visible)
		{
			SelectedButton->SetKeyboardFocus();
			return;
		}
	}

	if (PurchaseButton && PurchaseButton->GetIsEnabled())
	{
		PurchaseButton->SetKeyboardFocus();
		return;
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
	Backdrop->SetBrushColor(FLinearColor(0.005f, 0.004f, 0.003f, 0.34f));
	Backdrop->SetVisibility(ESlateVisibility::Visible);
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
	ReferenceSize->SetWidthOverride(1483.0f);
	ReferenceSize->SetHeightOverride(1061.0f);
	ScreenScaler->SetContent(ReferenceSize);

	UCanvasPanel* ReferenceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopReferenceCanvas"));
	ReferenceSize->SetContent(ReferenceCanvas);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPanel"));
	// The imported panel already contains the paper and ink-wash silhouette.
	// Keep the widget background transparent so its alpha fringe is not boxed
	// in by an opaque cream rectangle.
	Panel->SetBrushColor(FLinearColor::Transparent);
	Panel->SetPadding(FMargin(0.0f));
	if (UCanvasPanelSlot* PanelSlot = ReferenceCanvas->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetSize(FVector2D(670.0f, 960.0f));
	}

	PanelTexture = LoadShopTexture(TEXT("/Game/RawContent/UI/Shop/T_UI_ShopHUD_Panel.T_UI_ShopHUD_Panel"));
	FooterTexture = LoadShopTexture(TEXT("/Game/RawContent/UI/Shop/T_UI_ShopHUD_LandscapeFooter.T_UI_ShopHUD_LandscapeFooter"));
	PurchaseButtonTexture = LoadShopTexture(TEXT("/Game/RawContent/UI/Shop/T_UI_ShopHUD_PurchaseButton.T_UI_ShopHUD_PurchaseButton"));
	SealTexture = LoadShopTexture(TEXT("/Game/RawContent/UI/Shop/T_UI_ShopHUD_Seal.T_UI_ShopHUD_Seal"));
	RowDividerTexture = LoadShopTexture(TEXT("/Game/RawContent/UI/Shop/T_UI_ShopHUD_RowDivider.T_UI_ShopHUD_RowDivider"));
	InkIconTexture = LoadShopTexture(TEXT("/Game/RawContent/UI/Shop/T_UI_ShopHUD_PureInkDrop.T_UI_ShopHUD_PureInkDrop"));
	if (!RowDividerTexture)
	{
		RowDividerTexture = LoadShopTexture(TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_SmallDivider.T_UI_Reward_SmallDivider"));
	}
	if (!InkIconTexture)
	{
		InkIconTexture = LoadShopTexture(TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_PureInk.T_UI_Reward_PureInk"));
	}
	if (!InkIconTexture)
	{
		InkIconTexture = LoadShopTexture(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	}

	UOverlay* PanelOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ShopPanelOverlay"));
	Panel->SetContent(PanelOverlay);

	if (PanelTexture)
	{
		UImage* PanelImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopPanelImage"));
		PanelImage->SetBrushFromTexture(PanelTexture, true);
		PanelImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* PanelImageSlot = PanelOverlay->AddChildToOverlay(PanelImage))
		{
			PanelImageSlot->SetHorizontalAlignment(HAlign_Fill);
			PanelImageSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	if (FooterTexture)
	{
		UImage* FooterImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopLandscapeFooter"));
		FooterImage->SetBrushFromTexture(FooterTexture, true);
		FooterImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		USizeBox* FooterSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopLandscapeFooterSize"));
		FooterSize->SetHeightOverride(150.0f);
		FooterSize->SetContent(FooterImage);
		if (UOverlaySlot* FooterSlot = PanelOverlay->AddChildToOverlay(FooterSize))
		{
			FooterSlot->SetHorizontalAlignment(HAlign_Fill);
			FooterSlot->SetVerticalAlignment(VAlign_Bottom);
		}
	}

	UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopPanelContent"));
	if (UOverlaySlot* ContentSlot = PanelOverlay->AddChildToOverlay(PanelContent))
	{
		ContentSlot->SetPadding(FMargin(92.0f, 90.0f, 92.0f, 34.0f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopHeader"));
	if (UVerticalBoxSlot* HeaderSlot = PanelContent->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopTitle"));
	Title->SetText(FText::FromString(TEXT("墨铺")));
	SetTextStyle(Title, 34, FLinearColor(0.12f, 0.10f, 0.08f, 1.0f), ETextJustify::Left);
	if (UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	UHorizontalBox* BalanceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PureInkBalanceRow"));
	if (UHorizontalBoxSlot* BalanceSlot = Header->AddChildToHorizontalBox(BalanceRow))
	{
		BalanceSlot->SetVerticalAlignment(VAlign_Center);
		BalanceSlot->SetPadding(FMargin(12.0f, 0.0f, 8.0f, 0.0f));
	}

	UImage* BalanceInkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PureInkImage"));
	BalanceInkImage->SetBrushFromTexture(InkIconTexture, true);
	BalanceInkImage->SetColorAndOpacity(FLinearColor(0.08f, 0.07f, 0.06f, 1.0f));
	USizeBox* BalanceInkSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PureInkImageSize"));
	BalanceInkSize->SetWidthOverride(24.0f);
	BalanceInkSize->SetHeightOverride(24.0f);
	BalanceInkSize->SetContent(BalanceInkImage);
	BalanceRow->AddChildToHorizontalBox(BalanceInkSize);

	BalanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PureInkBalanceText"));
	BalanceText->SetText(FText::FromString(TEXT("纯墨 0")));
	SetTextStyle(BalanceText, 22, FLinearColor(0.12f, 0.10f, 0.08f, 1.0f));
	if (UHorizontalBoxSlot* BalanceTextSlot = BalanceRow->AddChildToHorizontalBox(BalanceText))
	{
		BalanceTextSlot->SetVerticalAlignment(VAlign_Center);
		BalanceTextSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	}

	if (SealTexture)
	{
		UImage* SealImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopSealImage"));
		SealImage->SetBrushFromTexture(SealTexture, true);
		USizeBox* SealSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopSealSize"));
		SealSize->SetWidthOverride(36.0f);
		SealSize->SetHeightOverride(36.0f);
		SealSize->SetContent(SealImage);
		Header->AddChildToHorizontalBox(SealSize);
	}

	// The reference composition leaves a deliberate breathing space between
	// the balance header and the first offer row. Keep it as a real layout slot
	// so the five-row stack does not jump when a row becomes sold out.
	USizeBox* OfferTopSpacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopOfferTopSpacer"));
	OfferTopSpacer->SetHeightOverride(100.0f);
	if (UVerticalBoxSlot* SpacerSlot = PanelContent->AddChildToVerticalBox(OfferTopSpacer))
	{
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UVerticalBox* OfferList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopOfferList"));
	if (UVerticalBoxSlot* OfferListSlot = PanelContent->AddChildToVerticalBox(OfferList))
	{
		OfferListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	for (int32 SlotIndex = 0; SlotIndex < VisibleOfferCount; ++SlotIndex)
	{
		USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dSize"), SlotIndex));
		RowSize->SetHeightOverride(94.0f);
		if (UVerticalBoxSlot* RowSizeSlot = OfferList->AddChildToVerticalBox(RowSize))
		{
			RowSizeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			RowSizeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}

		UButton* RowButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("ShopItem%dRowButton"), SlotIndex));
		RowButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

		UBorder* RowCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("ShopItem%dRowCard"), SlotIndex));
		RowCard->SetBrushColor(GetRowColor(false));
		RowCard->SetPadding(FMargin(14.0f, 8.0f));

		UOverlay* RowOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("ShopItem%dRowOverlay"), SlotIndex));
		RowCard->SetContent(RowOverlay);

		UHorizontalBox* RowContent = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dRowContent"), SlotIndex));
		if (UOverlaySlot* RowContentSlot = RowOverlay->AddChildToOverlay(RowContent))
		{
			RowContentSlot->SetHorizontalAlignment(HAlign_Fill);
			RowContentSlot->SetVerticalAlignment(VAlign_Center);
		}

		UVerticalBox* OfferTextStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dTextStack"), SlotIndex));
		USizeBox* OfferTextSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dTextSize"), SlotIndex));
		OfferTextSize->SetWidthOverride(340.0f);
		OfferTextSize->SetContent(OfferTextStack);
		if (UHorizontalBoxSlot* OfferTextSlot = RowContent->AddChildToHorizontalBox(OfferTextSize))
		{
			OfferTextSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* ItemTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ShopItem%dTitle"), SlotIndex));
		SetTextStyle(ItemTitle, 24, FLinearColor(0.12f, 0.10f, 0.08f, 1.0f), ETextJustify::Left);
		if (UVerticalBoxSlot* ItemTitleSlot = OfferTextStack->AddChildToVerticalBox(ItemTitle))
		{
			ItemTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}
		ItemTitles.Add(ItemTitle);

		UTextBlock* DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ShopItem%dDescriptionText"), SlotIndex));
		DescriptionText->SetAutoWrapText(true);
		DescriptionText->SetWrapTextAt(340.0f);
		SetTextStyle(DescriptionText, 18, FLinearColor(0.25f, 0.22f, 0.18f, 1.0f), ETextJustify::Left);
		USizeBox* DescriptionSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dDescriptionSize"), SlotIndex));
		DescriptionSize->SetWidthOverride(340.0f);
		DescriptionSize->SetContent(DescriptionText);
		OfferTextStack->AddChildToVerticalBox(DescriptionSize);
		DescriptionTexts.Add(DescriptionText);

		UHorizontalBox* CostRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dPureInkRow"), SlotIndex));
		if (UHorizontalBoxSlot* CostRowSlot = RowContent->AddChildToHorizontalBox(CostRow))
		{
			CostRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CostRowSlot->SetHorizontalAlignment(HAlign_Right);
			CostRowSlot->SetVerticalAlignment(VAlign_Center);
		}
		CostRows.Add(CostRow);

		UImage* PureInkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("ShopItem%dPureInkImage"), SlotIndex));
		PureInkImage->SetBrushFromTexture(InkIconTexture, true);
		PureInkImage->SetColorAndOpacity(FLinearColor(0.08f, 0.07f, 0.06f, 1.0f));
		USizeBox* PureInkImageSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dPureInkImageSize"), SlotIndex));
		PureInkImageSize->SetWidthOverride(20.0f);
		PureInkImageSize->SetHeightOverride(20.0f);
		PureInkImageSize->SetContent(PureInkImage);
		CostRow->AddChildToHorizontalBox(PureInkImageSize);

		UTextBlock* PureInkCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ShopItem%dPureInkCostText"), SlotIndex));
		SetTextStyle(PureInkCostText, 20, FLinearColor(0.12f, 0.10f, 0.08f, 1.0f), ETextJustify::Right);
		if (UHorizontalBoxSlot* CostTextSlot = CostRow->AddChildToHorizontalBox(PureInkCostText))
		{
			CostTextSlot->SetVerticalAlignment(VAlign_Center);
			CostTextSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
		}
		PureInkCostTexts.Add(PureInkCostText);

		UTextBlock* SoldOutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("ShopItem%dSoldOutText"), SlotIndex));
		SoldOutText->SetText(FText::FromString(TEXT("售罄")));
		SetTextStyle(SoldOutText, 23, FLinearColor(0.28f, 0.25f, 0.21f, 1.0f));
		SoldOutText->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* SoldOutSlot = RowOverlay->AddChildToOverlay(SoldOutText))
		{
			SoldOutSlot->SetHorizontalAlignment(HAlign_Center);
			SoldOutSlot->SetVerticalAlignment(VAlign_Center);
		}
		SoldOutTexts.Add(SoldOutText);

		RowButton->SetContent(RowCard);
		RowSize->SetContent(RowButton);
		OfferRowButtons.Add(RowButton);
		OfferRowCards.Add(RowCard);

		switch (SlotIndex)
		{
		case 0:
			RowButton->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleSelectFirst);
			break;
		case 1:
			RowButton->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleSelectSecond);
			break;
		case 2:
			RowButton->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleSelectThird);
			break;
		case 3:
			RowButton->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleSelectFourth);
			break;
		case 4:
			RowButton->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleSelectFifth);
			break;
		default:
			break;
		}

		if (SlotIndex < VisibleOfferCount - 1)
		{
			USizeBox* DividerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ShopItem%dDividerSize"), SlotIndex));
			DividerSize->SetHeightOverride(1.0f);
			if (RowDividerTexture)
			{
				UImage* DividerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("ShopItem%dDivider"), SlotIndex));
				DividerImage->SetBrushFromTexture(RowDividerTexture, true);
				DividerImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.42f));
				DividerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
				DividerSize->SetContent(DividerImage);
			}
			else
			{
				UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("ShopItem%dDivider"), SlotIndex));
				Divider->SetBrushColor(FLinearColor(0.20f, 0.17f, 0.13f, 0.28f));
				DividerSize->SetContent(Divider);
			}
			OfferList->AddChildToVerticalBox(DividerSize);
		}
	}

	USizeBox* PurchaseButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ShopPurchaseButtonSize"));
	PurchaseButtonSize->SetWidthOverride(440.0f);
	PurchaseButtonSize->SetHeightOverride(72.0f);
	PurchaseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopPurchaseButton"));
	PurchaseButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	UOverlay* PurchaseOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ShopPurchaseButtonOverlay"));
	if (PurchaseButtonTexture)
	{
		UImage* PurchaseImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopPurchaseButtonImage"));
		PurchaseImage->SetBrushFromTexture(PurchaseButtonTexture, true);
		PurchaseImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		PurchaseOverlay->AddChildToOverlay(PurchaseImage);
	}
	else
	{
		UBorder* PurchaseFallback = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPurchaseButtonFallback"));
		PurchaseFallback->SetBrushColor(FLinearColor(0.06f, 0.05f, 0.04f, 0.96f));
		PurchaseFallback->SetVisibility(ESlateVisibility::HitTestInvisible);
		PurchaseOverlay->AddChildToOverlay(PurchaseFallback);
	}
	PurchaseButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopPurchaseButtonText"));
	PurchaseButtonText->SetText(FText::FromString(TEXT("购买")));
	SetTextStyle(PurchaseButtonText, 27, FLinearColor(0.92f, 0.90f, 0.85f, 1.0f));
	if (UOverlaySlot* PurchaseTextSlot = PurchaseOverlay->AddChildToOverlay(PurchaseButtonText))
	{
		PurchaseTextSlot->SetHorizontalAlignment(HAlign_Center);
		PurchaseTextSlot->SetVerticalAlignment(VAlign_Center);
	}
	PurchaseButton->SetContent(PurchaseOverlay);
	PurchaseButton->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandlePurchaseSelected);
	PurchaseButtonSize->SetContent(PurchaseButton);
	if (UVerticalBoxSlot* PurchaseSlot = PanelContent->AddChildToVerticalBox(PurchaseButtonSize))
	{
		PurchaseSlot->SetHorizontalAlignment(HAlign_Center);
		PurchaseSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 4.0f));
	}

	FeedbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopFeedbackText"));
	FeedbackText->SetText(FText::GetEmpty());
	SetTextStyle(FeedbackText, 14, FLinearColor(0.30f, 0.26f, 0.22f, 1.0f));
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* FeedbackSlot = PanelContent->AddChildToVerticalBox(FeedbackText))
	{
		FeedbackSlot->SetHorizontalAlignment(HAlign_Center);
		FeedbackSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 2.0f));
	}

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopCloseButton"));
	CloseButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	CloseButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopCloseButtonText"));
	CloseButtonText->SetText(FText::FromString(TEXT("关闭")));
	SetTextStyle(CloseButtonText, 20, FLinearColor(0.28f, 0.25f, 0.21f, 1.0f));
	CloseButton->SetContent(CloseButtonText);
	CloseButton->OnClicked.AddDynamic(this, &URoguelikeShopWidget::HandleCloseShop);
	if (UVerticalBoxSlot* CloseSlot = PanelContent->AddChildToVerticalBox(CloseButton))
	{
		CloseSlot->SetHorizontalAlignment(HAlign_Center);
		CloseSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
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
		BalanceText->SetText(FText::Format(FText::FromString(TEXT("纯墨 {0}")), Balance));
	}

	const TArray<FShopItemDefinition> Offers = ObservedShopManager
		? ObservedShopManager->GetShopItems()
		: TArray<FShopItemDefinition>();

	DisplayedItemIds.SetNum(VisibleOfferCount);
	if (!Offers.IsValidIndex(SelectedOfferIndex)
		|| (ObservedShopManager && ObservedShopManager->IsItemPurchased(Offers[SelectedOfferIndex].ItemId)))
	{
		SelectedOfferIndex = FindFirstSelectableOffer(Offers);
	}

	for (int32 SlotIndex = 0; SlotIndex < VisibleOfferCount; ++SlotIndex)
	{
		RefreshOfferSlot(SlotIndex, Offers);
	}
	RefreshSelectionState();
}

void URoguelikeShopWidget::RefreshOfferSlot(int32 SlotIndex, const TArray<FShopItemDefinition>& Offers)
{
	if (!ItemTitles.IsValidIndex(SlotIndex)
		|| !DescriptionTexts.IsValidIndex(SlotIndex)
		|| !PureInkCostTexts.IsValidIndex(SlotIndex)
		|| !CostRows.IsValidIndex(SlotIndex)
		|| !SoldOutTexts.IsValidIndex(SlotIndex)
		|| !OfferRowButtons.IsValidIndex(SlotIndex))
	{
		return;
	}

	UTextBlock* ItemTitle = ItemTitles[SlotIndex];
	UTextBlock* DescriptionText = DescriptionTexts[SlotIndex];
	UTextBlock* PureInkCostText = PureInkCostTexts[SlotIndex];
	UHorizontalBox* CostRow = CostRows[SlotIndex];
	UTextBlock* SoldOutText = SoldOutTexts[SlotIndex];
	UButton* RowButton = OfferRowButtons[SlotIndex];
	if (!ItemTitle || !DescriptionText || !PureInkCostText || !CostRow || !SoldOutText || !RowButton)
	{
		return;
	}

	const FShopItemDefinition* Offer = Offers.IsValidIndex(SlotIndex) ? &Offers[SlotIndex] : nullptr;
	DisplayedItemIds[SlotIndex] = Offer ? Offer->ItemId : NAME_None;

	if (!Offer)
	{
		ItemTitle->SetText(FText::GetEmpty());
		DescriptionText->SetText(FText::GetEmpty());
		PureInkCostText->SetText(FText::GetEmpty());
		ItemTitle->SetVisibility(ESlateVisibility::Collapsed);
		DescriptionText->SetVisibility(ESlateVisibility::Collapsed);
		CostRow->SetVisibility(ESlateVisibility::Collapsed);
		SoldOutText->SetVisibility(ESlateVisibility::Collapsed);
		RowButton->SetVisibility(ESlateVisibility::Collapsed);
		RowButton->SetIsEnabled(false);
		return;
	}

	RowButton->SetVisibility(ESlateVisibility::Visible);
	const bool bSoldOut = ObservedShopManager && ObservedShopManager->IsItemPurchased(Offer->ItemId);
	if (bSoldOut)
	{
		ItemTitle->SetText(FText::GetEmpty());
		DescriptionText->SetText(FText::GetEmpty());
		PureInkCostText->SetText(FText::GetEmpty());
		ItemTitle->SetVisibility(ESlateVisibility::Collapsed);
		DescriptionText->SetVisibility(ESlateVisibility::Collapsed);
		CostRow->SetVisibility(ESlateVisibility::Collapsed);
		SoldOutText->SetVisibility(ESlateVisibility::Visible);
		RowButton->SetIsEnabled(false);
		return;
	}

	ItemTitle->SetText(Offer->Title);
	DescriptionText->SetText(Offer->Description);
	PureInkCostText->SetText(FText::AsNumber(Offer->Cost));
	ItemTitle->SetVisibility(ESlateVisibility::Visible);
	DescriptionText->SetVisibility(ESlateVisibility::Visible);
	CostRow->SetVisibility(ESlateVisibility::Visible);
	SoldOutText->SetVisibility(ESlateVisibility::Collapsed);
	RowButton->SetIsEnabled(true);
}

void URoguelikeShopWidget::RefreshSelectionState()
{
	for (int32 SlotIndex = 0; SlotIndex < OfferRowCards.Num(); ++SlotIndex)
	{
		UBorder* RowCard = OfferRowCards[SlotIndex];
		if (!RowCard)
		{
			continue;
		}

		const bool bSelected = SlotIndex == SelectedOfferIndex
			&& OfferRowButtons.IsValidIndex(SlotIndex)
			&& OfferRowButtons[SlotIndex]
			&& OfferRowButtons[SlotIndex]->GetIsEnabled();
		RowCard->SetBrushColor(GetRowColor(bSelected));
	}

	bool bCanPurchase = false;
	if (ObservedShopManager
		&& DisplayedItemIds.IsValidIndex(SelectedOfferIndex)
		&& !DisplayedItemIds[SelectedOfferIndex].IsNone())
	{
		bCanPurchase = ObservedShopManager->CanPurchaseItem(DisplayedItemIds[SelectedOfferIndex]);
	}

	if (PurchaseButton)
	{
		PurchaseButton->SetIsEnabled(bCanPurchase);
	}
	if (PurchaseButtonText)
	{
		PurchaseButtonText->SetColorAndOpacity(FSlateColor(
			bCanPurchase
				? FLinearColor(0.92f, 0.90f, 0.85f, 1.0f)
				: FLinearColor(0.55f, 0.53f, 0.49f, 1.0f)));
	}
}

void URoguelikeShopWidget::SelectOffer(int32 SlotIndex)
{
	if (!DisplayedItemIds.IsValidIndex(SlotIndex)
		|| DisplayedItemIds[SlotIndex].IsNone()
		|| !OfferRowButtons.IsValidIndex(SlotIndex)
		|| !OfferRowButtons[SlotIndex]
		|| !OfferRowButtons[SlotIndex]->GetIsEnabled())
	{
		return;
	}

	SelectedOfferIndex = SlotIndex;
	RefreshSelectionState();
}

int32 URoguelikeShopWidget::FindFirstSelectableOffer(const TArray<FShopItemDefinition>& Offers) const
{
	int32 FirstAvailableIndex = INDEX_NONE;
	for (int32 SlotIndex = 0; SlotIndex < Offers.Num() && SlotIndex < VisibleOfferCount; ++SlotIndex)
	{
		if (ObservedShopManager && ObservedShopManager->IsItemPurchased(Offers[SlotIndex].ItemId))
		{
			continue;
		}

		if (FirstAvailableIndex == INDEX_NONE)
		{
			FirstAvailableIndex = SlotIndex;
		}

		if (!ObservedShopManager || ObservedShopManager->CanPurchaseItem(Offers[SlotIndex].ItemId))
		{
			return SlotIndex;
		}
	}
	return FirstAvailableIndex;
}

void URoguelikeShopWidget::TryPurchaseSelected()
{
	if (!ObservedShopManager
		|| !DisplayedItemIds.IsValidIndex(SelectedOfferIndex)
		|| DisplayedItemIds[SelectedOfferIndex].IsNone())
	{
		SetFeedbackText(FText::FromString(TEXT("请选择商品。")), FLinearColor(0.68f, 0.34f, 0.20f, 1.0f));
		return;
	}

	const FName ItemId = DisplayedItemIds[SelectedOfferIndex];
	if (!ObservedShopManager->PurchaseItem(ItemId))
	{
		SetFeedbackText(FText::FromString(TEXT("暂不可购买：请检查生命、墨量或售罄状态。")), FLinearColor(0.68f, 0.34f, 0.20f, 1.0f));
		RefreshShop();
	}
}

void URoguelikeShopWidget::SetFeedbackText(const FText& InText, const FLinearColor& InColor)
{
	if (FeedbackText)
	{
		FeedbackText->SetText(InText);
		FeedbackText->SetColorAndOpacity(FSlateColor(InColor));
		FeedbackText->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void URoguelikeShopWidget::HandleSelectFirst()
{
	SelectOffer(0);
}

void URoguelikeShopWidget::HandleSelectSecond()
{
	SelectOffer(1);
}

void URoguelikeShopWidget::HandleSelectThird()
{
	SelectOffer(2);
}

void URoguelikeShopWidget::HandleSelectFourth()
{
	SelectOffer(3);
}

void URoguelikeShopWidget::HandleSelectFifth()
{
	SelectOffer(4);
}

void URoguelikeShopWidget::HandlePurchaseSelected()
{
	TryPurchaseSelected();
}

void URoguelikeShopWidget::HandleCloseShop()
{
	if (ObservedShopManager)
	{
		ObservedShopManager->CloseShop();
	}
}

void URoguelikeShopWidget::HandlePurchaseCompleted(FName ItemId, int32 Cost, int32 NewBalance)
{
	(void)ItemId;
	(void)Cost;
	SetFeedbackText(
		FText::Format(FText::FromString(TEXT("购买成功，剩余纯墨：{0}")), NewBalance),
		FLinearColor(0.25f, 0.43f, 0.20f, 1.0f));
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

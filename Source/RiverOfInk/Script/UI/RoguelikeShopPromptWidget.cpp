// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RoguelikeShopPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> URoguelikeShopPromptWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void URoguelikeShopPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildDefaultWidgetTree();
}

void URoguelikeShopPromptWidget::SetPromptText(const FText& InPromptText)
{
	if (PromptText)
	{
		PromptText->SetText(InPromptText);
	}
}

void URoguelikeShopPromptWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ShopPromptCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	PromptBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopInteractionPrompt"));
	PromptBorder->SetBrushColor(FLinearColor(0.02f, 0.06f, 0.12f, 0.92f));
	PromptBorder->SetPadding(FMargin(20.0f, 10.0f));
	if (UCanvasPanelSlot* PromptSlot = RootCanvas->AddChildToCanvas(PromptBorder))
	{
		PromptSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PromptSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PromptSlot->SetPosition(FVector2D(0.0f, 150.0f));
		PromptSlot->SetSize(FVector2D(440.0f, 58.0f));
	}

	PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopInteractionText"));
	PromptText->SetText(FText::FromString(TEXT("[ J ]  Talk to the Ink Trader")));
	PromptText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.92f, 1.0f, 1.0f)));
	PromptText->SetJustification(ETextJustify::Center);
	FSlateFontInfo Font = PromptText->GetFont();
	Font.Size = 21;
	PromptText->SetFont(Font);
	PromptBorder->SetContent(PromptText);
}

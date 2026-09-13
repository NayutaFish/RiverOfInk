// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRewardScrimWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace
{
	const TCHAR* DefaultTexturePath = TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");
}

TSharedRef<SWidget> URoguelikeRewardScrimWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void URoguelikeRewardScrimWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
	ApplyStyle();
}

void URoguelikeRewardScrimWidget::ConfigureScrim(float InBlurStrength)
{
	BlurStrength = FMath::Clamp(InBlurStrength, 0.0f, 100.0f);
	BuildWidgetTree();
	ApplyStyle();
}

void URoguelikeRewardScrimWidget::BuildWidgetTree()
{
	if (!WidgetTree || bNativeTreeBuilt)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RewardScrimRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	BackgroundBlur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("RewardBackgroundBlur"));
	BackgroundBlur->SetApplyAlphaToBlur(false);
	BackgroundBlur->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* BlurSlot = RootCanvas->AddChildToCanvas(BackgroundBlur))
	{
		BlurSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BlurSlot->SetOffsets(FMargin(0.0f));
		BlurSlot->SetZOrder(0);
	}

	BackgroundOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardScrimOverlay"));
	if (UTexture2D* DefaultTexture = LoadObject<UTexture2D>(nullptr, DefaultTexturePath))
	{
		BackgroundOverlay->SetBrushFromTexture(DefaultTexture, false);
	}
	BackgroundOverlay->SetColorAndOpacity(FLinearColor(0.08f, 0.07f, 0.055f, 0.22f));
	BackgroundOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(BackgroundOverlay))
	{
		OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		OverlaySlot->SetOffsets(FMargin(0.0f));
		OverlaySlot->SetZOrder(1);
	}

	bNativeTreeBuilt = true;
}

void URoguelikeRewardScrimWidget::ApplyStyle()
{
	if (BackgroundBlur)
	{
		BackgroundBlur->SetBlurStrength(FMath::Clamp(BlurStrength, 0.0f, 100.0f));
		BackgroundBlur->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (BackgroundOverlay)
	{
		BackgroundOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRewardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "RoguelikeSystem/RoguelikeRewardOptionWidget.h"

namespace
{
	const TCHAR* DefaultTexturePath = TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");
	const TCHAR* TitleDividerPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_TitleDivider.T_UI_Reward_TitleDivider");

	FVector2D GetTextureAspectSize(const UTexture2D* Texture, float DesiredWidth, const FVector2D& FallbackSize)
	{
		const float SafeWidth = FMath::Max(1.0f, DesiredWidth);
		if (Texture && Texture->GetSizeX() > 0 && Texture->GetSizeY() > 0)
		{
			return FVector2D(
				SafeWidth,
				SafeWidth * static_cast<float>(Texture->GetSizeY()) / static_cast<float>(Texture->GetSizeX()));
		}

		return FallbackSize;
	}
}

TSharedRef<SWidget> URoguelikeRewardWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void URoguelikeRewardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	BuildDefaultWidgetTree();
	ConfigureWidgetTree();
}

void URoguelikeRewardWidget::NativeDestruct()
{
	SelectionFinishedCallback.Unbind();
	for (URoguelikeRewardOptionWidget* OptionWidget : OptionWidgets)
	{
		if (OptionWidget)
		{
			OptionWidget->OnOptionClicked.Unbind();
			OptionWidget->OnOptionHovered.Unbind();
			OptionWidget->OnOptionUnhovered.Unbind();
		}
	}
	OptionWidgets.Empty();
	Super::NativeDestruct();
}

void URoguelikeRewardWidget::SetupRewardOptions(
	ARoguelikeRewardManager* InRewardManager,
	const TArray<FRoguelikeRewardOption>& InOptions)
{
	RewardManager = InRewardManager;
	RewardOptions = InOptions;
	bSelectionLocked = false;
	BuildDefaultWidgetTree();
	ConfigureWidgetTree();
	OnRewardOptionsSet(RewardOptions);

	if (!RewardOptionsRow)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Reward widget cannot create options: horizontal row is missing."));
		return;
	}

	RewardOptionsRow->ClearChildren();
	OptionWidgets.Empty();

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		UE_LOG(LogRoguelike, Error, TEXT("Reward widget cannot create options: owning PlayerController is missing."));
		return;
	}

	for (int32 OptionIndex = 0; OptionIndex < RewardOptions.Num(); ++OptionIndex)
	{
		URoguelikeRewardOptionWidget* OptionWidget = CreateWidget<URoguelikeRewardOptionWidget>(
			PlayerController,
			URoguelikeRewardOptionWidget::StaticClass());
		if (!OptionWidget)
		{
			UE_LOG(LogRoguelike, Error, TEXT("Reward option widget creation failed: Index=%d."), OptionIndex);
			continue;
		}

		// The option widget is a native runtime class, so copy the editor-tuned
		// values from the owning WBP before it builds its runtime animations and
		// dynamic brush material.
		OptionWidget->SelectionSweepDuration = SelectionSweepDuration;
		OptionWidget->SelectionHoldDuration = SelectionHoldDuration;
		OptionWidget->SelectionRevealSoftness = SelectionRevealSoftness;
		OptionWidget->FadeOutDuration = FadeOutDuration;
		OptionWidget->InitializeRewardOption(RewardOptions[OptionIndex], OptionIndex);
		OptionWidget->OnOptionClicked.BindUObject(this, &URoguelikeRewardWidget::SelectOption);
		OptionWidget->OnOptionHovered.BindUObject(this, &URoguelikeRewardWidget::HandleOptionHovered);
		OptionWidget->OnOptionUnhovered.BindUObject(this, &URoguelikeRewardWidget::HandleOptionUnhovered);

		if (UHorizontalBoxSlot* OptionSlot = RewardOptionsRow->AddChildToHorizontalBox(OptionWidget))
		{
			OptionSlot->SetPadding(FMargin(18.0f, 0.0f));
			OptionSlot->SetVerticalAlignment(VAlign_Center);
		}
		OptionWidgets.Add(OptionWidget);
	}

	for (int32 OptionIndex = 1; OptionIndex < OptionWidgets.Num(); ++OptionIndex)
	{
		UButton* PreviousButton = OptionWidgets[OptionIndex - 1]
			? OptionWidgets[OptionIndex - 1]->GetHitArea()
			: nullptr;
		UButton* CurrentButton = OptionWidgets[OptionIndex]
			? OptionWidgets[OptionIndex]->GetHitArea()
			: nullptr;
		if (PreviousButton && CurrentButton)
		{
			PreviousButton->SetNavigationRuleExplicit(EUINavigation::Right, CurrentButton);
			CurrentButton->SetNavigationRuleExplicit(EUINavigation::Left, PreviousButton);
		}
	}

	SetSelectionLocked(false);
	ForceLayoutPrepass();
	UE_LOG(LogRoguelike, Log, TEXT("Reward widget generated dynamically: Count=%d."), OptionWidgets.Num());
}

void URoguelikeRewardWidget::SelectOption(int32 OptionIndex)
{
	if (bSelectionLocked || !RewardOptions.IsValidIndex(OptionIndex) || !RewardManager)
	{
		return;
	}

	SetSelectionLocked(true);
	RewardManager->SelectReward(OptionIndex);
}

bool URoguelikeRewardWidget::PlaySelectionFeedback(int32 OptionIndex)
{
	if (!OptionWidgets.IsValidIndex(OptionIndex) || !OptionWidgets[OptionIndex])
	{
		return false;
	}

	SetSelectionLocked(true);
	for (int32 Index = 0; Index < OptionWidgets.Num(); ++Index)
	{
		URoguelikeRewardOptionWidget* OptionWidget = OptionWidgets[Index];
		if (!OptionWidget)
		{
			continue;
		}

		if (Index == OptionIndex)
		{
			OptionWidget->SetSelectionFinishedCallback(
				FRoguelikeRewardOptionFinishedDelegate::CreateUObject(
					this,
					&URoguelikeRewardWidget::HandleSelectionFinished));
		}
		else
		{
			OptionWidget->PlayFadeOut();
		}
	}

	return OptionWidgets[OptionIndex]->PlaySelectionFeedback();
}

void URoguelikeRewardWidget::SetSelectionLocked(bool bLocked)
{
	bSelectionLocked = bLocked;
	for (URoguelikeRewardOptionWidget* OptionWidget : OptionWidgets)
	{
		if (OptionWidget)
		{
			OptionWidget->SetInteractionEnabled(!bLocked);
		}
	}
}

void URoguelikeRewardWidget::FocusFirstOption()
{
	for (URoguelikeRewardOptionWidget* OptionWidget : OptionWidgets)
	{
		if (OptionWidget)
		{
			OptionWidget->FocusOption();
			return;
		}
	}

	SetKeyboardFocus();
}

void URoguelikeRewardWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || bNativeTreeBuilt)
	{
		return;
	}

	// WBP_RoguelikeReward is retained as the public asset entry point, but the
	// old fixed two-card tree must not win over the generic runtime layout.
	if (WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(WidgetTree->RootWidget);
		WidgetTree->RootWidget = nullptr;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RewardRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RewardRootOverlay"));
	if (UCanvasPanelSlot* RootSlot = RootCanvas->AddChildToCanvas(RootOverlay))
	{
		RootSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		RootSlot->SetOffsets(FMargin(0.0f));
	}

	UTexture2D* DefaultTexture = LoadObject<UTexture2D>(nullptr, DefaultTexturePath);
	BackgroundOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardBackgroundOverlay"));
	if (DefaultTexture)
	{
		BackgroundOverlay->SetBrushFromTexture(DefaultTexture, true);
	}
	// Keep the gameplay world visible behind the floating ink choices. This is
	// deliberately a low-opacity wash, not a card or a full-screen black panel.
	BackgroundOverlay->SetColorAndOpacity(FLinearColor(0.08f, 0.07f, 0.055f, 0.22f));
	BackgroundOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	RootOverlay->AddChildToOverlay(BackgroundOverlay);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardTitle"));
	TitleText->SetText(FText::FromString(TEXT("选择奖励")));
	TitleText->SetJustification(ETextJustify::Center);
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 50;
	TitleText->SetFont(TitleFont);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.025f, 0.022f, 0.018f, 1.0f)));
	if (UOverlaySlot* TitleSlot = RootOverlay->AddChildToOverlay(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetVerticalAlignment(VAlign_Top);
		TitleSlot->SetPadding(FMargin(0.0f, 74.0f, 0.0f, 0.0f));
	}

	TitleDecoration = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardTitleDecoration"));
	if (!TitleDividerTexture)
	{
		TitleDividerTexture = LoadObject<UTexture2D>(nullptr, TitleDividerPath);
	}
	if (TitleDividerTexture)
	{
		TitleDecoration->SetBrushFromTexture(TitleDividerTexture, true);
	}
	TitleDecoration->SetColorAndOpacity(FLinearColor::White);
	TitleDecoration->SetDesiredSizeOverride(
		GetTextureAspectSize(TitleDividerTexture, 420.0f, FVector2D(420.0f, 26.0f)));
	TitleDecoration->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* DecorationSlot = RootOverlay->AddChildToOverlay(TitleDecoration))
	{
		DecorationSlot->SetHorizontalAlignment(HAlign_Center);
		DecorationSlot->SetVerticalAlignment(VAlign_Top);
		DecorationSlot->SetPadding(FMargin(0.0f, 136.0f, 0.0f, 0.0f));
	}

	RewardOptionsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RewardOptionsRow"));
	if (UOverlaySlot* OptionsSlot = RootOverlay->AddChildToOverlay(RewardOptionsRow))
	{
		OptionsSlot->SetHorizontalAlignment(HAlign_Center);
		OptionsSlot->SetVerticalAlignment(VAlign_Center);
		OptionsSlot->SetPadding(FMargin(0.0f, 48.0f, 0.0f, 0.0f));
	}

	bNativeTreeBuilt = true;
}

void URoguelikeRewardWidget::ConfigureWidgetTree()
{
	if (BackgroundOverlay)
	{
		BackgroundOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("选择奖励")));
	}
}

void URoguelikeRewardWidget::HandleSelectionFinished()
{
	SelectionFinishedCallback.ExecuteIfBound();
}

void URoguelikeRewardWidget::HandleOptionHovered(int32 OptionIndex)
{
	if (bSelectionLocked)
	{
		return;
	}

	for (int32 Index = 0; Index < OptionWidgets.Num(); ++Index)
	{
		if (OptionWidgets[Index])
		{
			OptionWidgets[Index]->SetHoverState(Index == OptionIndex);
		}
	}
}

void URoguelikeRewardWidget::HandleOptionUnhovered(int32 OptionIndex)
{
	(void)OptionIndex;
	if (bSelectionLocked)
	{
		return;
	}

	for (URoguelikeRewardOptionWidget* OptionWidget : OptionWidgets)
	{
		if (OptionWidget)
		{
			OptionWidget->SetHoverState(false);
		}
	}
}

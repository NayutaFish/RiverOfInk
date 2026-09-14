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
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "RoguelikeSystem/RoguelikeRewardOptionWidget.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* TitleDividerPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_TitleDivider.T_UI_Reward_TitleDivider");
	const TCHAR* RewardScrollPanelPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_ScrollPanel.T_UI_Reward_ScrollPanel");
	const TCHAR* SmallDividerPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_SmallDivider.T_UI_Reward_SmallDivider");
	const TCHAR* RewardWidgetStandardUiFontPath = TEXT(
		"/Game/RawContent/UI/Fonts/AaGuDianKeBenSongYouMoBan_2_Font.AaGuDianKeBenSongYouMoBan_2_Font");
	constexpr float PanelHorizontalSafeZone = 0.10f;
	constexpr float PanelContentWidth = 1.0f - (PanelHorizontalSafeZone * 2.0f);
	constexpr float FirstColumnDividerAnchor = PanelHorizontalSafeZone + (PanelContentWidth / 3.0f);
	constexpr float SecondColumnDividerAnchor = PanelHorizontalSafeZone + (PanelContentWidth * 2.0f / 3.0f);

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
	// 宽限期计时器不能留到界面销毁之后（回调会打到已经失效的 widget 上）。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputGraceTimerHandle);
	}

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
			OptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			OptionSlot->SetPadding(FMargin(0.0f));
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

	// 输入宽限期：界面刚出现的这段时间里不接收选择输入，避免刚清完场手还按在左键上误选。
	// 只锁"选择"，卡面/悬停动画照常；计时结束后自动恢复。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputGraceTimerHandle);
	}

	if (InputGracePeriod > KINDA_SMALL_NUMBER)
	{
		SetSelectionLocked(true);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InputGraceTimerHandle,
				this,
				&URoguelikeRewardWidget::HandleInputGraceFinished,
				InputGracePeriod,
				false);
		}
		else
		{
			SetSelectionLocked(false);
		}
	}

	ForceLayoutPrepass();

	UE_LOG(LogRoguelike, Log,
		TEXT("Reward widget generated dynamically: Count=%d InputGrace=%.2fs."),
		OptionWidgets.Num(),
		InputGracePeriod);
}

void URoguelikeRewardWidget::HandleInputGraceFinished()
{
	SetSelectionLocked(false);
	UE_LOG(LogRoguelike, Verbose, TEXT("Reward input grace period ended; selection input restored."));
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
		RootSlot->SetZOrder(0);
	}

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardTitle"));
	TitleText->SetText(FText::FromString(TEXT("选择奖励")));
	TitleText->SetJustification(ETextJustify::Center);
	FSlateFontInfo TitleFont = TitleText->GetFont();
	if (UFont* StandardUiFont = LoadObject<UFont>(nullptr, RewardWidgetStandardUiFontPath))
	{
		TitleFont.FontObject = StandardUiFont;
	}
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

	// The selection choices deliberately share one scroll. The art leaves the
	// centre clean for readable build data and limits decoration to the Bian
	// River market scenes at the two ends.
	RewardScrollPanel = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardScrollPanel"));
	if (!RewardScrollPanelTexture)
	{
		RewardScrollPanelTexture = LoadObject<UTexture2D>(nullptr, RewardScrollPanelPath);
	}
	if (RewardScrollPanelTexture)
	{
		RewardScrollPanel->SetBrushFromTexture(RewardScrollPanelTexture, true);
	}
	RewardScrollPanel->SetColorAndOpacity(FLinearColor::White);
	RewardScrollPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(RewardScrollPanel))
	{
		PanelSlot->SetAnchors(FAnchors(0.065f, 0.247f, 0.935f, 0.880f));
		PanelSlot->SetOffsets(FMargin(0.0f));
		PanelSlot->SetZOrder(1);
	}

	PanelContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RewardScrollContent"));
	PanelContentCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* ContentSlot = RootCanvas->AddChildToCanvas(PanelContentCanvas))
	{
		ContentSlot->SetAnchors(FAnchors(0.065f, 0.247f, 0.935f, 0.880f));
		ContentSlot->SetOffsets(FMargin(0.0f));
		ContentSlot->SetZOrder(2);
	}

	RewardOptionsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RewardOptionsRow"));
	if (UCanvasPanelSlot* OptionsSlot = PanelContentCanvas->AddChildToCanvas(RewardOptionsRow))
	{
		OptionsSlot->SetAnchors(FAnchors(PanelHorizontalSafeZone, 0.0f, 1.0f - PanelHorizontalSafeZone, 1.0f));
		OptionsSlot->SetOffsets(FMargin(0.0f, 38.0f, 0.0f, 50.0f));
		OptionsSlot->SetZOrder(1);
	}

	UTexture2D* SmallDividerTexture = LoadObject<UTexture2D>(nullptr, SmallDividerPath);
	auto AddVerticalDivider = [this, SmallDividerTexture](const FName& Name, float AnchorX)
	{
		UImage* Divider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		if (SmallDividerTexture)
		{
			Divider->SetBrushFromTexture(SmallDividerTexture, true);
		}
		Divider->SetColorAndOpacity(FLinearColor(0.19f, 0.18f, 0.16f, 0.48f));
		Divider->SetDesiredSizeOverride(FVector2D(348.0f, 16.0f));
		Divider->SetRenderTransformAngle(90.0f);
		Divider->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Divider->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* DividerSlot = PanelContentCanvas->AddChildToCanvas(Divider))
		{
			DividerSlot->SetAnchors(FAnchors(AnchorX, 0.50f));
			DividerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			DividerSlot->SetSize(FVector2D(348.0f, 16.0f));
			DividerSlot->SetPosition(FVector2D::ZeroVector);
			DividerSlot->SetZOrder(2);
		}
	};
	AddVerticalDivider(TEXT("RewardColumnDividerLeft"), FirstColumnDividerAnchor);
	AddVerticalDivider(TEXT("RewardColumnDividerRight"), SecondColumnDividerAnchor);

	UImage* FooterDivider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardPanelFooterDivider"));
	if (SmallDividerTexture)
	{
		FooterDivider->SetBrushFromTexture(SmallDividerTexture, true);
	}
	FooterDivider->SetColorAndOpacity(FLinearColor(0.14f, 0.13f, 0.12f, 0.62f));
	FooterDivider->SetDesiredSizeOverride(GetTextureAspectSize(SmallDividerTexture, 248.0f, FVector2D(248.0f, 16.0f)));
	FooterDivider->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* FooterSlot = PanelContentCanvas->AddChildToCanvas(FooterDivider))
	{
		FooterSlot->SetAnchors(FAnchors(0.5f, 0.925f));
		FooterSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FooterSlot->SetSize(FooterDivider->GetDesiredSize());
		FooterSlot->SetPosition(FVector2D::ZeroVector);
		FooterSlot->SetZOrder(3);
	}

	bNativeTreeBuilt = true;
}

void URoguelikeRewardWidget::ConfigureWidgetTree()
{
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

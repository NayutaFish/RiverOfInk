// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CombatBuildHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
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
#include "Engine/Texture2D.h"
#include "Player/PlayerCharacter.h"
#include "Player/Skill/SkillComponent.h"
#include "TimerManager.h"

namespace
{
	static const TCHAR* BuildHudPanelPath = TEXT("/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_Panel.T_UI_BuildHUD_Panel");
	static const TCHAR* BuildHudFlyWhitePath = TEXT("/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_FlyWhite.T_UI_BuildHUD_FlyWhite");
	static const TCHAR* BuildHudRecentInkPath = TEXT("/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_RecentInk.T_UI_BuildHUD_RecentInk");
	static const TCHAR* BuildHudPreviousInkPath = TEXT("/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_PreviousInk.T_UI_BuildHUD_PreviousInk");

	constexpr float LatestFeedbackDuration = 0.22f;
	constexpr float LatestFeedbackStartScale = 0.78f;

	FString MakeBuildAssetStem(const FBuildHistoryEntry& Entry)
	{
		if (Entry.RewardType == ERoguelikeRewardType::ChangeSkillForm)
		{
			return Entry.NewSkillForm == EPlayerSkillForm::TwoStageArc
				? TEXT("TwoStageArc")
				: TEXT("SkillForm");
		}

		if (Entry.RewardType == ERoguelikeRewardType::GainSkill)
		{
			return Entry.SkillID == EPlayerSkillID::CircularSlash
				? TEXT("CircularSlash")
				: TEXT("TripleProjectile");
		}

		if (Entry.RewardType == ERoguelikeRewardType::UpgradeSkill)
		{
			switch (Entry.UpgradeType)
			{
			case ESkillUpgradeType::Cooldown:
				return TEXT("Cooldown");
			case ESkillUpgradeType::Damage:
				return TEXT("Damage");
			case ESkillUpgradeType::Mechanic:
			default:
				return TEXT("ProjectileCount");
			}
		}

		switch (Entry.ModifierID)
		{
		case ESkillModifierID::AddProjectile:
			return TEXT("ProjectileCount");
		case ESkillModifierID::InkGrenade:
			return TEXT("InkGrenade");
		case ESkillModifierID::ExtraExplosion:
			return TEXT("ExtraExplosion");
		case ESkillModifierID::TwinSlash:
			return TEXT("TwinSlash");
		case ESkillModifierID::NullRing:
			return TEXT("ProjectileErase");
		case ESkillModifierID::RadiusUp:
			return TEXT("Radius");
		case ESkillModifierID::CooldownDown:
			return TEXT("Cooldown");
		case ESkillModifierID::ProjectileHoming:
			return TEXT("ProjectileHoming");
		default:
			return TEXT("ProjectileCount");
		}
	}

	FString MakeBuildCacheKey(const FBuildHistoryEntry& Entry)
	{
		return FString::Printf(
			TEXT("%d_%d_%d_%d_%d"),
			static_cast<int32>(Entry.RewardType),
			static_cast<int32>(Entry.SkillID),
			static_cast<int32>(Entry.UpgradeType),
			static_cast<int32>(Entry.NewSkillForm),
			static_cast<int32>(Entry.ModifierID));
	}
}

TSharedRef<SWidget> UCombatBuildHudWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UCombatBuildHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	BuildDefaultWidgetTree();
	RefreshBuildHistory();
}

void UCombatBuildHudWidget::NativeDestruct()
{
	UnbindSkillEvents();
	StopLatestBuildFeedback(false);
	Super::NativeDestruct();
}

void UCombatBuildHudWidget::InitializeForPlayer(APlayerCharacter* InPlayer)
{
	UnbindSkillEvents();
	ObservedPlayer = InPlayer;
	ObservedSkillComponent = IsValid(ObservedPlayer) ? ObservedPlayer->SkillComponent : nullptr;
	BindSkillEvents();
	RefreshBuildHistory();

	UE_LOG(LogSkill, Log, TEXT("Combat build HUD bound to %s. History=%d."),
		*GetNameSafe(ObservedPlayer),
		ObservedSkillComponent ? ObservedSkillComponent->GetBuildHistory().Num() : 0);
}

void UCombatBuildHudWidget::SetDetailsKeyLabel(const FText& InKeyLabel)
{
	DetailsKeyLabel = InKeyLabel.IsEmpty() ? FText::FromString(TEXT("B")) : InKeyLabel;
	if (DetailsPromptText)
	{
		DetailsPromptText->SetText(FText::FromString(TEXT("查看构筑")));
	}
	if (DetailsKeyText)
	{
		DetailsKeyText->SetText(DetailsKeyLabel);
	}
	if (DetailsHintText)
	{
		DetailsHintText->SetText(FText::Format(FText::FromString(TEXT("{0}  返回战斗")), DetailsKeyLabel));
	}
}

void UCombatBuildHudWidget::RefreshBuildHistory()
{
	BuildDefaultWidgetTree();
	if (!IsValid(ObservedSkillComponent))
	{
		if (RootSizeBox)
		{
			RootSizeBox->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const TArray<FBuildHistoryEntry>& History = ObservedSkillComponent->GetBuildHistory();
	if (History.IsEmpty())
	{
		StopLatestBuildFeedback(false);
		bDetailsOpen = false;
		LastDisplayedHistoryCount = 0;
		bHasDisplayedLatest = false;
		if (RootSizeBox)
		{
			RootSizeBox->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (RootSizeBox)
	{
		RootSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	const FBuildHistoryEntry& Latest = History.Last();
	const FBuildHistoryEntry* Previous = History.Num() > 1
		? &History[History.Num() - 2]
		: nullptr;
	const bool bIsNewLatest = bHasDisplayedLatest
		&& (History.Num() != LastDisplayedHistoryCount || !IsSameEntry(LastDisplayedLatest, Latest));

	// Update the data and brushes before starting feedback. This keeps the
	// animation interruptible without ever showing stale build information.
	SetBuildEntry(&Latest, true);
	SetBuildEntry(Previous, false);
	LastDisplayedLatest = Latest;
	LastDisplayedHistoryCount = History.Num();
	if (bIsNewLatest)
	{
		StartLatestBuildFeedback();
	}
	else
	{
		StopLatestBuildFeedback(true);
	}
	bHasDisplayedLatest = true;

	if (DetailsLatestText)
	{
		DetailsLatestText->SetText(GetBuildDetailLine(Latest));
	}
	if (DetailsPreviousText)
	{
		DetailsPreviousText->SetText(Previous
			? GetBuildDetailLine(*Previous)
			: FText::FromString(TEXT("上一构筑：暂无记录")));
	}
	if (DetailsPromptText)
	{
		DetailsPromptText->SetText(FText::FromString(TEXT("查看构筑")));
	}
	if (DetailsKeyText)
	{
		DetailsKeyText->SetText(DetailsKeyLabel);
	}
	UpdateDetailsVisibility();
}

void UCombatBuildHudWidget::ToggleBuildDetails()
{
	if (!IsValid(ObservedSkillComponent) || ObservedSkillComponent->GetBuildHistory().IsEmpty())
	{
		UE_LOG(LogSkill, Verbose, TEXT("Combat build detail toggle ignored: build history is empty."));
		return;
	}

	bDetailsOpen = !bDetailsOpen;
	UpdateDetailsVisibility();
}

void UCombatBuildHudWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CombatBuildHudCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CombatBuildHudSize"));
	RootSizeBox->SetWidthOverride(PanelWidth);
	RootSizeBox->SetHeightOverride(PanelHeight);
	if (UCanvasPanelSlot* RootSlot = RootCanvas->AddChildToCanvas(RootSizeBox))
	{
		RootSlot->SetAnchors(FAnchors(1.0f, 1.0f));
		RootSlot->SetAlignment(FVector2D(1.0f, 1.0f));
		RootSlot->SetPosition(FVector2D(-32.0f, -28.0f));
		RootSlot->SetAutoSize(true);
	}

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CombatBuildHudPanel"));
	PanelBorder->SetBrushColor(PanelFallbackColor);
	PanelBorder->SetPadding(FMargin(0.0f));
	RootSizeBox->AddChild(PanelBorder);

	PanelOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CombatBuildHudOverlay"));
	PanelBorder->SetContent(PanelOverlay);

	PanelImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CombatBuildHudPanelImage"));
	if (!PanelTexture)
	{
		PanelTexture = LoadObject<UTexture2D>(nullptr, BuildHudPanelPath);
	}
	if (PanelTexture)
	{
		PanelImage->SetBrushFromTexture(PanelTexture, true);
	}
	PanelBorder->SetBrushColor(PanelTexture ? FLinearColor::Transparent : PanelFallbackColor);
	PanelImage->SetColorAndOpacity(FLinearColor::White);
	PanelImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	UScaleBox* PanelFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("CombatBuildHudPanelFit"));
	PanelFit->SetStretch(EStretch::ScaleToFit);
	PanelFit->SetContent(PanelImage);
	if (UOverlaySlot* PanelSlot = PanelOverlay->AddChildToOverlay(PanelFit))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Fill);
		PanelSlot->SetVerticalAlignment(VAlign_Fill);
	}

	ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CombatBuildContentRow"));
	if (UOverlaySlot* ContentSlot = PanelOverlay->AddChildToOverlay(ContentRow))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(FMargin(30.0f, 22.0f, 30.0f, 38.0f));
	}

	RecentSlotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RecentBuildSlotSize"));
	RecentSlotBox->SetWidthOverride(276.0f);
	RecentSlotBox->SetHeightOverride(184.0f);
	RecentSlotRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RecentBuildSlot"));
	RecentSlotBox->SetContent(RecentSlotRoot);
	if (UHorizontalBoxSlot* RecentRowSlot = ContentRow->AddChildToHorizontalBox(RecentSlotBox))
	{
		RecentRowSlot->SetHorizontalAlignment(HAlign_Fill);
		RecentRowSlot->SetVerticalAlignment(VAlign_Center);
		RecentRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UBorder* RecentFallback = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RecentBuildFallback"));
	RecentFallback->SetBrushColor(RecentFallbackColor);
	if (UOverlaySlot* FallbackSlot = RecentSlotRoot->AddChildToOverlay(RecentFallback))
	{
		FallbackSlot->SetHorizontalAlignment(HAlign_Fill);
		FallbackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	auto AddFittedLayer = [this](UOverlay* Parent, UImage* Image, const TCHAR* WidgetName, const FMargin& LayerPadding)
	{
		UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), WidgetName);
		Fit->SetStretch(EStretch::ScaleToFit);
		Fit->SetContent(Image);
		Fit->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* FitSlot = Parent->AddChildToOverlay(Fit))
		{
			FitSlot->SetHorizontalAlignment(HAlign_Fill);
			FitSlot->SetVerticalAlignment(VAlign_Fill);
			FitSlot->SetPadding(LayerPadding);
		}
		return Fit;
	};
	RecentFlyWhiteImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RecentBuildFlyWhite"));
	if (!FlyWhiteTexture)
	{
		FlyWhiteTexture = LoadObject<UTexture2D>(nullptr, BuildHudFlyWhitePath);
	}
	if (FlyWhiteTexture)
	{
		RecentFlyWhiteImage->SetBrushFromTexture(FlyWhiteTexture, true);
	}
	RecentFlyWhiteImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.82f));
	RecentFlyWhiteImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFittedLayer(RecentSlotRoot, RecentFlyWhiteImage, TEXT("RecentBuildFlyWhiteFit"), FMargin(8.0f, 15.0f, 8.0f, 15.0f));
	RecentInkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RecentBuildInk"));
	if (!RecentInkTexture)
	{
		RecentInkTexture = LoadObject<UTexture2D>(nullptr, BuildHudRecentInkPath);
	}
	if (RecentInkTexture)
	{
		RecentInkImage->SetBrushFromTexture(RecentInkTexture, true);
	}
	RecentInkImage->SetColorAndOpacity(FLinearColor::White);
	RecentInkImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFittedLayer(RecentSlotRoot, RecentInkImage, TEXT("RecentBuildInkFit"), FMargin(0.0f));
	RecentIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RecentBuildIcon"));
	RecentIconImage->SetColorAndOpacity(FLinearColor::White);
	RecentIconImage->SetDesiredSizeOverride(FVector2D(62.0f, 62.0f));
	UScaleBox* RecentIconFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("RecentBuildIconFit"));
	RecentIconFit->SetStretch(EStretch::ScaleToFit);
	RecentIconFit->SetContent(RecentIconImage);
	if (UOverlaySlot* IconSlot = RecentSlotRoot->AddChildToOverlay(RecentIconFit))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.0f, -10.0f, 0.0f, 0.0f));
	}
	RecentCaptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecentBuildCaption"));
	RecentCaptionText->SetText(FText::FromString(TEXT("最近构筑")));
	SetTextStyle(RecentCaptionText, 11, FLinearColor(0.08f, 0.07f, 0.06f, 0.86f));
	if (UOverlaySlot* CaptionSlot = RecentSlotRoot->AddChildToOverlay(RecentCaptionText))
	{
		CaptionSlot->SetHorizontalAlignment(HAlign_Left);
		CaptionSlot->SetVerticalAlignment(VAlign_Top);
		CaptionSlot->SetPadding(FMargin(14.0f, 12.0f, 0.0f, 0.0f));
	}
	RecentTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecentBuildTitle"));
	SetTextStyle(RecentTitleText, 16, FLinearColor(0.04f, 0.035f, 0.03f, 0.94f));
	RecentTitleText->SetJustification(ETextJustify::Center);
	if (UOverlaySlot* TitleSlot = RecentSlotRoot->AddChildToOverlay(RecentTitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetVerticalAlignment(VAlign_Bottom);
		TitleSlot->SetPadding(FMargin(16.0f, 0.0f, 16.0f, 19.0f));
	}
	RecentMetaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RecentBuildMeta"));
	SetTextStyle(RecentMetaText, 11, FLinearColor(0.10f, 0.09f, 0.08f, 0.78f));
	RecentMetaText->SetJustification(ETextJustify::Right);
	if (UOverlaySlot* MetaSlot = RecentSlotRoot->AddChildToOverlay(RecentMetaText))
	{
		MetaSlot->SetHorizontalAlignment(HAlign_Right);
		MetaSlot->SetVerticalAlignment(VAlign_Top);
		MetaSlot->SetPadding(FMargin(0.0f, 12.0f, 14.0f, 0.0f));
	}

	PreviousSlotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PreviousBuildSlotSize"));
	PreviousSlotBox->SetWidthOverride(156.0f);
	PreviousSlotBox->SetHeightOverride(166.0f);
	PreviousSlotRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PreviousBuildSlot"));
	PreviousSlotBox->SetContent(PreviousSlotRoot);
	if (UHorizontalBoxSlot* PreviousRowSlot = ContentRow->AddChildToHorizontalBox(PreviousSlotBox))
	{
		PreviousRowSlot->SetHorizontalAlignment(HAlign_Left);
		PreviousRowSlot->SetVerticalAlignment(VAlign_Center);
		PreviousRowSlot->SetPadding(FMargin(10.0f, 8.0f, 0.0f, 0.0f));
		PreviousRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UBorder* PreviousFallback = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviousBuildFallback"));
	PreviousFallback->SetBrushColor(PreviousFallbackColor);
	if (UOverlaySlot* FallbackSlot = PreviousSlotRoot->AddChildToOverlay(PreviousFallback))
	{
		FallbackSlot->SetHorizontalAlignment(HAlign_Fill);
		FallbackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	PreviousFlyWhiteImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviousBuildFlyWhite"));
	if (FlyWhiteTexture)
	{
		PreviousFlyWhiteImage->SetBrushFromTexture(FlyWhiteTexture, true);
	}
	PreviousFlyWhiteImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.68f));
	PreviousFlyWhiteImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFittedLayer(PreviousSlotRoot, PreviousFlyWhiteImage, TEXT("PreviousBuildFlyWhiteFit"), FMargin(7.0f, 14.0f, 7.0f, 14.0f));
	PreviousInkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviousBuildInk"));
	if (!PreviousInkTexture)
	{
		PreviousInkTexture = LoadObject<UTexture2D>(nullptr, BuildHudPreviousInkPath);
	}
	if (PreviousInkTexture)
	{
		PreviousInkImage->SetBrushFromTexture(PreviousInkTexture, true);
	}
	PreviousInkImage->SetColorAndOpacity(FLinearColor::White);
	PreviousInkImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddFittedLayer(PreviousSlotRoot, PreviousInkImage, TEXT("PreviousBuildInkFit"), FMargin(0.0f));
	PreviousIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviousBuildIcon"));
	PreviousIconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.78f));
	PreviousIconImage->SetDesiredSizeOverride(FVector2D(46.0f, 46.0f));
	UScaleBox* PreviousIconFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("PreviousBuildIconFit"));
	PreviousIconFit->SetStretch(EStretch::ScaleToFit);
	PreviousIconFit->SetContent(PreviousIconImage);
	if (UOverlaySlot* IconSlot = PreviousSlotRoot->AddChildToOverlay(PreviousIconFit))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.0f, -8.0f, 0.0f, 0.0f));
	}
	PreviousCaptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PreviousBuildCaption"));
	PreviousCaptionText->SetText(FText::FromString(TEXT("上一构筑")));
	SetTextStyle(PreviousCaptionText, 10, FLinearColor(0.16f, 0.15f, 0.13f, 0.70f));
	if (UOverlaySlot* CaptionSlot = PreviousSlotRoot->AddChildToOverlay(PreviousCaptionText))
	{
		CaptionSlot->SetHorizontalAlignment(HAlign_Left);
		CaptionSlot->SetVerticalAlignment(VAlign_Top);
		CaptionSlot->SetPadding(FMargin(9.0f, 10.0f, 0.0f, 0.0f));
	}
	PreviousTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PreviousBuildTitle"));
	SetTextStyle(PreviousTitleText, 13, FLinearColor(0.16f, 0.15f, 0.13f, 0.78f));
	PreviousTitleText->SetJustification(ETextJustify::Center);
	if (UOverlaySlot* TitleSlot = PreviousSlotRoot->AddChildToOverlay(PreviousTitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetVerticalAlignment(VAlign_Bottom);
		TitleSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 15.0f));
	}
	PreviousMetaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PreviousBuildMeta"));
	SetTextStyle(PreviousMetaText, 10, FLinearColor(0.22f, 0.21f, 0.19f, 0.62f));
	PreviousMetaText->SetJustification(ETextJustify::Right);
	if (UOverlaySlot* MetaSlot = PreviousSlotRoot->AddChildToOverlay(PreviousMetaText))
	{
		MetaSlot->SetHorizontalAlignment(HAlign_Right);
		MetaSlot->SetVerticalAlignment(VAlign_Top);
		MetaSlot->SetPadding(FMargin(0.0f, 10.0f, 9.0f, 0.0f));
	}

	DetailsPromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuildDetailsPromptRow"));
	if (UOverlaySlot* PromptSlot = PanelOverlay->AddChildToOverlay(DetailsPromptRow))
	{
		PromptSlot->SetHorizontalAlignment(HAlign_Center);
		PromptSlot->SetVerticalAlignment(VAlign_Bottom);
		PromptSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 11.0f));
	}
	DetailsKeyCapBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BuildDetailsKeyCapSize"));
	DetailsKeyCapBox->SetWidthOverride(36.0f);
	DetailsKeyCapBox->SetHeightOverride(36.0f);
	DetailsKeyCapBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BuildDetailsKeyCap"));
	DetailsKeyCapBorder->SetBrushColor(FLinearColor(0.08f, 0.075f, 0.065f, 0.88f));
	DetailsKeyCapBorder->SetPadding(FMargin(2.0f));
	DetailsKeyCapBox->SetContent(DetailsKeyCapBorder);
	DetailsKeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildDetailsKeyText"));
	SetTextStyle(DetailsKeyText, 19, FLinearColor(0.96f, 0.94f, 0.88f, 0.96f));
	DetailsKeyText->SetJustification(ETextJustify::Center);
	DetailsKeyText->SetText(DetailsKeyLabel);
	DetailsKeyCapBorder->SetContent(DetailsKeyText);
	if (UHorizontalBoxSlot* KeySlot = DetailsPromptRow->AddChildToHorizontalBox(DetailsKeyCapBox))
	{
		KeySlot->SetHorizontalAlignment(HAlign_Center);
		KeySlot->SetVerticalAlignment(VAlign_Center);
		KeySlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 0.0f));
	}
	DetailsPromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildDetailsPrompt"));
	SetTextStyle(DetailsPromptText, 11, FLinearColor(0.10f, 0.09f, 0.08f, 0.82f));
	DetailsPromptText->SetText(FText::FromString(TEXT("查看构筑")));
	if (UHorizontalBoxSlot* LabelSlot = DetailsPromptRow->AddChildToHorizontalBox(DetailsPromptText))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	DetailsOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BuildDetailsOverlay"));
	if (UOverlaySlot* DetailSlot = PanelOverlay->AddChildToOverlay(DetailsOverlay))
	{
		DetailSlot->SetHorizontalAlignment(HAlign_Fill);
		DetailSlot->SetVerticalAlignment(VAlign_Fill);
		DetailSlot->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 8.0f));
	}
	DetailsTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildDetailsTitle"));
	DetailsTitleText->SetText(FText::FromString(TEXT("战斗内构筑")));
	SetTextStyle(DetailsTitleText, 17, FLinearColor::White);
	if (UOverlaySlot* TitleSlot = DetailsOverlay->AddChildToOverlay(DetailsTitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Left);
		TitleSlot->SetVerticalAlignment(VAlign_Top);
	}
	DetailsLatestText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildDetailsLatest"));
	SetTextStyle(DetailsLatestText, 14, FLinearColor(0.90f, 0.94f, 0.95f, 0.92f));
	if (UOverlaySlot* LatestSlot = DetailsOverlay->AddChildToOverlay(DetailsLatestText))
	{
		LatestSlot->SetHorizontalAlignment(HAlign_Left);
		LatestSlot->SetVerticalAlignment(VAlign_Top);
		LatestSlot->SetPadding(FMargin(0.0f, 31.0f, 0.0f, 0.0f));
	}
	DetailsPreviousText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildDetailsPrevious"));
	SetTextStyle(DetailsPreviousText, 13, FLinearColor(0.80f, 0.81f, 0.80f, 0.78f));
	if (UOverlaySlot* PreviousDetailSlot = DetailsOverlay->AddChildToOverlay(DetailsPreviousText))
	{
		PreviousDetailSlot->SetHorizontalAlignment(HAlign_Left);
		PreviousDetailSlot->SetVerticalAlignment(VAlign_Top);
		PreviousDetailSlot->SetPadding(FMargin(0.0f, 57.0f, 0.0f, 0.0f));
	}
	DetailsHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildDetailsHint"));
	SetTextStyle(DetailsHintText, 12, FLinearColor(0.82f, 0.82f, 0.78f, 0.64f));
	DetailsHintText->SetJustification(ETextJustify::Right);
	SetDetailsKeyLabel(DetailsKeyLabel);
	if (UOverlaySlot* HintSlot = DetailsOverlay->AddChildToOverlay(DetailsHintText))
	{
		HintSlot->SetHorizontalAlignment(HAlign_Right);
		HintSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	UpdateDetailsVisibility();
}

void UCombatBuildHudWidget::BindSkillEvents()
{
	if (bSkillEventsSubscribed || !IsValid(ObservedSkillComponent))
	{
		return;
	}

	ObservedSkillComponent->OnSkillStateChanged.AddUObject(this, &UCombatBuildHudWidget::HandleSkillStateChanged);
	ObservedSkillComponent->OnBuildHistoryChanged.AddUObject(this, &UCombatBuildHudWidget::HandleBuildHistoryChanged);
	bSkillEventsSubscribed = true;
}

void UCombatBuildHudWidget::UnbindSkillEvents()
{
	if (bSkillEventsSubscribed && IsValid(ObservedSkillComponent))
	{
		ObservedSkillComponent->OnSkillStateChanged.RemoveAll(this);
		ObservedSkillComponent->OnBuildHistoryChanged.RemoveAll(this);
	}
	bSkillEventsSubscribed = false;
}

void UCombatBuildHudWidget::HandleSkillStateChanged()
{
	RefreshBuildHistory();
}

void UCombatBuildHudWidget::HandleBuildHistoryChanged()
{
	RefreshBuildHistory();
}

void UCombatBuildHudWidget::SetBuildEntry(const FBuildHistoryEntry* Entry, bool bRecent)
{
	USizeBox* SlotBox = bRecent ? RecentSlotBox : PreviousSlotBox;
	UImage* IconImage = bRecent ? RecentIconImage : PreviousIconImage;
	UTextBlock* TitleText = bRecent ? RecentTitleText : PreviousTitleText;
	UTextBlock* MetaText = bRecent ? RecentMetaText : PreviousMetaText;
	if (!SlotBox || !IconImage || !TitleText || !MetaText)
	{
		return;
	}

	if (!Entry)
	{
		SlotBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SlotBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	IconImage->SetBrushFromTexture(LoadBuildIcon(*Entry), true);
	IconImage->SetColorAndOpacity(bRecent ? FLinearColor::White : FLinearColor(1.0f, 1.0f, 1.0f, 0.78f));
	TitleText->SetText(GetBuildTitle(*Entry));
	MetaText->SetText(GetBuildMeta(*Entry));
}

void UCombatBuildHudWidget::UpdateDetailsVisibility()
{
	if (!DetailsOverlay || !ContentRow || !DetailsPromptRow)
	{
		return;
	}

	const bool bHasHistory = IsValid(ObservedSkillComponent)
		&& !ObservedSkillComponent->GetBuildHistory().IsEmpty();
	DetailsOverlay->SetVisibility(bHasHistory && bDetailsOpen
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	ContentRow->SetVisibility(bHasHistory && !bDetailsOpen
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	DetailsPromptRow->SetVisibility(bHasHistory && !bDetailsOpen
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (DetailsHintText)
	{
		DetailsHintText->SetText(FText::Format(FText::FromString(TEXT("{0}  返回战斗")), DetailsKeyLabel));
	}
}

void UCombatBuildHudWidget::StartLatestBuildFeedback()
{
	if (!RecentSlotRoot)
	{
		return;
	}

	StopLatestBuildFeedback(false);
	SetWidgetScale(RecentSlotRoot, LatestFeedbackStartScale);
	RecentSlotRoot->SetRenderOpacity(0.0f);
	if (UWorld* World = GetWorld())
	{
		LatestFeedbackStartTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(
			LatestFeedbackTimer,
			this,
			&UCombatBuildHudWidget::UpdateLatestBuildFeedback,
			1.0f / 60.0f,
			true);
	}
}

void UCombatBuildHudWidget::UpdateLatestBuildFeedback()
{
	UWorld* World = GetWorld();
	if (!World || !RecentSlotRoot)
	{
		StopLatestBuildFeedback(false);
		return;
	}

	const float Alpha = FMath::Clamp(
		(World->GetTimeSeconds() - LatestFeedbackStartTime) / LatestFeedbackDuration,
		0.0f,
		1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	SetWidgetScale(RecentSlotRoot, FMath::Lerp(LatestFeedbackStartScale, 1.0f, SmoothAlpha));
	RecentSlotRoot->SetRenderOpacity(SmoothAlpha);
	if (Alpha >= 1.0f)
	{
		StopLatestBuildFeedback(true);
	}
}

void UCombatBuildHudWidget::StopLatestBuildFeedback(bool bRestoreFinalState)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LatestFeedbackTimer);
	}
	if (bRestoreFinalState && RecentSlotRoot)
	{
		SetWidgetScale(RecentSlotRoot, 1.0f);
		RecentSlotRoot->SetRenderOpacity(1.0f);
	}
}

void UCombatBuildHudWidget::SetWidgetScale(UWidget* Widget, float Scale) const
{
	if (!Widget)
	{
		return;
	}

	FWidgetTransform Transform;
	Transform.Scale = FVector2D(Scale, Scale);
	Widget->SetRenderTransform(Transform);
}

void UCombatBuildHudWidget::SetTextStyle(UTextBlock* TextBlock, int32 FontSize, const FLinearColor& Color) const
{
	if (!TextBlock)
	{
		return;
	}

	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = FontSize;
	TextBlock->SetFont(Font);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
}

UTexture2D* UCombatBuildHudWidget::LoadOptionalTexture(FName CacheKey, const TCHAR* AssetPath)
{
	if (TObjectPtr<UTexture2D>* CachedTexture = BuildIconCache.Find(CacheKey))
	{
		return CachedTexture->Get();
	}

	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, AssetPath);
	if (Texture)
	{
		BuildIconCache.Add(CacheKey, Texture);
	}
	else
	{
		UE_LOG(LogSkill, Verbose, TEXT("Combat build HUD asset unavailable: %s"), AssetPath);
	}
	return Texture;
}

UTexture2D* UCombatBuildHudWidget::LoadBuildIcon(const FBuildHistoryEntry& Entry)
{
	const FString Stem = MakeBuildAssetStem(Entry);
	const FString BuildCacheKey = MakeBuildCacheKey(Entry);
	const FName RedrawnCacheKey(*FString::Printf(TEXT("Redrawn_%s"), *BuildCacheKey));
	const FName FallbackCacheKey(*FString::Printf(TEXT("Fallback_%s"), *BuildCacheKey));
	const FString RedrawnPath = FString::Printf(
		TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_%s_Redrawn.T_UI_Build_%s_Redrawn"),
		*Stem,
		*Stem);
	if (UTexture2D* RedrawnTexture = LoadOptionalTexture(RedrawnCacheKey, *RedrawnPath))
	{
		return RedrawnTexture;
	}

	const TCHAR* FallbackPath = TEXT("/Game/RawContent/UI/Texture/Icon_TripleProjectile.Icon_TripleProjectile");
	if (Entry.RewardType == ERoguelikeRewardType::ChangeSkillForm)
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Texture/Icon_CircularSlash.Icon_CircularSlash");
	}
	else if (Entry.RewardType == ERoguelikeRewardType::GainSkill)
	{
		FallbackPath = Entry.SkillID == EPlayerSkillID::CircularSlash
			? TEXT("/Game/RawContent/UI/Texture/Icon_CircularSlash.Icon_CircularSlash")
			: TEXT("/Game/RawContent/UI/Texture/Icon_TripleProjectile.Icon_TripleProjectile");
	}
	else if (Stem == TEXT("ProjectileCount"))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileCount.T_UI_Build_ProjectileCount");
	}
	else if (Stem == TEXT("InkGrenade"))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_InkGrenade.T_UI_Build_InkGrenade");
	}
	else if (Stem == TEXT("ExtraExplosion"))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ExtraExplosion.T_UI_Build_ExtraExplosion");
	}
	else if (Stem == TEXT("TwinSlash"))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_TwinSlash.T_UI_Build_TwinSlash");
	}
	else if (Stem == TEXT("ProjectileErase"))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileErase.T_UI_Build_ProjectileErase");
	}
	else if (Stem == TEXT("Radius"))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_Radius.T_UI_Build_Radius");
	}
	else if (Stem == TEXT("Cooldown"))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_Cooldown.T_UI_Build_Cooldown");
	}

	return LoadOptionalTexture(FallbackCacheKey, FallbackPath);
}

FText UCombatBuildHudWidget::GetBuildTitle(const FBuildHistoryEntry& Entry) const
{
	if (Entry.RewardType == ERoguelikeRewardType::ChangeSkillForm)
	{
		return Entry.NewSkillForm == EPlayerSkillForm::TwoStageArc
			? FText::FromString(TEXT("两段弧斩"))
			: FText::FromString(TEXT("形态变化"));
	}
	if (Entry.RewardType == ERoguelikeRewardType::GainSkill)
	{
		return FText::FromString(TEXT("获得技能"));
	}
	if (Entry.RewardType == ERoguelikeRewardType::UpgradeSkill)
	{
		return FText::FromString(TEXT("技能升级"));
	}

	switch (Entry.ModifierID)
	{
	case ESkillModifierID::AddProjectile:
		return FText::FromString(TEXT("投射物增幅"));
	case ESkillModifierID::InkGrenade:
		return FText::FromString(TEXT("墨雷形态"));
	case ESkillModifierID::ExtraExplosion:
		return FText::FromString(TEXT("余烬连爆"));
	case ESkillModifierID::TwinSlash:
		return FText::FromString(TEXT("双重环斩"));
	case ESkillModifierID::NullRing:
		return FText::FromString(TEXT("净墨环"));
	case ESkillModifierID::RadiusUp:
		return FText::FromString(TEXT("扩展环斩"));
	case ESkillModifierID::CooldownDown:
		return FText::FromString(TEXT("冷却缩减"));
	case ESkillModifierID::ProjectileHoming:
		return FText::FromString(TEXT("引墨"));
	default:
		return FText::FromString(TEXT("构筑"));
	}
}

FText UCombatBuildHudWidget::GetBuildMeta(const FBuildHistoryEntry& Entry) const
{
	const FString SkillText = GetSkillLabel(Entry.SkillID).ToString();
	if (Entry.RewardType == ERoguelikeRewardType::Modifier && Entry.ResultingStackCount > 0)
	{
		return FText::FromString(FString::Printf(TEXT("%s  x%d"), *SkillText, Entry.ResultingStackCount));
	}
	if (Entry.RewardType == ERoguelikeRewardType::ChangeSkillForm)
	{
		return FText::FromString(FString::Printf(TEXT("%s  形态"), *SkillText));
	}
	if (Entry.RewardType == ERoguelikeRewardType::UpgradeSkill)
	{
		return FText::FromString(FString::Printf(TEXT("%s  升级"), *SkillText));
	}
	return FText::FromString(SkillText);
}

FText UCombatBuildHudWidget::GetSkillLabel(EPlayerSkillID SkillID) const
{
	return SkillID == EPlayerSkillID::CircularSlash
		? FText::FromString(TEXT("E"))
		: FText::FromString(TEXT("Q"));
}

FText UCombatBuildHudWidget::GetBuildDetailLine(const FBuildHistoryEntry& Entry) const
{
	const FString SkillText = GetSkillLabel(Entry.SkillID).ToString();
	const FString TitleText = GetBuildTitle(Entry).ToString();
	if (Entry.RewardType == ERoguelikeRewardType::Modifier && Entry.ResultingStackCount > 0)
	{
		return FText::FromString(FString::Printf(
			TEXT("%s：%s（本次 +%d，当前 x%d）"),
			*SkillText,
			*TitleText,
			Entry.StackDelta,
			Entry.ResultingStackCount));
	}
	return FText::FromString(FString::Printf(TEXT("%s：%s"), *SkillText, *TitleText));
}

bool UCombatBuildHudWidget::IsSameEntry(const FBuildHistoryEntry& A, const FBuildHistoryEntry& B) const
{
	return A.RewardType == B.RewardType
		&& A.SkillID == B.SkillID
		&& A.UpgradeType == B.UpgradeType
		&& A.PreviousSkillForm == B.PreviousSkillForm
		&& A.NewSkillForm == B.NewSkillForm
		&& A.ModifierID == B.ModifierID
		&& A.StackDelta == B.StackDelta
		&& A.ResultingStackCount == B.ResultingStackCount;
}

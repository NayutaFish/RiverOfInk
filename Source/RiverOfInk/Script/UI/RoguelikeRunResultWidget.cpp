// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/RoguelikeRunResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Player/PlayerCharacter.h"
#include "Styling/SlateBrush.h"
#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"
#include "UI/BuildPresentationResolver.h"
#include "UI/CombatBuildIconPlaceholderWidget.h"

namespace
{
	constexpr float DesignWidth = 1920.0f;
	constexpr float DesignHeight = 1080.0f;
	constexpr int32 ResultViewportZOrder = 1000;
	const TCHAR* ResultUiFontPath = TEXT(
		"/Game/RawContent/UI/Fonts/AaGuDianKeBenSongYouMoBan_2_Font.AaGuDianKeBenSongYouMoBan_2_Font");

	FLinearColor PaperColor(0.94f, 0.90f, 0.80f, 1.0f);
	FLinearColor InkColor(0.08f, 0.065f, 0.045f, 1.0f);
	FLinearColor SoftInkColor(0.19f, 0.16f, 0.11f, 0.82f);
	FLinearColor DividerColor(0.20f, 0.17f, 0.12f, 0.48f);
	FLinearColor SealColor(0.55f, 0.10f, 0.06f, 1.0f);
	FLinearColor ButtonInkColor(0.07f, 0.055f, 0.038f, 1.0f);

	UTexture2D* LoadResultTexture(const TCHAR* ObjectPath)
	{
		return LoadObject<UTexture2D>(nullptr, ObjectPath);
	}

	UCanvasPanelSlot* AddCanvasChild(
		UCanvasPanel* Parent,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size)
	{
		if (!Parent || !Child)
		{
			return nullptr;
		}

		UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAutoSize(false);
		return Slot;
	}

	UTextBlock* MakeText(
		UWidgetTree* Tree,
		const FName Name,
		const FText& Text,
		int32 Size,
		const FLinearColor& Color,
		ETextJustify::Type Justification = ETextJustify::Left)
	{
		UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(Text);
		TextBlock->SetJustification(Justification);
		FSlateFontInfo Font = TextBlock->GetFont();
		if (UFont* ResultUiFont = LoadObject<UFont>(nullptr, ResultUiFontPath))
		{
			Font.FontObject = ResultUiFont;
		}
		Font.Size = Size;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		return TextBlock;
	}

	FText BuildRewardDeltaText(const FSettlementRewardPick& Pick)
	{
		switch (Pick.RewardType)
		{
		case ERoguelikeRewardType::Modifier:
			return FText::Format(NSLOCTEXT("RunResult", "ModifierDelta", "+{0} 层"), FText::AsNumber(Pick.StackCount));
		case ERoguelikeRewardType::Currency:
			return FText::Format(NSLOCTEXT("RunResult", "CurrencyDelta", "+{0} 纯墨"), FText::AsNumber(Pick.CurrencyAmount));
		case ERoguelikeRewardType::Health:
			return FText::Format(NSLOCTEXT("RunResult", "HealthDelta", "+{0} 生命"), FText::AsNumber(FMath::RoundToInt(Pick.HealthRestoreAmount)));
		default:
			return NSLOCTEXT("RunResult", "RewardAcquired", "已获得");
		}
	}

	FText BuildRewardName(const FSettlementRewardPick& Pick)
	{
		if (!Pick.DisplayName.IsEmpty())
		{
			return FText::FromString(Pick.DisplayName);
		}
		if (Pick.RewardType == ERoguelikeRewardType::Currency)
		{
			return NSLOCTEXT("RunResult", "CurrencyReward", "纯墨");
		}
		if (Pick.RewardType == ERoguelikeRewardType::Health)
		{
			return NSLOCTEXT("RunResult", "HealthReward", "疗伤");
		}
		return NSLOCTEXT("RunResult", "UnknownReward", "未命名增益");
	}

	FBuildHistoryEntry ToBuildHistoryEntry(const FSettlementRewardPick& Pick)
	{
		FBuildHistoryEntry Entry;
		Entry.RewardType = Pick.RewardType;
		Entry.SkillID = Pick.SkillID;
		Entry.UpgradeType = Pick.UpgradeType;
		Entry.NewSkillForm = Pick.TargetSkillForm;
		Entry.ModifierID = Pick.ModifierID;
		Entry.StackDelta = Pick.StackCount;
		return Entry;
	}

	UTexture2D* LoadRewardIcon(const FSettlementRewardPick& Pick)
	{
		if (Pick.RewardType == ERoguelikeRewardType::Currency)
		{
			return LoadObject<UTexture2D>(nullptr,
				TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_PureInk.T_UI_Reward_PureInk"));
		}
		if (Pick.RewardType == ERoguelikeRewardType::Health)
		{
			return LoadObject<UTexture2D>(nullptr,
				TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_Health.T_UI_Reward_Health"));
		}

		const FName IconKey = FBuildPresentationResolver::ResolveIconKey(ToBuildHistoryEntry(Pick));
		if (UTexture2D* RedrawnIcon = LoadObject<UTexture2D>(nullptr,
			*FBuildPresentationResolver::GetRedrawnIconObjectPath(IconKey)))
		{
			return RedrawnIcon;
		}
		return LoadObject<UTexture2D>(nullptr, *FBuildPresentationResolver::GetLegacyIconObjectPath(IconKey));
	}
}

TSharedRef<SWidget> URoguelikeRunResultWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void URoguelikeRunResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	// NativeTick is driven by Slate rather than world time, so its wall-clock
	// entrance sequence continues while this screen pauses gameplay.
	BuildDefaultWidgetTree();
}

void URoguelikeRunResultWidget::NativeDestruct()
{
	bSequencePlaying = false;
	RestartButton->OnClicked.RemoveAll(this);
	PreparationButton->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void URoguelikeRunResultWidget::OpenForCurrentRun()
{
	BuildDefaultWidgetTree();

	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;
	if (!RunFlow || !RunFlow->HasResultSnapshot())
	{
		UE_LOG(LogRoguelikeRunFlow, Error, TEXT("Result widget could not open: no frozen result snapshot is available."));
		return;
	}

	PopulateSnapshot(RunFlow->GetResultSnapshot(), RunFlow->GetRunOutcome());
	ResetEntranceState();
	bLeaveRequested = false;

	if (!IsInViewport())
	{
		AddToViewport(ResultViewportZOrder);
	}
	ConfigureResultInput(true);
	SetKeyboardFocus();
	bSequencePlaying = true;
	SequenceStartSeconds = FPlatformTime::Seconds();

	UE_LOG(LogRoguelikeRunFlow, Log,
		TEXT("Result HUD opened. Outcome=%d Kills=%d Picks=%d."),
		static_cast<int32>(RunFlow->GetRunOutcome()),
		RunFlow->GetResultSnapshot().NonPlayerDeathCount,
		RunFlow->GetResultSnapshot().RewardPicks.Num());
}

void URoguelikeRunResultWidget::CloseForTransition()
{
	bSequencePlaying = false;
	bLeaveRequested = false;
	ConfigureResultInput(false);
}

void URoguelikeRunResultWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bSequencePlaying)
	{
		return;
	}

	UpdateEntranceSequence(static_cast<float>(FPlatformTime::Seconds() - SequenceStartSeconds));
}

void URoguelikeRunResultWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || bNativeTreeBuilt)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(WidgetTree->RootWidget);
		WidgetTree->RootWidget = nullptr;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResultRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* PaperBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultPaperBackground"));
	PaperBackground->SetBrushColor(PaperColor);
	if (UTexture2D* PaperTexture = LoadResultTexture(TEXT("/Game/RawContent/UI/Result/T_UI_Result_Paper.T_UI_Result_Paper")))
	{
		FSlateBrush PaperBrush;
		PaperBrush.SetResourceObject(PaperTexture);
		PaperBrush.DrawAs = ESlateBrushDrawType::Image;
		PaperBrush.Tiling = ESlateBrushTileType::Both;
		PaperBackground->SetBrush(PaperBrush);
		PaperBackground->SetBrushColor(FLinearColor::White);
	}
	PaperBackground->SetVisibility(ESlateVisibility::Visible);
	if (UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(PaperBackground))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	// Keep the canal-town art in a viewport-sized layer. The result content is
	// deliberately authored on a 1920x1080 canvas, but these textures must
	// remain flush with the physical left and right screen edges at every
	// aspect ratio rather than inheriting a letterboxed 16:9 layout.
	TownDecorationLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResultTownDecorationLayer"));
	TownDecorationLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* TownLayerSlot = RootCanvas->AddChildToCanvas(TownDecorationLayer))
	{
		TownLayerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		TownLayerSlot->SetOffsets(FMargin(0.0f));
	}

	UScaleBox* LayoutScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ResultLayoutScaleBox"));
	LayoutScaleBox->SetStretch(EStretch::ScaleToFit);
	LayoutScaleBox->SetStretchDirection(EStretchDirection::Both);
	if (UCanvasPanelSlot* ScaleSlot = RootCanvas->AddChildToCanvas(LayoutScaleBox))
	{
		ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ScaleSlot->SetOffsets(FMargin(0.0f));
	}

	USizeBox* DesignSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ResultDesignSize"));
	DesignSize->SetWidthOverride(DesignWidth);
	DesignSize->SetHeightOverride(DesignHeight);
	LayoutScaleBox->SetContent(DesignSize);

	ContentRoot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultContentRoot"));
	ContentRoot->SetBrushColor(FLinearColor::Transparent);
	DesignSize->SetContent(ContentRoot);

	UCanvasPanel* LayoutCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResultLayoutCanvas"));
	ContentRoot->SetContent(LayoutCanvas);

	// The bottom line art intentionally only occupies the corners, leaving the
	// result data and actions readable over uninterrupted paper in the middle.
	const auto AddTownDecoration = [this](const FName Name, const TCHAR* TexturePath, const FAnchors& Anchors)
	{
		if (UTexture2D* TownTexture = LoadResultTexture(TexturePath))
		{
			UImage* TownImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
			TownImage->SetBrushFromTexture(TownTexture, true);
			TownImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.66f));
			TownImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* TownSlot = TownDecorationLayer->AddChildToCanvas(TownImage))
			{
				TownSlot->SetAnchors(Anchors);
				TownSlot->SetOffsets(FMargin(0.0f));
			}
		}
	};
	AddTownDecoration(TEXT("ResultTownLeft"), TEXT("/Game/RawContent/UI/Result/T_UI_Result_TownLeft.T_UI_Result_TownLeft"), FAnchors(0.0f, 0.67f, 0.45f, 1.0f));
	AddTownDecoration(TEXT("ResultTownRight"), TEXT("/Game/RawContent/UI/Result/T_UI_Result_TownRight.T_UI_Result_TownRight"), FAnchors(0.55f, 0.67f, 1.0f, 1.0f));

	OutcomeTitleImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ResultOutcomeTitleImage"));
	OutcomeTitleImage->SetVisibility(ESlateVisibility::Collapsed);
	OutcomeTitleImage->SetColorAndOpacity(FLinearColor::White);
	AddCanvasChild(LayoutCanvas, OutcomeTitleImage, FVector2D(162.0f, 42.0f), FVector2D(410.0f, 190.0f));

	OutcomeTitleText = MakeText(WidgetTree, TEXT("ResultOutcomeTitle"), FText::GetEmpty(), 96, InkColor);
	AddCanvasChild(LayoutCanvas, OutcomeTitleText, FVector2D(175.0f, 72.0f), FVector2D(380.0f, 130.0f));

	if (UTexture2D* SealTexture = LoadResultTexture(TEXT("/Game/RawContent/UI/Shop/T_UI_ShopHUD_Seal.T_UI_ShopHUD_Seal")))
	{
		UImage* SealImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ResultSealImage"));
		SealImage->SetBrushFromTexture(SealTexture, true);
		SealImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddCanvasChild(LayoutCanvas, SealImage, FVector2D(525.0f, 134.0f), FVector2D(48.0f, 48.0f));
	}
	else
	{
		UTextBlock* SealText = MakeText(WidgetTree, TEXT("ResultSealText"), NSLOCTEXT("RunResult", "Seal", "结算"), 22, SealColor, ETextJustify::Center);
		AddCanvasChild(LayoutCanvas, SealText, FVector2D(505.0f, 136.0f), FVector2D(70.0f, 55.0f));
	}

	OutcomeSubtitleText = MakeText(WidgetTree, TEXT("ResultOutcomeSubtitle"), NSLOCTEXT("RunResult", "Review", "本局回顾"), 26, SoftInkColor);
	AddCanvasChild(LayoutCanvas, OutcomeSubtitleText, FVector2D(176.0f, 225.0f), FVector2D(260.0f, 42.0f));
	UBorder* HeaderRule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultHeaderRule"));
	HeaderRule->SetBrushColor(DividerColor);
	AddCanvasChild(LayoutCanvas, HeaderRule, FVector2D(176.0f, 267.0f), FVector2D(405.0f, 1.0f));

	UBorder* VerticalDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultVerticalDivider"));
	VerticalDivider->SetBrushColor(DividerColor);
	AddCanvasChild(LayoutCanvas, VerticalDivider, FVector2D(724.0f, 294.0f), FVector2D(1.0f, 510.0f));

	UTextBlock* StatsHeading = MakeText(WidgetTree, TEXT("ResultStatsHeading"), NSLOCTEXT("RunResult", "StatsHeading", "本局战绩"), 38, InkColor);
	AddCanvasChild(LayoutCanvas, StatsHeading, FVector2D(170.0f, 296.0f), FVector2D(390.0f, 56.0f));

	const TArray<FText> StatLabels =
	{
		NSLOCTEXT("RunResult", "KillLabel", "击败敌人"),
		NSLOCTEXT("RunResult", "DamageLabel", "累计伤害"),
		NSLOCTEXT("RunResult", "InkLabel", "获得纯墨"),
		NSLOCTEXT("RunResult", "PurchaseLabel", "购买次数")
	};

	for (int32 Index = 0; Index < StatLabels.Num(); ++Index)
	{
		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("ResultStatRow%d"), Index));
		Row->SetBrushColor(FLinearColor::Transparent);
		UCanvasPanel* RowCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ResultStatRowCanvas%d"), Index));
		Row->SetContent(RowCanvas);
		const bool bPrimaryStat = Index == 0;
		const float RowY = bPrimaryStat ? 350.0f : 620.0f + static_cast<float>(Index - 1) * 64.0f;
		AddCanvasChild(LayoutCanvas, Row, FVector2D(170.0f, RowY), FVector2D(465.0f, bPrimaryStat ? 250.0f : 64.0f));

		if (!bPrimaryStat)
		{
			UBorder* RowRule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("ResultStatRule%d"), Index));
			RowRule->SetBrushColor(DividerColor);
			AddCanvasChild(RowCanvas, RowRule, FVector2D(0.0f, 0.0f), FVector2D(450.0f, 1.0f));
		}

		UTextBlock* LabelText = MakeText(WidgetTree, *FString::Printf(TEXT("ResultStatLabel%d"), Index), StatLabels[Index], bPrimaryStat ? 34 : 29, SoftInkColor, bPrimaryStat ? ETextJustify::Center : ETextJustify::Left);
		AddCanvasChild(RowCanvas, LabelText, bPrimaryStat ? FVector2D(70.0f, 150.0f) : FVector2D(0.0f, 14.0f), bPrimaryStat ? FVector2D(320.0f, 45.0f) : FVector2D(240.0f, 42.0f));

		UTextBlock* ValueText = MakeText(WidgetTree, *FString::Printf(TEXT("ResultStatValue%d"), Index), FText::FromString(TEXT("0")), bPrimaryStat ? 106 : 38, InkColor, bPrimaryStat ? ETextJustify::Center : ETextJustify::Right);
		ValueText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		AddCanvasChild(RowCanvas, ValueText, bPrimaryStat ? FVector2D(58.0f, 8.0f) : FVector2D(265.0f, 10.0f), bPrimaryStat ? FVector2D(345.0f, 135.0f) : FVector2D(185.0f, 44.0f));

		if (bPrimaryStat)
		{
			UTextBlock* EliteText = MakeText(WidgetTree, TEXT("ResultEliteText"), FText::GetEmpty(), 24, SoftInkColor, ETextJustify::Center);
			AddCanvasChild(RowCanvas, EliteText, FVector2D(80.0f, 204.0f), FVector2D(300.0f, 34.0f));
		}

		StatRows.Add(Row);
		StatValueTexts.Add(ValueText);
	}

	UTextBlock* RewardHeading = MakeText(WidgetTree, TEXT("ResultRewardsHeading"), NSLOCTEXT("RunResult", "RewardsHeading", "增益历程"), 38, InkColor);
	AddCanvasChild(LayoutCanvas, RewardHeading, FVector2D(805.0f, 296.0f), FVector2D(360.0f, 56.0f));
	UBorder* RewardHeadingRule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultRewardsHeadingRule"));
	RewardHeadingRule->SetBrushColor(DividerColor);
	AddCanvasChild(LayoutCanvas, RewardHeadingRule, FVector2D(805.0f, 352.0f), FVector2D(865.0f, 1.0f));

	EmptyRewardsText = MakeText(WidgetTree, TEXT("ResultEmptyRewards"), NSLOCTEXT("RunResult", "NoRewards", "本局未获得增益"), 28, SoftInkColor, ETextJustify::Center);
	AddCanvasChild(LayoutCanvas, EmptyRewardsText, FVector2D(805.0f, 520.0f), FVector2D(865.0f, 65.0f));
	EmptyRewardsText->SetVisibility(ESlateVisibility::Collapsed);

	ActionRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResultActionRoot"));
	AddCanvasChild(LayoutCanvas, ActionRoot, FVector2D(610.0f, 890.0f), FVector2D(700.0f, 94.0f));

	RestartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ResultRestartButton"));
	RestartButton->SetBackgroundColor(FLinearColor::Transparent);
	// UButton's default pressed padding can shrink an overlay just enough to
	// crop the first/last glyph of a short CJK label. The brush owns all visual
	// feedback here, so keep its content rectangle stable in every state.
	FButtonStyle RestartButtonStyle = RestartButton->GetStyle();
	RestartButtonStyle.SetNormalPadding(FMargin(0.0f));
	RestartButtonStyle.SetPressedPadding(FMargin(0.0f));
	RestartButton->SetStyle(RestartButtonStyle);
	UOverlay* RestartVisual = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ResultRestartVisual"));
	if (UTexture2D* PrimaryBrushTexture = LoadResultTexture(TEXT("/Game/RawContent/UI/Result/T_UI_Result_PrimaryBrush.T_UI_Result_PrimaryBrush")))
	{
		UImage* PrimaryBrushImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ResultRestartBrush"));
		PrimaryBrushImage->SetBrushFromTexture(PrimaryBrushTexture, true);
		PrimaryBrushImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* BrushSlot = RestartVisual->AddChildToOverlay(PrimaryBrushImage))
		{
			BrushSlot->SetHorizontalAlignment(HAlign_Fill);
			BrushSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	UTextBlock* RestartText = MakeText(WidgetTree, TEXT("ResultRestartText"), NSLOCTEXT("RunResult", "Restart", "再来一局"), 28, PaperColor, ETextJustify::Center);
	RestartText->SetAutoWrapText(false);
	RestartText->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* TextSlot = RestartVisual->AddChildToOverlay(RestartText))
	{
		// Center the intrinsic text size instead of stretching it to the brush;
		// this guarantees all four glyphs retain their full advance width.
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetPadding(FMargin(28.0f, 8.0f));
	}
	RestartButton->SetContent(RestartVisual);
	UCanvasPanel* ActionCanvas = Cast<UCanvasPanel>(ActionRoot);
	AddCanvasChild(ActionCanvas, RestartButton, FVector2D(0.0f, 0.0f), FVector2D(410.0f, 82.0f));

	PreparationButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ResultPreparationButton"));
	PreparationButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* PreparationText = MakeText(WidgetTree, TEXT("ResultPreparationText"), NSLOCTEXT("RunResult", "Preparation", "返回准备区"), 27, InkColor, ETextJustify::Center);
	PreparationText->SetVisibility(ESlateVisibility::HitTestInvisible);
	PreparationButton->SetContent(PreparationText);
	AddCanvasChild(ActionCanvas, PreparationButton, FVector2D(445.0f, 6.0f), FVector2D(220.0f, 60.0f));
	UBorder* PreparationRule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultPreparationRule"));
	PreparationRule->SetBrushColor(DividerColor);
	AddCanvasChild(ActionCanvas, PreparationRule, FVector2D(470.0f, 70.0f), FVector2D(170.0f, 1.0f));

	StatusText = MakeText(WidgetTree, TEXT("ResultStatusText"), FText::GetEmpty(), 20, SealColor, ETextJustify::Center);
	AddCanvasChild(LayoutCanvas, StatusText, FVector2D(550.0f, 992.0f), FVector2D(820.0f, 30.0f));

	RestartButton->OnClicked.AddDynamic(this, &URoguelikeRunResultWidget::RequestRestart);
	PreparationButton->OnClicked.AddDynamic(this, &URoguelikeRunResultWidget::RequestPreparationRoom);
	bNativeTreeBuilt = true;
}

void URoguelikeRunResultWidget::PopulateSnapshot(const FRoguelikeRunResultSnapshot& Snapshot, ERoguelikeRunOutcome Outcome)
{
	const bool bVictory = Outcome == ERoguelikeRunOutcome::Victory;
	OutcomeTitleText->SetText(bVictory
		? NSLOCTEXT("RunResult", "Victory", "通关")
		: NSLOCTEXT("RunResult", "Defeat", "落败"));
	OutcomeTitleText->SetVisibility(ESlateVisibility::Visible);
	if (OutcomeTitleImage)
	{
		const TCHAR* TitleTexturePath = bVictory
			? TEXT("/Game/RawContent/UI/Result/T_UI_Result_TitleVictory.T_UI_Result_TitleVictory")
			: TEXT("/Game/RawContent/UI/Result/T_UI_Result_TitleDefeat.T_UI_Result_TitleDefeat");
		if (UTexture2D* TitleTexture = LoadResultTexture(TitleTexturePath))
		{
			OutcomeTitleImage->SetBrushFromTexture(TitleTexture, true);
			OutcomeTitleImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			OutcomeTitleText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			OutcomeTitleImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	OutcomeSubtitleText->SetText(bVictory
		? NSLOCTEXT("RunResult", "VictoryReview", "本局回顾")
		: NSLOCTEXT("RunResult", "DefeatReview", "整装再战"));
	StatusText->SetText(FText::GetEmpty());

	const TArray<FText> StatValues =
	{
		FText::AsNumber(Snapshot.NonPlayerDeathCount),
		FText::AsNumber(FMath::RoundToInt(Snapshot.TotalDamageDealtToNonPlayer)),
		FText::AsNumber(Snapshot.TotalShopCurrencyGained),
		FText::AsNumber(Snapshot.ShopPurchaseCount)
	};
	for (int32 Index = 0; Index < StatValueTexts.Num() && Index < StatValues.Num(); ++Index)
	{
		StatValueTexts[Index]->SetText(StatValues[Index]);
	}

	if (StatRows.IsValidIndex(0))
	{
		if (UCanvasPanel* FirstRowCanvas = Cast<UCanvasPanel>(Cast<UBorder>(StatRows[0])->GetContent()))
		{
			if (UTextBlock* EliteText = Cast<UTextBlock>(FirstRowCanvas->GetChildAt(2)))
			{
				EliteText->SetText(FText::Format(
					NSLOCTEXT("RunResult", "EliteCount", "其中精英 {0}"),
					FText::AsNumber(Snapshot.EliteEnemyDeathCount)));
			}
		}
	}

	for (UWidget* Entry : RewardEntries)
	{
		if (Entry)
		{
			Entry->RemoveFromParent();
		}
	}
	RewardEntries.Reset();

	const int32 VisibleCount = FMath::Min(6, Snapshot.RewardPicks.Num());
	EmptyRewardsText->SetVisibility(VisibleCount == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UCanvasPanel* LayoutCanvas = Cast<UCanvasPanel>(ContentRoot->GetContent());
	for (int32 Index = 0; Index < VisibleCount; ++Index)
	{
		UWidget* Entry = BuildRewardEntry(Snapshot.RewardPicks[Index], Index);
		const int32 Row = Index / 3;
		const int32 Column = Index % 3;
		AddCanvasChild(LayoutCanvas, Entry, FVector2D(805.0f + Column * 290.0f, 375.0f + Row * 215.0f), FVector2D(270.0f, 195.0f));
		RewardEntries.Add(Entry);
	}
}

void URoguelikeRunResultWidget::ResetEntranceState()
{
	ContentRoot->SetRenderOpacity(0.0f);
	if (TownDecorationLayer)
	{
		TownDecorationLayer->SetRenderOpacity(0.0f);
	}
	for (int32 Index = 0; Index < StatRows.Num(); ++Index)
	{
		if (StatRows[Index])
		{
			StatRows[Index]->SetRenderOpacity(0.0f);
		}
		if (StatValueTexts.IsValidIndex(Index) && StatValueTexts[Index])
		{
			StatValueTexts[Index]->SetRenderTransform(FWidgetTransform());
		}
	}
	for (UWidget* Entry : RewardEntries)
	{
		if (Entry)
		{
			Entry->SetRenderOpacity(0.0f);
		}
	}
	EmptyRewardsText->SetRenderOpacity(0.0f);
	ActionRoot->SetRenderOpacity(0.0f);
	SetActionsEnabled(false);
}

void URoguelikeRunResultWidget::UpdateEntranceSequence(float ElapsedSeconds)
{
	const float SafeIntroDuration = FMath::Max(0.01f, IntroDuration);
	const float IntroOpacity = FMath::Clamp(ElapsedSeconds / SafeIntroDuration, 0.0f, 1.0f);
	ContentRoot->SetRenderOpacity(IntroOpacity);
	if (TownDecorationLayer)
	{
		TownDecorationLayer->SetRenderOpacity(IntroOpacity);
	}
	if (ElapsedSeconds < SafeIntroDuration)
	{
		return;
	}

	const float StatsStart = SafeIntroDuration;
	const float StatStep = FMath::Max(0.01f, StatPulseDuration + StatGap);
	for (int32 Index = 0; Index < StatRows.Num(); ++Index)
	{
		const float LocalTime = ElapsedSeconds - StatsStart - static_cast<float>(Index) * StatStep;
		const float RowOpacity = FMath::Clamp(LocalTime / FMath::Max(0.01f, StatFadeDuration), 0.0f, 1.0f);
		StatRows[Index]->SetRenderOpacity(RowOpacity);
		if (LocalTime < 0.0f || !StatValueTexts.IsValidIndex(Index) || !StatValueTexts[Index])
		{
			continue;
		}

		float Scale = 1.0f;
		if (LocalTime < StatPulsePeakTime)
		{
			Scale = FMath::InterpEaseInOut(
				1.0f,
				StatPulseMaxScale,
				FMath::Clamp(LocalTime / FMath::Max(0.01f, StatPulsePeakTime), 0.0f, 1.0f),
				2.0f);
		}
		else if (LocalTime < StatPulseDuration)
		{
			Scale = FMath::InterpEaseInOut(
				StatPulseMaxScale,
				1.0f,
				FMath::Clamp((LocalTime - StatPulsePeakTime) / FMath::Max(0.01f, StatPulseDuration - StatPulsePeakTime), 0.0f, 1.0f),
				2.0f);
		}
		StatValueTexts[Index]->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(Scale, Scale), FVector2D::ZeroVector, 0.0f));
	}

	const float RewardsStart = StatsStart + StatRows.Num() * StatStep + RewardStartDelay;
	const float RewardStep = FMath::Max(0.01f, RewardFadeDuration + RewardGap);
	if (RewardEntries.IsEmpty())
	{
		EmptyRewardsText->SetRenderOpacity(ElapsedSeconds >= RewardsStart ? 1.0f : 0.0f);
	}
	else
	{
		for (int32 Index = 0; Index < RewardEntries.Num(); ++Index)
		{
			const float LocalTime = ElapsedSeconds - RewardsStart - static_cast<float>(Index) * RewardStep;
			RewardEntries[Index]->SetRenderOpacity(FMath::Clamp(LocalTime / FMath::Max(0.01f, RewardFadeDuration), 0.0f, 1.0f));
		}
	}

	const float ActionsStart = RewardsStart + RewardEntries.Num() * RewardStep;
	const float ActionOpacity = FMath::Clamp(
		(ElapsedSeconds - ActionsStart) / FMath::Max(0.01f, ActionsFadeDuration), 0.0f, 1.0f);
	ActionRoot->SetRenderOpacity(ActionOpacity);
	if (ActionOpacity >= 1.0f)
	{
		bSequencePlaying = false;
		for (UTextBlock* ValueText : StatValueTexts)
		{
			if (ValueText)
			{
				ValueText->SetRenderTransform(FWidgetTransform());
			}
		}
		SetActionsEnabled(true);
		RestartButton->SetKeyboardFocus();
	}
}

void URoguelikeRunResultWidget::SetActionsEnabled(bool bEnabled)
{
	RestartButton->SetIsEnabled(bEnabled && !bLeaveRequested);
	PreparationButton->SetIsEnabled(bEnabled && !bLeaveRequested);
}

void URoguelikeRunResultWidget::ConfigureResultInput(bool bResultOpen)
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	if (bResultOpen)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
		UGameplayStatics::SetGamePaused(this, true);

		if (APlayerCharacter* Player = Cast<APlayerCharacter>(PlayerController->GetPawn()))
		{
			Player->SetGameplayHudVisible(false);
		}
	}
	else
	{
		UGameplayStatics::SetGamePaused(this, false);
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
	}
}

void URoguelikeRunResultWidget::RequestRestart()
{
	BeginLeaveRequest();
	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;
	if (!RunFlow || !RunFlow->RestartRun())
	{
		RestoreAfterLeaveFailure(NSLOCTEXT("RunResult", "RestartFailed", "无法开始新局，请重试或返回准备区。"));
	}
}

void URoguelikeRunResultWidget::RequestPreparationRoom()
{
	BeginLeaveRequest();
	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;
	if (!RunFlow || !RunFlow->LoadPreparationRoom())
	{
		RestoreAfterLeaveFailure(NSLOCTEXT("RunResult", "PreparationFailed", "无法返回准备区，请重试。"));
	}
}

void URoguelikeRunResultWidget::BeginLeaveRequest()
{
	if (bLeaveRequested)
	{
		return;
	}

	bLeaveRequested = true;
	bSequencePlaying = false;
	SetActionsEnabled(false);
	StatusText->SetText(NSLOCTEXT("RunResult", "Loading", "正在加载……"));
	ConfigureResultInput(false);
}

void URoguelikeRunResultWidget::RestoreAfterLeaveFailure(const FText& FailureMessage)
{
	bLeaveRequested = false;
	StatusText->SetText(FailureMessage);
	ConfigureResultInput(true);
	SetActionsEnabled(true);
}

UWidget* URoguelikeRunResultWidget::BuildRewardEntry(const FSettlementRewardPick& Pick, int32 DisplayIndex)
{
	UBorder* EntryBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("ResultRewardEntry%d"), DisplayIndex));
	EntryBorder->SetBrushColor(FLinearColor::Transparent);
	UCanvasPanel* EntryCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ResultRewardCanvas%d"), DisplayIndex));
	EntryBorder->SetContent(EntryCanvas);

	UTextBlock* OrderText = MakeText(WidgetTree, *FString::Printf(TEXT("ResultRewardOrder%d"), DisplayIndex), FText::FromString(FString::Printf(TEXT("%02d"), DisplayIndex + 1)), 18, SoftInkColor);
	AddCanvasChild(EntryCanvas, OrderText, FVector2D(4.0f, 4.0f), FVector2D(42.0f, 28.0f));

	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("ResultRewardIconBox%d"), DisplayIndex));
	IconBox->SetWidthOverride(120.0f);
	IconBox->SetHeightOverride(120.0f);
	if (UTexture2D* Icon = LoadRewardIcon(Pick))
	{
		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("ResultRewardIcon%d"), DisplayIndex));
		IconImage->SetBrushFromTexture(Icon, true);
		IconImage->SetColorAndOpacity(FLinearColor::White);
		IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		IconBox->SetContent(IconImage);
	}
	else
	{
		UCombatBuildIconPlaceholderWidget* Placeholder = WidgetTree->ConstructWidget<UCombatBuildIconPlaceholderWidget>(
			UCombatBuildIconPlaceholderWidget::StaticClass(),
			*FString::Printf(TEXT("ResultRewardPlaceholder%d"), DisplayIndex));
		Placeholder->SetPlaceholderKind(FBuildPresentationResolver::ResolvePlaceholderKind(
			FBuildPresentationResolver::ResolveIconKey(ToBuildHistoryEntry(Pick))));
		IconBox->SetContent(Placeholder);
	}
	AddCanvasChild(EntryCanvas, IconBox, FVector2D(75.0f, 20.0f), FVector2D(120.0f, 120.0f));

	const FText RewardCaption = FText::Format(
		NSLOCTEXT("RunResult", "RewardCaption", "{0}  {1}"),
		BuildRewardName(Pick),
		BuildRewardDeltaText(Pick));
	UTextBlock* CaptionText = MakeText(WidgetTree, *FString::Printf(TEXT("ResultRewardCaption%d"), DisplayIndex), RewardCaption, 22, InkColor, ETextJustify::Center);
	CaptionText->SetAutoWrapText(true);
	AddCanvasChild(EntryCanvas, CaptionText, FVector2D(8.0f, 146.0f), FVector2D(254.0f, 42.0f));

	return EntryBorder;
}

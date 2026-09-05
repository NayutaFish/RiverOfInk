// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CombatBuildDetailsWidget.h"

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
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Player/PlayerCharacter.h"
#include "Player/Skill/SkillComponent.h"
#include "UI/BuildPresentationResolver.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	constexpr int32 DetailsCategoryCount = 5;
	constexpr int32 DetailsSlotCount = 5;

	static const TCHAR* DetailsPanelTexturePath = TEXT(
		"/Game/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_PaperFeibai.T_UI_BuildDetailsPanel_PaperFeibai");
	static const TCHAR* DetailsSelectedWashTexturePath = TEXT(
		"/Game/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_SelectedWash.T_UI_BuildDetailsPanel_SelectedWash");
	static const TCHAR* DetailsFontPath = TEXT(
		"/Game/RawContent/UI/Fonts/AaGuDianKeBenSongYouMoBan_2_Font.AaGuDianKeBenSongYouMoBan_2_Font");

	ECombatBuildCategory CategoryFromIndex(int32 CategoryIndex)
	{
		switch (CategoryIndex)
		{
		case 0:
			return ECombatBuildCategory::BasicAttack;
		case 1:
			return ECombatBuildCategory::Projectile;
		case 2:
			return ECombatBuildCategory::QSkill;
		case 3:
			return ECombatBuildCategory::ESkill;
		case 4:
		default:
			return ECombatBuildCategory::General;
		}
	}

	FLinearColor GetPlaceholderColor(bool bOwned)
	{
		return bOwned
			? FLinearColor(0.08f, 0.07f, 0.06f, 0.92f)
			: FLinearColor(0.28f, 0.26f, 0.22f, 0.42f);
	}

	FButtonStyle MakeTransparentButtonStyle()
	{
		FButtonStyle Style;
		Style.Normal = FSlateNoResource();
		Style.Hovered = FSlateNoResource();
		Style.Pressed = FSlateNoResource();
		Style.Disabled = FSlateNoResource();
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);
		return Style;
	}
}

TSharedRef<SWidget> UCombatBuildDetailsWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UCombatBuildDetailsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	BuildDefaultWidgetTree();
	RefreshDetails();
}

void UCombatBuildDetailsWidget::NativeDestruct()
{
	ClearForClose();
	Super::NativeDestruct();
}

void UCombatBuildDetailsWidget::InitializeForPlayer(APlayerCharacter* InPlayer)
{
	UnbindSkillEvents();
	ObservedPlayer = InPlayer;
	ObservedSkillComponent = IsValid(ObservedPlayer) ? ObservedPlayer->SkillComponent : nullptr;
	BindSkillEvents();
	RefreshDetails();

	UE_LOG(LogSkill, Log,
		TEXT("Combat build details whitebox bound to %s. Items=%d History=%d."),
		*GetNameSafe(ObservedPlayer),
		ViewModel.Items.Num(),
		ViewModel.SourceHistoryCount);
}

void UCombatBuildDetailsWidget::RefreshDetails()
{
	BuildDefaultWidgetTree();
	ViewModel = FBuildPresentationResolver::BuildViewModel(ObservedSkillComponent);
	UE_LOG(LogSkill, Log,
		TEXT("Combat build details refreshed from acquired history: Items=%d History=%d."),
		ViewModel.Items.Num(),
		ViewModel.SourceHistoryCount);
	RefreshCategorySlots();
	RefreshSelectedPreview();
	RefreshSelectionVisuals();
}

void UCombatBuildDetailsWidget::SetDetailsKey(const FKey& InKey)
{
	DetailsKey = InKey.IsValid() ? InKey : EKeys::B;
}

void UCombatBuildDetailsWidget::ClearForClose()
{
	UnbindSkillEvents();
	ObservedPlayer = nullptr;
	ObservedSkillComponent = nullptr;
	ViewModel = FCombatBuildDetailsViewModel();
	SelectedBuildId = NAME_None;
	SelectedCategoryIndex = INDEX_NONE;
	SelectedItemIndex = INDEX_NONE;
	CategoryStartIndices.Init(0, DetailsCategoryCount);
	CategoryItemCounts.Init(0, DetailsCategoryCount);
}

void UCombatBuildDetailsWidget::BindSkillEvents()
{
	if (bSkillEventsSubscribed || !IsValid(ObservedSkillComponent))
	{
		return;
	}

	// BuildHistoryChanged is deliberately the low-frequency build snapshot event.
	// OnSkillStateChanged also fires for casts and cooldown-stage transitions, so
	// subscribing to it here would rebuild this details list during combat.
	ObservedSkillComponent->OnBuildHistoryChanged.AddUObject(
		this,
		&UCombatBuildDetailsWidget::HandleBuildHistoryChanged);
	bSkillEventsSubscribed = true;
	UE_LOG(LogSkill, Log,
		TEXT("Combat build details subscribed to build-state changes: Component=%s Owner=%s."),
		*GetNameSafe(ObservedSkillComponent),
		*GetNameSafe(ObservedSkillComponent->GetOwner()));
}

void UCombatBuildDetailsWidget::UnbindSkillEvents()
{
	if (bSkillEventsSubscribed && IsValid(ObservedSkillComponent))
	{
		ObservedSkillComponent->OnBuildHistoryChanged.RemoveAll(this);
	}
	bSkillEventsSubscribed = false;
}

void UCombatBuildDetailsWidget::HandleBuildHistoryChanged()
{
	if (!IsValid(ObservedSkillComponent))
	{
		return;
	}

	UE_LOG(LogSkill, Verbose,
		TEXT("Combat build details received build-state change: Component=%s History=%d."),
		*GetNameSafe(ObservedSkillComponent),
		ObservedSkillComponent->GetBuildHistory().Num());
	RefreshDetails();
}

FReply UCombatBuildDetailsWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (HandleNavigationKey(InKeyEvent.GetKey()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UCombatBuildDetailsWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (HandleNavigationKey(InKeyEvent.GetKey()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UCombatBuildDetailsWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!RootCanvas)
	{
		RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (!RootCanvas)
		{
			if (WidgetTree->RootWidget)
			{
				UE_LOG(LogSkill, Warning,
					TEXT("Combat build details Blueprint root must be a CanvasPanel; native tree was not created."));
				return;
			}

			RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				TEXT("CombatBuildDetailsCanvas"));
			WidgetTree->RootWidget = RootCanvas;
		}
	}

	if (DetailsDesignBox)
	{
		return;
	}

	// Static panel content stays transparent to hit testing while slot and arrow
	// buttons remain interactive.
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// The details page is a modal overlay. Keep the scrim on the full viewport
	// canvas, behind the centered paper design, so it scales independently from
	// the 1200x860 book and never becomes part of the panel texture.
	if (!ScrimOverlay)
	{
		ScrimOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("CombatBuildDetailsScrim"));
		ScrimOverlay->SetBrushColor(ScrimColor);
		ScrimOverlay->SetPadding(FMargin(0.0f));
		ScrimOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* ScrimSlot = RootCanvas->AddChildToCanvas(ScrimOverlay))
		{
			ScrimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			ScrimSlot->SetOffsets(FMargin(0.0f));
			ScrimSlot->SetAlignment(FVector2D::ZeroVector);
			ScrimSlot->SetZOrder(0);
		}
	}

	DetailsSafeZone = WidgetTree->ConstructWidget<USafeZone>(
		USafeZone::StaticClass(),
		TEXT("DetailsSafeZone"));
	if (UCanvasPanelSlot* SafeZoneSlot = RootCanvas->AddChildToCanvas(DetailsSafeZone))
	{
		SafeZoneSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		SafeZoneSlot->SetOffsets(FMargin(0.0f));
		SafeZoneSlot->SetAlignment(FVector2D::ZeroVector);
		SafeZoneSlot->SetZOrder(100);
	}

	DetailsScaleBox = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(),
		TEXT("DetailsScaleBox"));
	DetailsScaleBox->SetStretch(EStretch::ScaleToFit);

	// UScaleBox does not expose alignment setters in UE 5.8. Put it in an
	// overlay slot so the design-size panel stays centered inside the safe area.
	UOverlay* SafeZoneOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("DetailsSafeZoneOverlay"));
	DetailsSafeZone->SetContent(SafeZoneOverlay);
	if (UOverlaySlot* ScaleBoxSlot = SafeZoneOverlay->AddChildToOverlay(DetailsScaleBox))
	{
		ScaleBoxSlot->SetHorizontalAlignment(HAlign_Center);
		ScaleBoxSlot->SetVerticalAlignment(VAlign_Center);
	}

	DetailsDesignBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("DetailsDesignSize"));
	DetailsDesignBox->SetWidthOverride(PanelWidth);
	DetailsDesignBox->SetHeightOverride(PanelHeight);
	DetailsScaleBox->SetContent(DetailsDesignBox);

	DetailsPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("DetailsPanel"));
	DetailsPanel->SetBrushColor(PanelFallbackColor);
	DetailsPanel->SetPadding(FMargin(0.0f));
	DetailsDesignBox->SetContent(DetailsPanel);

	DetailsPanelOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("DetailsPanelOverlay"));
	DetailsPanel->SetContent(DetailsPanelOverlay);

	PanelImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("DetailsPanelPaperFeibai"));
	if (!PanelTexture)
	{
		PanelTexture = LoadObject<UTexture2D>(nullptr, DetailsPanelTexturePath);
	}
	if (!SelectedWashTexture)
	{
		SelectedWashTexture = LoadObject<UTexture2D>(nullptr, DetailsSelectedWashTexturePath);
	}
	if (!DetailsFont)
	{
		DetailsFont = LoadObject<UFont>(nullptr, DetailsFontPath);
	}
	if (PanelTexture)
	{
		PanelImage->SetBrushFromTexture(PanelTexture, false);
		PanelImage->SetColorAndOpacity(FLinearColor::White);
		PanelImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		DetailsPanel->SetBrushColor(FLinearColor::Transparent);
	}
	else
	{
		PanelImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UOverlaySlot* PanelImageSlot = DetailsPanelOverlay->AddChildToOverlay(PanelImage))
	{
		PanelImageSlot->SetHorizontalAlignment(HAlign_Fill);
		PanelImageSlot->SetVerticalAlignment(VAlign_Fill);
	}

	PagesRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("DetailsPagesRow"));
	if (UOverlaySlot* PagesSlot = DetailsPanelOverlay->AddChildToOverlay(PagesRow))
	{
		PagesSlot->SetHorizontalAlignment(HAlign_Fill);
		PagesSlot->SetVerticalAlignment(VAlign_Fill);
		// Keep the runtime content inside the opaque page interiors. The paper
		// artwork's fold is at the design midpoint, so the left row budget must
		// end before x=600 instead of being allowed to fill across the fold.
		PagesSlot->SetPadding(FMargin(145.0f, 50.0f, 95.0f, 48.0f));
	}

	USizeBox* LeftPageBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("DetailsLeftPageSize"));
	LeftPageBox->SetWidthOverride(450.0f);
	LeftPageBox->SetHeightOverride(762.0f);
	UVerticalBox* LeftCategoryList = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("DetailsCategoryList"));
	LeftPageBox->SetContent(LeftCategoryList);
	if (UHorizontalBoxSlot* LeftPageSlot = PagesRow->AddChildToHorizontalBox(LeftPageBox))
	{
		LeftPageSlot->SetHorizontalAlignment(HAlign_Fill);
		LeftPageSlot->SetVerticalAlignment(VAlign_Fill);
		LeftPageSlot->SetPadding(FMargin(0.0f, 0.0f, 28.0f, 0.0f));
		FSlateChildSize PageSize(ESlateSizeRule::Automatic);
		LeftPageSlot->SetSize(PageSize);
	}

	CategorySlotRows.Reset();
	CategoryPreviousArrowTexts.Reset();
	CategoryNextArrowTexts.Reset();
	CategoryPreviousArrowBoxes.Reset();
	CategoryNextArrowBoxes.Reset();
	CategoryPreviousArrowButtons.Reset();
	CategoryNextArrowButtons.Reset();
	CategorySlotButtons.Reset();
	CategorySlotSelectionWashes.Reset();
	CategorySlotImages.Reset();
	CategorySlotPlaceholders.Reset();
	CategoryStartIndices.Init(0, DetailsCategoryCount);
	CategoryItemCounts.Init(0, DetailsCategoryCount);
	CategorySlotRows.Reserve(DetailsCategoryCount);
	CategoryPreviousArrowTexts.Reserve(DetailsCategoryCount);
	CategoryNextArrowTexts.Reserve(DetailsCategoryCount);
	CategoryPreviousArrowBoxes.Reserve(DetailsCategoryCount);
	CategoryNextArrowBoxes.Reserve(DetailsCategoryCount);
	CategoryPreviousArrowButtons.Reserve(DetailsCategoryCount);
	CategoryNextArrowButtons.Reserve(DetailsCategoryCount);
	CategorySlotButtons.Reserve(DetailsCategoryCount * DetailsSlotCount);
	CategorySlotSelectionWashes.Reserve(DetailsCategoryCount * DetailsSlotCount);
	CategorySlotImages.Reserve(DetailsCategoryCount * DetailsSlotCount);
	CategorySlotPlaceholders.Reserve(DetailsCategoryCount * DetailsSlotCount);
	for (int32 CategoryIndex = 0; CategoryIndex < DetailsCategoryCount; ++CategoryIndex)
	{
		BuildCategoryRow(CategoryFromIndex(CategoryIndex), LeftCategoryList, CategoryIndex);
	}

	USizeBox* RightPageBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("DetailsRightPageSize"));
	RightPageBox->SetWidthOverride(480.0f);
	RightPageBox->SetHeightOverride(762.0f);
	UVerticalBox* RightPage = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("DetailsRightPage"));
	RightPageBox->SetContent(RightPage);
	if (UHorizontalBoxSlot* RightPageSlot = PagesRow->AddChildToHorizontalBox(RightPageBox))
	{
		RightPageSlot->SetHorizontalAlignment(HAlign_Fill);
		RightPageSlot->SetVerticalAlignment(VAlign_Fill);
		FSlateChildSize PageSize(ESlateSizeRule::Automatic);
		RightPageSlot->SetSize(PageSize);
	}

	USizeBox* PreviewBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("SelectedBuildPreviewSize"));
	PreviewBox->SetWidthOverride(300.0f);
	PreviewBox->SetHeightOverride(300.0f);
	SelectedPreviewRoot = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("SelectedBuildPreview"));
	PreviewBox->SetContent(SelectedPreviewRoot);
	if (UVerticalBoxSlot* PreviewSlot = RightPage->AddChildToVerticalBox(PreviewBox))
	{
		PreviewSlot->SetHorizontalAlignment(HAlign_Center);
		PreviewSlot->SetVerticalAlignment(VAlign_Top);
		PreviewSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 20.0f));
	}

	SelectedWashImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("SelectedBuildWash"));
	if (SelectedWashTexture)
	{
		SelectedWashImage->SetBrushFromTexture(SelectedWashTexture, true);
		SelectedWashImage->SetColorAndOpacity(FLinearColor::White);
		SelectedWashImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		SelectedWashImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UOverlaySlot* WashSlot = SelectedPreviewRoot->AddChildToOverlay(SelectedWashImage))
	{
		WashSlot->SetHorizontalAlignment(HAlign_Center);
		WashSlot->SetVerticalAlignment(VAlign_Center);
		WashSlot->SetPadding(FMargin(14.0f));
	}

	SelectedIconImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("SelectedBuildIcon"));
	SelectedIconImage->SetDesiredSizeOverride(FVector2D(220.0f, 220.0f));
	SelectedIconImage->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* IconSlot = SelectedPreviewRoot->AddChildToOverlay(SelectedIconImage))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(38.0f));
	}

	SelectedIconPlaceholder = WidgetTree->ConstructWidget<UCombatBuildIconPlaceholderWidget>(
		UCombatBuildIconPlaceholderWidget::StaticClass(),
		TEXT("SelectedBuildIconPlaceholder"));
	SelectedIconPlaceholder->SetPlaceholderKind(ECombatBuildIconPlaceholderKind::Generic);
	SelectedIconPlaceholder->SetLineColor(FLinearColor(0.10f, 0.09f, 0.08f, 0.82f));
	SelectedIconPlaceholder->InnerPadding = 76.0f;
	SelectedIconPlaceholder->LineThickness = 5.0f;
	SelectedIconPlaceholder->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* PlaceholderSlot = SelectedPreviewRoot->AddChildToOverlay(SelectedIconPlaceholder))
	{
		PlaceholderSlot->SetHorizontalAlignment(HAlign_Fill);
		PlaceholderSlot->SetVerticalAlignment(VAlign_Fill);
		PlaceholderSlot->SetPadding(FMargin(34.0f));
	}

	SelectedBuildTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SelectedBuildTitle"));
	SetTextStyle(SelectedBuildTitle, 28, FLinearColor(0.09f, 0.075f, 0.06f, 0.96f));
	SelectedBuildTitle->SetJustification(ETextJustify::Center);
	SelectedBuildTitle->SetAutoWrapText(true);
	SelectedBuildTitle->SetWrapTextAt(430.0f);
	if (UVerticalBoxSlot* TitleSlot = RightPage->AddChildToVerticalBox(SelectedBuildTitle))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 12.0f));
	}

	SelectedBuildDescription = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SelectedBuildDescription"));
	SetTextStyle(SelectedBuildDescription, 18, FLinearColor(0.18f, 0.16f, 0.13f, 0.88f));
	SelectedBuildDescription->SetJustification(ETextJustify::Left);
	SelectedBuildDescription->SetAutoWrapText(true);
	SelectedBuildDescription->SetWrapTextAt(390.0f);
	if (UVerticalBoxSlot* DescriptionSlot = RightPage->AddChildToVerticalBox(SelectedBuildDescription))
	{
		DescriptionSlot->SetHorizontalAlignment(HAlign_Left);
		DescriptionSlot->SetPadding(FMargin(36.0f, 0.0f, 18.0f, 0.0f));
	}
}

void UCombatBuildDetailsWidget::BuildCategoryRow(
	ECombatBuildCategory Category,
	UVerticalBox* InCategoryList,
	int32 CategoryIndex)
{
	if (!InCategoryList)
	{
		return;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		*FString::Printf(TEXT("BuildCategoryRow_%d"), CategoryIndex));
	CategorySlotRows.Add(nullptr);
	if (UVerticalBoxSlot* RowSlot = InCategoryList->AddChildToVerticalBox(Row))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetVerticalAlignment(VAlign_Center);
		RowSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
		FSlateChildSize RowSize(ESlateSizeRule::Fill);
		RowSize.Value = 1.0f;
		RowSlot->SetSize(RowSize);
	}

	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		*FString::Printf(TEXT("BuildCategoryLabelSize_%d"), CategoryIndex));
	// Give four-character category labels enough room for one line while
	// keeping the complete five-slot row inside the left page.
	LabelBox->SetWidthOverride(112.0f);
	LabelBox->SetHeightOverride(100.0f);
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("BuildCategoryLabel_%d"), CategoryIndex));
	Label->SetText(FBuildPresentationResolver::GetCategoryLabel(Category));
	SetTextStyle(Label, 18, FLinearColor(0.13f, 0.105f, 0.08f, 0.92f));
	Label->SetJustification(ETextJustify::Left);
	Label->SetAutoWrapText(false);
	Label->SetWrapTextAt(112.0f);
	LabelBox->SetContent(Label);
	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelBox))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Left);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(16.0f, 0.0f, 4.0f, 0.0f));
	}

	USizeBox* PreviousArrowBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		*FString::Printf(TEXT("BuildPreviousArrowSize_%d"), CategoryIndex));
	PreviousArrowBox->SetWidthOverride(22.0f);
	PreviousArrowBox->SetHeightOverride(100.0f);
	UTextBlock* PreviousArrow = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("BuildPreviousArrow_%d"), CategoryIndex));
	PreviousArrow->SetText(FText::FromString(TEXT("<")));
	SetTextStyle(PreviousArrow, 28, FLinearColor(0.22f, 0.19f, 0.15f, 0.68f));
	PreviousArrow->SetJustification(ETextJustify::Center);
	PreviousArrow->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCombatBuildDetailsArrowButton* PreviousArrowButton = WidgetTree->ConstructWidget<UCombatBuildDetailsArrowButton>(
		UCombatBuildDetailsArrowButton::StaticClass(),
		*FString::Printf(TEXT("BuildPreviousArrowButton_%d"), CategoryIndex));
	PreviousArrowButton->SetStyle(MakeTransparentButtonStyle());
	PreviousArrowButton->SetBackgroundColor(FLinearColor::Transparent);
	PreviousArrowButton->SetColorAndOpacity(FLinearColor::White);
	PreviousArrowButton->SetContent(PreviousArrow);
	PreviousArrowBox->SetContent(PreviousArrowButton);
	CategoryPreviousArrowTexts.Add(PreviousArrow);
	CategoryPreviousArrowBoxes.Add(PreviousArrowBox);
	CategoryPreviousArrowButtons.Add(PreviousArrowButton);
	if (UHorizontalBoxSlot* PreviousArrowSlot = Row->AddChildToHorizontalBox(PreviousArrowBox))
	{
		PreviousArrowSlot->SetHorizontalAlignment(HAlign_Center);
		PreviousArrowSlot->SetVerticalAlignment(VAlign_Center);
	}

	UHorizontalBox* SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		*FString::Printf(TEXT("BuildCategorySlots_%d"), CategoryIndex));
	CategorySlotRows[CategoryIndex] = SlotRow;
	if (UHorizontalBoxSlot* SlotRowSlot = Row->AddChildToHorizontalBox(SlotRow))
	{
		SlotRowSlot->SetHorizontalAlignment(HAlign_Fill);
		SlotRowSlot->SetVerticalAlignment(VAlign_Center);
		FSlateChildSize SlotRowSize(ESlateSizeRule::Fill);
		SlotRowSize.Value = 1.0f;
		SlotRowSlot->SetSize(SlotRowSize);
	}

	for (int32 SlotIndex = 0; SlotIndex < DetailsSlotCount; ++SlotIndex)
	{
		USizeBox* SlotBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("BuildCategorySlotSize_%d_%d"), CategoryIndex, SlotIndex));
		// The label inset consumes 20 design pixels; reduce each slot by 6 so
		// labels, arrows, and all five Icon slots remain inside the left page.
		SlotBox->SetWidthOverride(52.0f);
		SlotBox->SetHeightOverride(58.0f);
		UOverlay* SlotRoot = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(),
			*FString::Printf(TEXT("BuildCategorySlot_%d_%d"), CategoryIndex, SlotIndex));

		// The circular wash is a separate art layer and must stay behind the
		// icon/placeholder so selection never creates a blue rectangle over ink.
		UImage* SelectionWash = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("BuildCategorySelectionWash_%d_%d"), CategoryIndex, SlotIndex));
		if (SelectedWashTexture)
		{
			SelectionWash->SetBrushFromTexture(SelectedWashTexture, true);
			SelectionWash->SetColorAndOpacity(FLinearColor::White);
		}
		SelectionWash->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* SelectionWashSlot = SlotRoot->AddChildToOverlay(SelectionWash))
		{
			SelectionWashSlot->SetHorizontalAlignment(HAlign_Fill);
			SelectionWashSlot->SetVerticalAlignment(VAlign_Fill);
			SelectionWashSlot->SetPadding(FMargin(0.0f));
		}

		UCombatBuildDetailsSlotButton* SlotButton = WidgetTree->ConstructWidget<UCombatBuildDetailsSlotButton>(
			UCombatBuildDetailsSlotButton::StaticClass(),
			*FString::Printf(TEXT("BuildCategorySlotButton_%d_%d"), CategoryIndex, SlotIndex));
		SlotButton->SetStyle(MakeTransparentButtonStyle());
		SlotButton->SetBackgroundColor(FLinearColor::Transparent);
		SlotButton->SetColorAndOpacity(FLinearColor::White);
		SlotButton->InitializeForDetails(this, CategoryIndex, SlotIndex);
		SlotButton->SetContent(SlotRoot);
		SlotBox->SetContent(SlotButton);
		if (UHorizontalBoxSlot* SlotBoxSlot = SlotRow->AddChildToHorizontalBox(SlotBox))
		{
			SlotBoxSlot->SetHorizontalAlignment(HAlign_Center);
			SlotBoxSlot->SetVerticalAlignment(VAlign_Center);
			SlotBoxSlot->SetPadding(FMargin(1.0f, 0.0f));
		}

		UImage* IconImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("BuildCategoryIcon_%d_%d"), CategoryIndex, SlotIndex));
		IconImage->SetColorAndOpacity(FLinearColor::White);
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* IconSlot = SlotRoot->AddChildToOverlay(IconImage))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
			IconSlot->SetPadding(FMargin(2.0f));
		}

		UCombatBuildIconPlaceholderWidget* Placeholder = WidgetTree->ConstructWidget<UCombatBuildIconPlaceholderWidget>(
			UCombatBuildIconPlaceholderWidget::StaticClass(),
			*FString::Printf(TEXT("BuildCategoryPlaceholder_%d_%d"), CategoryIndex, SlotIndex));
		Placeholder->SetPlaceholderKind(ECombatBuildIconPlaceholderKind::Generic);
		Placeholder->SetLineColor(FLinearColor(0.28f, 0.26f, 0.22f, 0.42f));
		Placeholder->LineThickness = 2.6f;
			Placeholder->InnerPadding = 10.0f;
		Placeholder->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* PlaceholderSlot = SlotRoot->AddChildToOverlay(Placeholder))
		{
			PlaceholderSlot->SetHorizontalAlignment(HAlign_Fill);
			PlaceholderSlot->SetVerticalAlignment(VAlign_Fill);
			PlaceholderSlot->SetPadding(FMargin(3.0f));
		}

		CategorySlotButtons.Add(SlotButton);
		CategorySlotSelectionWashes.Add(SelectionWash);
		CategorySlotImages.Add(IconImage);
		CategorySlotPlaceholders.Add(Placeholder);
	}

	USizeBox* NextArrowBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		*FString::Printf(TEXT("BuildNextArrowSize_%d"), CategoryIndex));
	NextArrowBox->SetWidthOverride(22.0f);
	NextArrowBox->SetHeightOverride(100.0f);
	UTextBlock* NextArrow = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("BuildNextArrow_%d"), CategoryIndex));
	NextArrow->SetText(FText::FromString(TEXT(">")));
	SetTextStyle(NextArrow, 28, FLinearColor(0.22f, 0.19f, 0.15f, 0.68f));
	NextArrow->SetJustification(ETextJustify::Center);
	NextArrow->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCombatBuildDetailsArrowButton* NextArrowButton = WidgetTree->ConstructWidget<UCombatBuildDetailsArrowButton>(
		UCombatBuildDetailsArrowButton::StaticClass(),
		*FString::Printf(TEXT("BuildNextArrowButton_%d"), CategoryIndex));
	NextArrowButton->SetStyle(MakeTransparentButtonStyle());
	NextArrowButton->SetBackgroundColor(FLinearColor::Transparent);
	NextArrowButton->SetColorAndOpacity(FLinearColor::White);
	NextArrowButton->SetContent(NextArrow);
	NextArrowBox->SetContent(NextArrowButton);
	CategoryNextArrowTexts.Add(NextArrow);
	CategoryNextArrowBoxes.Add(NextArrowBox);
	CategoryNextArrowButtons.Add(NextArrowButton);
	BindArrowCallbacks(PreviousArrowButton, NextArrowButton, CategoryIndex);
	if (UHorizontalBoxSlot* NextArrowSlot = Row->AddChildToHorizontalBox(NextArrowBox))
	{
		NextArrowSlot->SetHorizontalAlignment(HAlign_Center);
		NextArrowSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UCombatBuildDetailsWidget::BuildSortedCategoryItems(
	TArray<TArray<const FCombatBuildDetailsItem*>>& OutItems) const
{
	OutItems.Reset();
	OutItems.SetNum(DetailsCategoryCount);

	for (const FCombatBuildDetailsItem& Item : ViewModel.Items)
	{
		const int32 CategoryIndex = GetCategoryIndex(Item.Category);
		if (OutItems.IsValidIndex(CategoryIndex) && OutItems[CategoryIndex].Num() < 64)
		{
			OutItems[CategoryIndex].Add(&Item);
		}
	}

	for (TArray<const FCombatBuildDetailsItem*>& CategoryItems : OutItems)
	{
		CategoryItems.Sort([](const FCombatBuildDetailsItem& A, const FCombatBuildDetailsItem& B)
		{
			if (A.SortOrder != B.SortOrder)
			{
				return A.SortOrder < B.SortOrder;
			}
			return A.BuildId.LexicalLess(B.BuildId);
		});
	}
}

void UCombatBuildDetailsWidget::RefreshCategorySlots()
{
	if (CategorySlotImages.Num() != DetailsCategoryCount * DetailsSlotCount
		|| CategorySlotPlaceholders.Num() != DetailsCategoryCount * DetailsSlotCount
		|| CategorySlotButtons.Num() != DetailsCategoryCount * DetailsSlotCount
		|| CategorySlotSelectionWashes.Num() != DetailsCategoryCount * DetailsSlotCount)
	{
		return;
	}

	TArray<TArray<const FCombatBuildDetailsItem*>> ItemsByCategory;
	BuildSortedCategoryItems(ItemsByCategory);
	if (CategoryItemCounts.Num() != DetailsCategoryCount
		|| CategoryStartIndices.Num() != DetailsCategoryCount)
	{
		CategoryItemCounts.Init(0, DetailsCategoryCount);
		CategoryStartIndices.Init(0, DetailsCategoryCount);
	}

	for (int32 CategoryIndex = 0; CategoryIndex < DetailsCategoryCount; ++CategoryIndex)
	{
		CategoryItemCounts[CategoryIndex] = ItemsByCategory[CategoryIndex].Num();
		CategoryStartIndices[CategoryIndex] = FMath::Clamp(
			CategoryStartIndices[CategoryIndex],
			0,
			GetMaxCategoryStartIndex(CategoryIndex));
		const int32 StartIndex = CategoryStartIndices[CategoryIndex];

		for (int32 SlotIndex = 0; SlotIndex < DetailsSlotCount; ++SlotIndex)
		{
			const int32 WidgetIndex = CategoryIndex * DetailsSlotCount + SlotIndex;
			UImage* IconImage = CategorySlotImages[WidgetIndex];
			UCombatBuildIconPlaceholderWidget* Placeholder = CategorySlotPlaceholders[WidgetIndex];
			UCombatBuildDetailsSlotButton* SlotButton = CategorySlotButtons[WidgetIndex];
			const int32 ItemIndex = StartIndex + SlotIndex;
			const FCombatBuildDetailsItem* Item = ItemsByCategory[CategoryIndex].IsValidIndex(ItemIndex)
				? ItemsByCategory[CategoryIndex][ItemIndex]
				: nullptr;

			if (Item)
			{
				SlotButton->SetIsEnabled(true);
				SlotButton->SetVisibility(ESlateVisibility::Visible);
				const ECombatBuildIconPlaceholderKind PlaceholderKind =
					FBuildPresentationResolver::ResolvePlaceholderKind(Item->IconKey);
				Placeholder->SetPlaceholderKind(PlaceholderKind);
				Placeholder->SetLineColor(GetPlaceholderColor(Item->bOwned));

				if (UTexture2D* IconTexture = LoadBuildIcon(Item->IconKey))
				{
					IconImage->SetBrushFromTexture(IconTexture, true);
					// ViewModel contains acquired entries only, so every visible Icon
					// keeps its full ink density and the remaining slots stay empty.
					IconImage->SetColorAndOpacity(FLinearColor::White);
					IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
					Placeholder->SetVisibility(ESlateVisibility::Collapsed);
				}
				else
				{
					IconImage->SetVisibility(ESlateVisibility::Collapsed);
					Placeholder->SetVisibility(ESlateVisibility::HitTestInvisible);
				}
			}
			else
			{
				SlotButton->SetIsEnabled(false);
				SlotButton->SetVisibility(bShowWhiteboxEmptySlots
					? ESlateVisibility::HitTestInvisible
					: ESlateVisibility::Collapsed);
				IconImage->SetVisibility(ESlateVisibility::Collapsed);
				Placeholder->SetPlaceholderKind(ECombatBuildIconPlaceholderKind::Generic);
				Placeholder->SetLineColor(FLinearColor(0.35f, 0.33f, 0.28f, 0.20f));
				Placeholder->SetVisibility(bShowWhiteboxEmptySlots
					? ESlateVisibility::HitTestInvisible
					: ESlateVisibility::Collapsed);
			}
		}

		const bool bCanShowPrevious = StartIndex > 0;
		const bool bCanShowNext = StartIndex < GetMaxCategoryStartIndex(CategoryIndex);
		if (CategoryPreviousArrowTexts.IsValidIndex(CategoryIndex))
		{
			CategoryPreviousArrowTexts[CategoryIndex]->SetVisibility(bCanShowPrevious
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
		}
		if (CategoryNextArrowTexts.IsValidIndex(CategoryIndex))
		{
			CategoryNextArrowTexts[CategoryIndex]->SetVisibility(bCanShowNext
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
		}
		if (CategoryPreviousArrowButtons.IsValidIndex(CategoryIndex))
		{
			CategoryPreviousArrowButtons[CategoryIndex]->SetVisibility(bCanShowPrevious
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		}
		if (CategoryNextArrowButtons.IsValidIndex(CategoryIndex))
		{
			CategoryNextArrowButtons[CategoryIndex]->SetVisibility(bCanShowNext
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		}
		if (CategoryPreviousArrowBoxes.IsValidIndex(CategoryIndex))
		{
			CategoryPreviousArrowBoxes[CategoryIndex]->SetVisibility(bCanShowPrevious
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		}
		if (CategoryNextArrowBoxes.IsValidIndex(CategoryIndex))
		{
			CategoryNextArrowBoxes[CategoryIndex]->SetVisibility(bCanShowNext
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		}
	}

	RefreshSelectionVisuals();
}

bool UCombatBuildDetailsWidget::ShowPrevious(int32 CategoryIndex)
{
	if (!CategoryStartIndices.IsValidIndex(CategoryIndex)
		|| CategoryStartIndices[CategoryIndex] <= 0)
	{
		return false;
	}

	--CategoryStartIndices[CategoryIndex];
	RefreshCategorySlots();
	return true;
}

bool UCombatBuildDetailsWidget::ShowNext(int32 CategoryIndex)
{
	if (!CategoryStartIndices.IsValidIndex(CategoryIndex)
		|| CategoryStartIndices[CategoryIndex] >= GetMaxCategoryStartIndex(CategoryIndex))
	{
		return false;
	}

	++CategoryStartIndices[CategoryIndex];
	RefreshCategorySlots();
	return true;
}

int32 UCombatBuildDetailsWidget::GetCategoryStartIndex(int32 CategoryIndex) const
{
	return CategoryStartIndices.IsValidIndex(CategoryIndex)
		? CategoryStartIndices[CategoryIndex]
		: 0;
}

int32 UCombatBuildDetailsWidget::GetCategoryItemCount(int32 CategoryIndex) const
{
	return CategoryItemCounts.IsValidIndex(CategoryIndex)
		? CategoryItemCounts[CategoryIndex]
		: 0;
}

int32 UCombatBuildDetailsWidget::GetMaxCategoryStartIndex(int32 CategoryIndex) const
{
	const int32 TotalCount = GetCategoryItemCount(CategoryIndex);
	return FMath::Max(0, TotalCount - DetailsSlotCount);
}

void UCombatBuildDetailsWidget::BindArrowCallbacks(
	UButton* PreviousButton,
	UButton* NextButton,
	int32 CategoryIndex)
{
	if (!PreviousButton || !NextButton)
	{
		return;
	}

	switch (CategoryIndex)
	{
	case 0:
		PreviousButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandlePreviousArrow0);
		NextButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandleNextArrow0);
		break;
	case 1:
		PreviousButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandlePreviousArrow1);
		NextButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandleNextArrow1);
		break;
	case 2:
		PreviousButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandlePreviousArrow2);
		NextButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandleNextArrow2);
		break;
	case 3:
		PreviousButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandlePreviousArrow3);
		NextButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandleNextArrow3);
		break;
	case 4:
		PreviousButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandlePreviousArrow4);
		NextButton->OnClicked.AddDynamic(this, &UCombatBuildDetailsWidget::HandleNextArrow4);
		break;
	default:
		break;
	}
}

void UCombatBuildDetailsWidget::HandlePreviousArrow0()
{
	ShowPrevious(0);
}

void UCombatBuildDetailsWidget::HandlePreviousArrow1()
{
	ShowPrevious(1);
}

void UCombatBuildDetailsWidget::HandlePreviousArrow2()
{
	ShowPrevious(2);
}

void UCombatBuildDetailsWidget::HandlePreviousArrow3()
{
	ShowPrevious(3);
}

void UCombatBuildDetailsWidget::HandlePreviousArrow4()
{
	ShowPrevious(4);
}

void UCombatBuildDetailsWidget::HandleNextArrow0()
{
	ShowNext(0);
}

void UCombatBuildDetailsWidget::HandleNextArrow1()
{
	ShowNext(1);
}

void UCombatBuildDetailsWidget::HandleNextArrow2()
{
	ShowNext(2);
}

void UCombatBuildDetailsWidget::HandleNextArrow3()
{
	ShowNext(3);
}

void UCombatBuildDetailsWidget::HandleNextArrow4()
{
	ShowNext(4);
}

void UCombatBuildDetailsWidget::RefreshSelectionVisuals()
{
	if (CategorySlotSelectionWashes.Num() != DetailsCategoryCount * DetailsSlotCount)
	{
		return;
	}

	TArray<TArray<const FCombatBuildDetailsItem*>> ItemsByCategory;
	BuildSortedCategoryItems(ItemsByCategory);
	for (int32 CategoryIndex = 0; CategoryIndex < DetailsCategoryCount; ++CategoryIndex)
	{
		const int32 StartIndex = GetCategoryStartIndex(CategoryIndex);
		for (int32 SlotIndex = 0; SlotIndex < DetailsSlotCount; ++SlotIndex)
		{
			const int32 WidgetIndex = CategoryIndex * DetailsSlotCount + SlotIndex;
			const int32 ItemIndex = StartIndex + SlotIndex;
			const FCombatBuildDetailsItem* Item = ItemsByCategory[CategoryIndex].IsValidIndex(ItemIndex)
				? ItemsByCategory[CategoryIndex][ItemIndex]
				: nullptr;
			UImage* SelectionWash = CategorySlotSelectionWashes[WidgetIndex];
			if (SelectionWash)
			{
				SelectionWash->SetVisibility(Item && Item->BuildId == SelectedBuildId
					? ESlateVisibility::HitTestInvisible
					: ESlateVisibility::Collapsed);
			}
		}
	}
}

void UCombatBuildDetailsWidget::SelectItem(
	int32 CategoryIndex,
	int32 ItemIndex,
	bool bMoveKeyboardFocus)
{
	TArray<TArray<const FCombatBuildDetailsItem*>> ItemsByCategory;
	BuildSortedCategoryItems(ItemsByCategory);
	if (!ItemsByCategory.IsValidIndex(CategoryIndex)
		|| !ItemsByCategory[CategoryIndex].IsValidIndex(ItemIndex))
	{
		return;
	}

	const FCombatBuildDetailsItem* Item = ItemsByCategory[CategoryIndex][ItemIndex];
	if (!Item)
	{
		return;
	}

	SelectedBuildId = Item->BuildId;
	SelectedCategoryIndex = CategoryIndex;
	SelectedItemIndex = ItemIndex;
	RefreshSelectedPreview();
	RefreshSelectionVisuals();
	if (bMoveKeyboardFocus)
	{
		FocusSelectedSlot();
	}
}

void UCombatBuildDetailsWidget::HandleSlotClicked(int32 CategoryIndex, int32 SlotIndex)
{
	const int32 ItemIndex = GetCategoryStartIndex(CategoryIndex) + SlotIndex;
	SelectItem(CategoryIndex, ItemIndex, true);
}

void UCombatBuildDetailsWidget::HandleSlotHovered(int32 CategoryIndex, int32 SlotIndex)
{
	// Hover updates the same stable selection used by keyboard navigation, so
	// the preview and the next keyboard move never disagree about the active slot.
	const int32 ItemIndex = GetCategoryStartIndex(CategoryIndex) + SlotIndex;
	SelectItem(CategoryIndex, ItemIndex, true);
}

bool UCombatBuildDetailsWidget::MoveSelectionHorizontal(int32 Direction)
{
	if (Direction == 0)
	{
		return false;
	}

	TArray<TArray<const FCombatBuildDetailsItem*>> ItemsByCategory;
	BuildSortedCategoryItems(ItemsByCategory);
	if (!ItemsByCategory.IsValidIndex(SelectedCategoryIndex))
	{
		FocusFirstAvailableSlot();
		return true;
	}

	const TArray<const FCombatBuildDetailsItem*>& CategoryItems = ItemsByCategory[SelectedCategoryIndex];
	int32 CurrentIndex = INDEX_NONE;
	for (int32 Index = 0; Index < CategoryItems.Num(); ++Index)
	{
		if (CategoryItems[Index] && CategoryItems[Index]->BuildId == SelectedBuildId)
		{
			CurrentIndex = Index;
			break;
		}
	}
	if (CurrentIndex == INDEX_NONE)
	{
		FocusFirstAvailableSlot();
		return true;
	}

	const int32 StartIndex = GetCategoryStartIndex(SelectedCategoryIndex);
	const int32 MaxVisibleIndex = StartIndex + DetailsSlotCount - 1;
	if (Direction > 0 && CurrentIndex < StartIndex)
	{
		CurrentIndex = StartIndex - 1;
	}
	else if (Direction < 0 && CurrentIndex > MaxVisibleIndex)
	{
		CurrentIndex = MaxVisibleIndex + 1;
	}

	const int32 TargetIndex = CurrentIndex + Direction;
	if (!CategoryItems.IsValidIndex(TargetIndex))
	{
		return false;
	}

	if (TargetIndex > MaxVisibleIndex)
	{
		if (!ShowNext(SelectedCategoryIndex))
		{
			return false;
		}
	}
	else if (TargetIndex < StartIndex)
	{
		if (!ShowPrevious(SelectedCategoryIndex))
		{
			return false;
		}
	}

	SelectItem(SelectedCategoryIndex, TargetIndex, true);
	return true;
}

bool UCombatBuildDetailsWidget::MoveSelectionVertical(int32 Direction)
{
	if (Direction == 0)
	{
		return false;
	}

	TArray<TArray<const FCombatBuildDetailsItem*>> ItemsByCategory;
	BuildSortedCategoryItems(ItemsByCategory);
	if (!ItemsByCategory.IsValidIndex(SelectedCategoryIndex))
	{
		FocusFirstAvailableSlot();
		return true;
	}

	int32 CurrentItemIndex = INDEX_NONE;
	for (int32 ItemIndex = 0; ItemIndex < ItemsByCategory[SelectedCategoryIndex].Num(); ++ItemIndex)
	{
		const FCombatBuildDetailsItem* Item = ItemsByCategory[SelectedCategoryIndex][ItemIndex];
		if (Item && Item->BuildId == SelectedBuildId)
		{
			CurrentItemIndex = ItemIndex;
			break;
		}
	}
	if (CurrentItemIndex == INDEX_NONE)
	{
		FocusFirstAvailableSlot();
		return true;
	}

	const int32 CurrentStartIndex = GetCategoryStartIndex(SelectedCategoryIndex);
	const int32 LocalSlotIndex = FMath::Clamp(
		CurrentItemIndex - CurrentStartIndex,
		0,
		DetailsSlotCount - 1);
	int32 TargetCategoryIndex = SelectedCategoryIndex;
	for (int32 Attempt = 0; Attempt < DetailsCategoryCount; ++Attempt)
	{
		TargetCategoryIndex += Direction;
		if (TargetCategoryIndex < 0 || TargetCategoryIndex >= DetailsCategoryCount)
		{
			return false;
		}
		if (ItemsByCategory[TargetCategoryIndex].Num() > 0)
		{
			break;
		}
	}

	if (!ItemsByCategory.IsValidIndex(TargetCategoryIndex)
		|| ItemsByCategory[TargetCategoryIndex].Num() == 0)
	{
		return false;
	}

	const int32 TargetLocalSlot = FMath::Min(
		LocalSlotIndex,
		ItemsByCategory[TargetCategoryIndex].Num() - 1);
	const int32 TargetIndex = GetCategoryStartIndex(TargetCategoryIndex) + TargetLocalSlot;
	SelectItem(TargetCategoryIndex, TargetIndex, true);
	return true;
}

bool UCombatBuildDetailsWidget::HandleNavigationKey(const FKey& Key)
{
	if ((DetailsKey.IsValid() && Key == DetailsKey) || Key == EKeys::Escape)
	{
		if (ObservedPlayer)
		{
			ObservedPlayer->CloseCombatBuildDetails();
		}
		return true;
	}

	if (Key == EKeys::A || Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		return MoveSelectionHorizontal(-1);
	}
	if (Key == EKeys::D || Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		return MoveSelectionHorizontal(1);
	}
	if (Key == EKeys::W || Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
	{
		return MoveSelectionVertical(-1);
	}
	if (Key == EKeys::S || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
	{
		return MoveSelectionVertical(1);
	}

	return false;
}

void UCombatBuildDetailsWidget::FocusSelectedSlot()
{
	if (CategorySlotButtons.Num() == DetailsCategoryCount * DetailsSlotCount
		&& CategoryStartIndices.IsValidIndex(SelectedCategoryIndex))
	{
		const int32 SlotIndex = SelectedItemIndex - CategoryStartIndices[SelectedCategoryIndex];
		const int32 WidgetIndex = SelectedCategoryIndex * DetailsSlotCount + SlotIndex;
		if (SlotIndex >= 0 && SlotIndex < DetailsSlotCount
			&& CategorySlotButtons.IsValidIndex(WidgetIndex)
			&& CategorySlotButtons[WidgetIndex]
			&& CategorySlotButtons[WidgetIndex]->GetVisibility() == ESlateVisibility::Visible
			&& CategorySlotButtons[WidgetIndex]->GetIsEnabled())
		{
			CategorySlotButtons[WidgetIndex]->SetKeyboardFocus();
			return;
		}
	}

	SetKeyboardFocus();
}

void UCombatBuildDetailsWidget::FocusFirstAvailableSlot()
{
	TArray<TArray<const FCombatBuildDetailsItem*>> ItemsByCategory;
	BuildSortedCategoryItems(ItemsByCategory);
	if (ViewModel.Items.IsEmpty())
	{
		SetKeyboardFocus();
		return;
	}

	int32 TargetCategoryIndex = INDEX_NONE;
	int32 TargetItemIndex = INDEX_NONE;
	for (int32 CategoryIndex = 0; CategoryIndex < DetailsCategoryCount; ++CategoryIndex)
	{
		const int32 StartIndex = GetCategoryStartIndex(CategoryIndex);
		if (ItemsByCategory[CategoryIndex].IsValidIndex(StartIndex))
		{
			TargetCategoryIndex = CategoryIndex;
			TargetItemIndex = StartIndex;
			break;
		}
	}

	if (TargetCategoryIndex == INDEX_NONE)
	{
		for (int32 CategoryIndex = 0; CategoryIndex < DetailsCategoryCount; ++CategoryIndex)
		{
			if (!ItemsByCategory[CategoryIndex].IsEmpty())
			{
				TargetCategoryIndex = CategoryIndex;
				TargetItemIndex = 0;
				break;
			}
		}
	}

	if (TargetCategoryIndex == INDEX_NONE)
	{
		SetKeyboardFocus();
		return;
	}

	SelectedCategoryIndex = TargetCategoryIndex;
	SelectedItemIndex = TargetItemIndex;
	SelectItem(TargetCategoryIndex, TargetItemIndex, true);
}

void UCombatBuildDetailsWidget::RefreshSelectedPreview()
{
	TArray<TArray<const FCombatBuildDetailsItem*>> ItemsByCategory;
	BuildSortedCategoryItems(ItemsByCategory);

	const FCombatBuildDetailsItem* SelectedItem = nullptr;
	int32 ResolvedCategoryIndex = INDEX_NONE;
	int32 ResolvedItemIndex = INDEX_NONE;
	if (!SelectedBuildId.IsNone())
	{
		for (int32 CategoryIndex = 0; CategoryIndex < DetailsCategoryCount; ++CategoryIndex)
		{
			for (int32 ItemIndex = 0; ItemIndex < ItemsByCategory[CategoryIndex].Num(); ++ItemIndex)
			{
				const FCombatBuildDetailsItem* Item = ItemsByCategory[CategoryIndex][ItemIndex];
				if (Item && Item->BuildId == SelectedBuildId)
				{
					SelectedItem = Item;
					ResolvedCategoryIndex = CategoryIndex;
					ResolvedItemIndex = ItemIndex;
					break;
				}
			}
			if (SelectedItem)
			{
				break;
			}
		}
	}

	if (!SelectedItem)
	{
		for (int32 CategoryIndex = 0; CategoryIndex < DetailsCategoryCount; ++CategoryIndex)
		{
			const int32 StartIndex = GetCategoryStartIndex(CategoryIndex);
			if (ItemsByCategory[CategoryIndex].IsValidIndex(StartIndex))
			{
				SelectedItem = ItemsByCategory[CategoryIndex][StartIndex];
				ResolvedCategoryIndex = CategoryIndex;
				ResolvedItemIndex = StartIndex;
				break;
			}
		}
	}

	if (!SelectedItem && !ViewModel.Items.IsEmpty())
	{
		for (int32 CategoryIndex = 0; CategoryIndex < DetailsCategoryCount && !SelectedItem; ++CategoryIndex)
		{
			if (!ItemsByCategory[CategoryIndex].IsEmpty())
			{
				SelectedItem = ItemsByCategory[CategoryIndex][0];
				ResolvedCategoryIndex = CategoryIndex;
				ResolvedItemIndex = 0;
			}
		}
	}

	if (!SelectedItem)
	{
		SelectedBuildId = NAME_None;
		SelectedCategoryIndex = INDEX_NONE;
		SelectedItemIndex = INDEX_NONE;
		if (SelectedBuildTitle)
		{
			SelectedBuildTitle->SetText(NSLOCTEXT("BuildDetails", "Preview.EmptyTitle", "选择构筑"));
		}
		if (SelectedBuildDescription)
		{
			SelectedBuildDescription->SetText(NSLOCTEXT("BuildDetails", "Preview.EmptyDescription", "构筑详情将在数据接入后显示。"));
		}
		if (SelectedIconImage)
		{
			SelectedIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (SelectedIconPlaceholder)
		{
			SelectedIconPlaceholder->SetPlaceholderKind(ECombatBuildIconPlaceholderKind::Generic);
			SelectedIconPlaceholder->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (SelectedWashImage)
		{
			SelectedWashImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	SelectedBuildId = SelectedItem->BuildId;
	SelectedCategoryIndex = ResolvedCategoryIndex;
	SelectedItemIndex = ResolvedItemIndex;
	if (SelectedWashImage)
	{
		SelectedWashImage->SetVisibility(SelectedWashTexture
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (SelectedBuildTitle)
	{
		SelectedBuildTitle->SetText(SelectedItem->Title);
	}
	if (SelectedBuildDescription)
	{
		SelectedBuildDescription->SetText(SelectedItem->Description);
	}
	if (!SelectedIconPlaceholder || !SelectedIconImage)
	{
		return;
	}
	SelectedIconPlaceholder->SetPlaceholderKind(
		FBuildPresentationResolver::ResolvePlaceholderKind(SelectedItem->IconKey));
	SelectedIconPlaceholder->SetLineColor(GetPlaceholderColor(SelectedItem->bOwned));
	if (UTexture2D* IconTexture = LoadBuildIcon(SelectedItem->IconKey))
	{
		SelectedIconImage->SetBrushFromTexture(IconTexture, true);
		SelectedIconImage->SetColorAndOpacity(FLinearColor::White);
		SelectedIconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		SelectedIconPlaceholder->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		SelectedIconImage->SetVisibility(ESlateVisibility::Collapsed);
		SelectedIconPlaceholder->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UCombatBuildDetailsWidget::SetTextStyle(
	UTextBlock* TextBlock,
	int32 FontSize,
	const FLinearColor& Color) const
{
	if (!TextBlock)
	{
		return;
	}

	FSlateFontInfo FontInfo = TextBlock->GetFont();
	UFont* FontAsset = DetailsFont;
	if (!FontAsset)
	{
		FontAsset = LoadObject<UFont>(nullptr, DetailsFontPath);
	}
	if (FontAsset)
	{
		FontInfo.FontObject = FontAsset;
	}
	FontInfo.Size = FontSize;
	TextBlock->SetFont(FontInfo);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
}

UTexture2D* UCombatBuildDetailsWidget::LoadBuildIcon(FName IconKey)
{
	if (IconKey.IsNone())
	{
		return nullptr;
	}

	if (const TObjectPtr<UTexture2D>* CachedTexture = BuildIconCache.Find(IconKey))
	{
		return CachedTexture->Get();
	}

	if (const TObjectPtr<UTexture2D>* ConfiguredTexture = ConfiguredBuildIcons.Find(IconKey))
	{
		if (ConfiguredTexture->Get())
		{
			BuildIconCache.Add(IconKey, ConfiguredTexture->Get());
			return ConfiguredTexture->Get();
		}
	}

	UTexture2D* Texture = nullptr;
	const FString RedrawnPath = FBuildPresentationResolver::GetRedrawnIconObjectPath(IconKey);
	Texture = LoadObject<UTexture2D>(nullptr, *RedrawnPath);
	if (!Texture)
	{
		const FString LegacyPath = FBuildPresentationResolver::GetLegacyIconObjectPath(IconKey);
		Texture = LoadObject<UTexture2D>(nullptr, *LegacyPath);
	}
	if (Texture)
	{
		BuildIconCache.Add(IconKey, Texture);
	}
	return Texture;
}

int32 UCombatBuildDetailsWidget::GetCategoryIndex(ECombatBuildCategory Category)
{
	switch (Category)
	{
	case ECombatBuildCategory::BasicAttack:
		return 0;
	case ECombatBuildCategory::Projectile:
		return 1;
	case ECombatBuildCategory::QSkill:
		return 2;
	case ECombatBuildCategory::ESkill:
		return 3;
	case ECombatBuildCategory::General:
	default:
		return 4;
	}
}

UCombatBuildDetailsArrowButton::UCombatBuildDetailsArrowButton(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitIsFocusable(false);
}

UCombatBuildDetailsSlotButton::UCombatBuildDetailsSlotButton(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitIsFocusable(true);
}

void UCombatBuildDetailsSlotButton::InitializeForDetails(
	UCombatBuildDetailsWidget* InOwner,
	int32 InCategoryIndex,
	int32 InSlotIndex)
{
	OwnerWidget = InOwner;
	CategoryIndex = InCategoryIndex;
	SlotIndex = InSlotIndex;
	OnClicked.AddDynamic(this, &UCombatBuildDetailsSlotButton::HandleClicked);
	OnHovered.AddDynamic(this, &UCombatBuildDetailsSlotButton::HandleHovered);
}

void UCombatBuildDetailsSlotButton::HandleClicked()
{
	if (OwnerWidget)
	{
		OwnerWidget->HandleSlotClicked(CategoryIndex, SlotIndex);
	}
}

void UCombatBuildDetailsSlotButton::HandleHovered()
{
	if (OwnerWidget)
	{
		OwnerWidget->HandleSlotHovered(CategoryIndex, SlotIndex);
	}
}

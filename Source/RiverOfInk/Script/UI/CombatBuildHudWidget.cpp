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

	constexpr float LatestFeedbackDuration = 0.22f;
	constexpr float LatestFeedbackStartScale = 0.78f;

	FName MakeBuildIconKey(const FBuildHistoryEntry& Entry)
	{
		if (Entry.RewardType == ERoguelikeRewardType::ChangeSkillForm)
		{
			return Entry.NewSkillForm == EPlayerSkillForm::TwoStageArc
				? FName(TEXT("TwoStageArc"))
				: FName(TEXT("SkillForm"));
		}

		if (Entry.RewardType == ERoguelikeRewardType::GainSkill)
		{
			return Entry.SkillID == EPlayerSkillID::CircularSlash
				? FName(TEXT("CircularSlash"))
				: FName(TEXT("TripleProjectile"));
		}

		if (Entry.RewardType == ERoguelikeRewardType::UpgradeSkill)
		{
			switch (Entry.UpgradeType)
			{
			case ESkillUpgradeType::Cooldown:
				return FName(TEXT("Cooldown"));
			case ESkillUpgradeType::Damage:
				return FName(TEXT("Damage"));
			case ESkillUpgradeType::Mechanic:
			default:
				return FName(TEXT("ProjectileCount"));
			}
		}

		switch (Entry.ModifierID)
		{
		case ESkillModifierID::AddProjectile:
			return FName(TEXT("ProjectileCount"));
		case ESkillModifierID::InkGrenade:
			return FName(TEXT("InkGrenade"));
		case ESkillModifierID::ExtraExplosion:
			return FName(TEXT("ExtraExplosion"));
		case ESkillModifierID::TwinSlash:
			return FName(TEXT("TwinSlash"));
		case ESkillModifierID::NullRing:
			return FName(TEXT("ProjectileErase"));
		case ESkillModifierID::RadiusUp:
			return FName(TEXT("Radius"));
		case ESkillModifierID::CooldownDown:
			return FName(TEXT("Cooldown"));
		case ESkillModifierID::ProjectileHoming:
			return FName(TEXT("ProjectileHoming"));
		default:
			return FName(TEXT("ProjectileCount"));
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
	ApplyViewportLayout();
	BindSkillEvents();
	RefreshBuildHistory();

	UE_LOG(LogSkill, Log, TEXT("Combat build HUD bound to %s. History=%d."),
		*GetNameSafe(ObservedPlayer),
		ObservedSkillComponent ? ObservedSkillComponent->GetBuildHistory().Num() : 0);
}

void UCombatBuildHudWidget::SetDetailsKeyLabel(const FText& InKeyLabel)
{
	DetailsKeyLabel = InKeyLabel.IsEmpty() ? FText::FromString(TEXT("B")) : InKeyLabel;
	if (DetailsKeyText)
	{
		DetailsKeyText->SetText(DetailsKeyLabel);
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

	// Resolve the visible window before starting feedback so an interrupted
	// animation can never show a stale primary slot.
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

	UE_LOG(LogSkill, Verbose,
		TEXT("Combat build HUD history refreshed: Count=%d HasPrevious=%s."),
		History.Num(),
		Previous ? TEXT("true") : TEXT("false"));
}

void UCombatBuildHudWidget::ToggleBuildDetails()
{
	if (!IsValid(ObservedSkillComponent) || ObservedSkillComponent->GetBuildHistory().IsEmpty())
	{
		UE_LOG(LogSkill, Verbose, TEXT("Combat build detail request ignored: build history is empty."));
		return;
	}

	// The detail panel, pause, focus, and UIOnly input belong to a later
	// feature slice. Keep this method as a compatibility boundary for the
	// existing B binding without embedding that feature in the combat HUD.
	UE_LOG(LogSkill, Verbose,
		TEXT("Combat build detail request deferred: detail HUD is outside the current slice."));
}

void UCombatBuildHudWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CombatBuildHudCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CombatBuildHudSize"));
	RootSizeBox->SetWidthOverride(PanelWidth);
	RootSizeBox->SetHeightOverride(PanelHeight);
	RootSizeBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* RootSlot = RootCanvas->AddChildToCanvas(RootSizeBox))
	{
		// The outer UUserWidget owns the bottom-right viewport placement. Keep
		// this inner panel at the widget origin so the two layout systems cannot
		// apply the bottom-right offset twice in an editor-embedded viewport.
		RootSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		RootSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		RootSlot->SetPosition(FVector2D::ZeroVector);
		RootSlot->SetSize(FVector2D(PanelWidth, PanelHeight));
		RootSlot->SetAutoSize(false);
		RootSlot->SetZOrder(30);
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
		PanelBorder->SetBrushColor(FLinearColor::Transparent);
		PanelImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		// Slice 2 intentionally uses the plain border as a whitebox panel when
		// no final panel texture has been imported yet.
		PanelImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	PanelImage->SetColorAndOpacity(FLinearColor::White);
	if (UOverlaySlot* PanelSlot = PanelOverlay->AddChildToOverlay(PanelImage))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Fill);
		PanelSlot->SetVerticalAlignment(VAlign_Fill);
	}

	ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CombatBuildContentRow"));
	if (UOverlaySlot* ContentSlot = PanelOverlay->AddChildToOverlay(ContentRow))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(FMargin(28.0f, 18.0f, 28.0f, 44.0f));
	}

	RecentSlotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RecentBuildSlotSize"));
	RecentSlotBox->SetWidthOverride(300.0f);
	RecentSlotBox->SetHeightOverride(178.0f);
	RecentSlotRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RecentBuildSlot"));
	RecentSlotBox->SetContent(RecentSlotRoot);
	if (UHorizontalBoxSlot* RecentRowSlot = ContentRow->AddChildToHorizontalBox(RecentSlotBox))
	{
		RecentRowSlot->SetHorizontalAlignment(HAlign_Fill);
		RecentRowSlot->SetVerticalAlignment(VAlign_Center);
		FSlateChildSize RecentSize(ESlateSizeRule::Fill);
		RecentSize.Value = 1.65f;
		RecentRowSlot->SetSize(RecentSize);
	}

	UBorder* RecentFallback = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RecentBuildFallback"));
	RecentFallback->SetBrushColor(RecentFallbackColor);
	RecentFallback->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* RecentFallbackSlot = RecentSlotRoot->AddChildToOverlay(RecentFallback))
	{
		RecentFallbackSlot->SetHorizontalAlignment(HAlign_Fill);
		RecentFallbackSlot->SetVerticalAlignment(VAlign_Fill);
		RecentFallbackSlot->SetPadding(FMargin(8.0f));
	}

	RecentIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RecentBuildIcon"));
	RecentIconImage->SetColorAndOpacity(FLinearColor::White);
	RecentIconImage->SetDesiredSizeOverride(FVector2D(126.0f, 126.0f));
	RecentIconImage->SetVisibility(ESlateVisibility::Collapsed);
	UScaleBox* RecentIconFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("RecentBuildIconFit"));
	RecentIconFit->SetStretch(EStretch::ScaleToFit);
	RecentIconFit->SetContent(RecentIconImage);
	if (UOverlaySlot* RecentIconSlot = RecentSlotRoot->AddChildToOverlay(RecentIconFit))
	{
		RecentIconSlot->SetHorizontalAlignment(HAlign_Center);
		RecentIconSlot->SetVerticalAlignment(VAlign_Center);
		RecentIconSlot->SetPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f));
	}

	RecentIconPlaceholder = WidgetTree->ConstructWidget<UCombatBuildIconPlaceholderWidget>(
		UCombatBuildIconPlaceholderWidget::StaticClass(),
		TEXT("RecentBuildIconPlaceholder"));
	RecentIconPlaceholder->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* RecentPlaceholderSlot = RecentSlotRoot->AddChildToOverlay(RecentIconPlaceholder))
	{
		RecentPlaceholderSlot->SetHorizontalAlignment(HAlign_Fill);
		RecentPlaceholderSlot->SetVerticalAlignment(VAlign_Fill);
		RecentPlaceholderSlot->SetPadding(FMargin(18.0f, 12.0f, 18.0f, 12.0f));
	}

	PreviousSlotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PreviousBuildSlotSize"));
	PreviousSlotBox->SetWidthOverride(142.0f);
	PreviousSlotBox->SetHeightOverride(154.0f);
	PreviousSlotRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PreviousBuildSlot"));
	PreviousSlotBox->SetContent(PreviousSlotRoot);
	if (UHorizontalBoxSlot* PreviousRowSlot = ContentRow->AddChildToHorizontalBox(PreviousSlotBox))
	{
		PreviousRowSlot->SetHorizontalAlignment(HAlign_Fill);
		PreviousRowSlot->SetVerticalAlignment(VAlign_Center);
		PreviousRowSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
		FSlateChildSize PreviousSize(ESlateSizeRule::Fill);
		PreviousSize.Value = 0.85f;
		PreviousRowSlot->SetSize(PreviousSize);
	}

	UBorder* PreviousFallback = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviousBuildFallback"));
	PreviousFallback->SetBrushColor(PreviousFallbackColor);
	PreviousFallback->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* PreviousFallbackSlot = PreviousSlotRoot->AddChildToOverlay(PreviousFallback))
	{
		PreviousFallbackSlot->SetHorizontalAlignment(HAlign_Fill);
		PreviousFallbackSlot->SetVerticalAlignment(VAlign_Fill);
		PreviousFallbackSlot->SetPadding(FMargin(6.0f));
	}

	PreviousIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviousBuildIcon"));
	PreviousIconImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.78f));
	PreviousIconImage->SetDesiredSizeOverride(FVector2D(78.0f, 78.0f));
	PreviousIconImage->SetVisibility(ESlateVisibility::Collapsed);
	UScaleBox* PreviousIconFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("PreviousBuildIconFit"));
	PreviousIconFit->SetStretch(EStretch::ScaleToFit);
	PreviousIconFit->SetContent(PreviousIconImage);
	if (UOverlaySlot* PreviousIconSlot = PreviousSlotRoot->AddChildToOverlay(PreviousIconFit))
	{
		PreviousIconSlot->SetHorizontalAlignment(HAlign_Center);
		PreviousIconSlot->SetVerticalAlignment(VAlign_Center);
		PreviousIconSlot->SetPadding(FMargin(8.0f));
	}

	PreviousIconPlaceholder = WidgetTree->ConstructWidget<UCombatBuildIconPlaceholderWidget>(
		UCombatBuildIconPlaceholderWidget::StaticClass(),
		TEXT("PreviousBuildIconPlaceholder"));
	PreviousIconPlaceholder->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* PreviousPlaceholderSlot = PreviousSlotRoot->AddChildToOverlay(PreviousIconPlaceholder))
	{
		PreviousPlaceholderSlot->SetHorizontalAlignment(HAlign_Fill);
		PreviousPlaceholderSlot->SetVerticalAlignment(VAlign_Fill);
		PreviousPlaceholderSlot->SetPadding(FMargin(12.0f));
	}

	DetailsPromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuildDetailsPromptRow"));
	if (UOverlaySlot* PromptSlot = PanelOverlay->AddChildToOverlay(DetailsPromptRow))
	{
		PromptSlot->SetHorizontalAlignment(HAlign_Center);
		PromptSlot->SetVerticalAlignment(VAlign_Bottom);
		PromptSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
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
	}
}

void UCombatBuildHudWidget::ApplyViewportLayout()
{
	// AddToViewport fills the widget by default. Give the widget an explicit
	// desired size and place that size as a single bottom-right viewport slot;
	// this remains correct when the editor embeds PIE in a non-fullscreen pane.
	SetDesiredSizeInViewport(FVector2D(PanelWidth, PanelHeight));
	SetAnchorsInViewport(FAnchors(1.0f, 1.0f));
	SetAlignmentInViewport(FVector2D(1.0f, 1.0f));
	SetPositionInViewport(FVector2D(-32.0f, -28.0f));
}

void UCombatBuildHudWidget::BindSkillEvents()
{
	if (bSkillEventsSubscribed || !IsValid(ObservedSkillComponent))
	{
		return;
	}

	ObservedSkillComponent->OnBuildHistoryChanged.AddUObject(this, &UCombatBuildHudWidget::HandleBuildHistoryChanged);
	bSkillEventsSubscribed = true;
	UE_LOG(LogSkill, Log,
		TEXT("Combat build HUD subscribed to build-history changes: Component=%s Owner=%s."),
		*GetNameSafe(ObservedSkillComponent),
		*GetNameSafe(ObservedSkillComponent->GetOwner()));
}

void UCombatBuildHudWidget::UnbindSkillEvents()
{
	if (bSkillEventsSubscribed && IsValid(ObservedSkillComponent))
	{
		ObservedSkillComponent->OnBuildHistoryChanged.RemoveAll(this);
	}
	bSkillEventsSubscribed = false;
}

void UCombatBuildHudWidget::HandleBuildHistoryChanged()
{
	UE_LOG(LogSkill, Log,
		TEXT("Combat build HUD received build-history change: Component=%s Owner=%s."),
		*GetNameSafe(ObservedSkillComponent),
		ObservedSkillComponent ? *GetNameSafe(ObservedSkillComponent->GetOwner()) : TEXT("None"));
	RefreshBuildHistory();
}

void UCombatBuildHudWidget::SetBuildEntry(const FBuildHistoryEntry* Entry, bool bRecent)
{
	USizeBox* SlotBox = bRecent ? RecentSlotBox : PreviousSlotBox;
	UImage* IconImage = bRecent ? RecentIconImage : PreviousIconImage;
	UCombatBuildIconPlaceholderWidget* IconPlaceholder = bRecent ? RecentIconPlaceholder : PreviousIconPlaceholder;
	if (!SlotBox || !IconImage)
	{
		return;
	}

	if (!Entry)
	{
		SlotBox->SetVisibility(ESlateVisibility::Collapsed);
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
		if (IconPlaceholder)
		{
			IconPlaceholder->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	SlotBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	const FName BuildIconKey = ResolveBuildIconKey(*Entry);
	UTexture2D* IconTexture = LoadBuildIcon(*Entry);
	if (!IconTexture)
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
		IconImage->SetBrush(FSlateBrush());
		if (IconPlaceholder)
		{
			IconPlaceholder->SetPlaceholderKind(ResolvePlaceholderKind(BuildIconKey));
			IconPlaceholder->SetLineColor(bRecent
				? FLinearColor(0.035f, 0.03f, 0.025f, 0.94f)
				: FLinearColor(0.035f, 0.03f, 0.025f, 0.70f));
			IconPlaceholder->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		UE_LOG(LogSkill, Log,
			TEXT("Combat build HUD using geometry placeholder: BuildIconKey=%s Recent=%s."),
			*BuildIconKey.ToString(),
			bRecent ? TEXT("true") : TEXT("false"));
		return;
	}

	if (IconPlaceholder)
	{
		IconPlaceholder->SetVisibility(ESlateVisibility::Collapsed);
	}
	IconImage->SetBrushFromTexture(IconTexture, true);
	IconImage->SetColorAndOpacity(bRecent
		? FLinearColor::White
		: FLinearColor(1.0f, 1.0f, 1.0f, 0.78f));
	IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
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
	UWorld* World = GetWorld();
	if (!World)
	{
		// A widget can be constructed before it is attached to a world. Do not
		// leave the primary slot in its temporary animation state when there is
		// no timer source available yet.
		SetWidgetScale(RecentSlotRoot, 1.0f);
		RecentSlotRoot->SetRenderOpacity(1.0f);
		return;
	}

	LatestFeedbackStartTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(
		LatestFeedbackTimer,
		this,
		&UCombatBuildHudWidget::UpdateLatestBuildFeedback,
		1.0f / 60.0f,
		true);
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
	const FName BuildIconKey = ResolveBuildIconKey(Entry);
	const FString Stem = BuildIconKey.ToString();
	if (const TObjectPtr<UTexture2D>* ConfiguredTexturePtr = ConfiguredBuildIcons.Find(BuildIconKey))
	{
		if (UTexture2D* ConfiguredTexture = ConfiguredTexturePtr->Get())
		{
			return ConfiguredTexture;
		}
	}

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
	else if (BuildIconKey == FName(TEXT("ProjectileCount")))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileCount.T_UI_Build_ProjectileCount");
	}
	else if (BuildIconKey == FName(TEXT("InkGrenade")))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_InkGrenade.T_UI_Build_InkGrenade");
	}
	else if (BuildIconKey == FName(TEXT("ExtraExplosion")))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ExtraExplosion.T_UI_Build_ExtraExplosion");
	}
	else if (BuildIconKey == FName(TEXT("TwinSlash")))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_TwinSlash.T_UI_Build_TwinSlash");
	}
	else if (BuildIconKey == FName(TEXT("ProjectileErase")))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileErase.T_UI_Build_ProjectileErase");
	}
	else if (BuildIconKey == FName(TEXT("Radius")))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_Radius.T_UI_Build_Radius");
	}
	else if (BuildIconKey == FName(TEXT("Cooldown")))
	{
		FallbackPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_Cooldown.T_UI_Build_Cooldown");
	}

	return LoadOptionalTexture(FallbackCacheKey, FallbackPath);
}

FName UCombatBuildHudWidget::ResolveBuildIconKey(const FBuildHistoryEntry& Entry) const
{
	// Build history is value data; the stable enum identifiers are the only
	// source for icon resolution. Display text is never parsed here.
	return MakeBuildIconKey(Entry);
}

ECombatBuildIconPlaceholderKind UCombatBuildHudWidget::ResolvePlaceholderKind(FName BuildIconKey) const
{
	if (BuildIconKey == FName(TEXT("TwoStageArc")))
	{
		return ECombatBuildIconPlaceholderKind::TwoStageArc;
	}
	if (BuildIconKey == FName(TEXT("TwinSlash")))
	{
		return ECombatBuildIconPlaceholderKind::TwinSlash;
	}
	if (BuildIconKey == FName(TEXT("Cooldown")))
	{
		return ECombatBuildIconPlaceholderKind::Cooldown;
	}

	return ECombatBuildIconPlaceholderKind::Generic;
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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRewardOptionWidget.h"

#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "RiverOfInk.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float OptionWidth = 270.0f;
	constexpr float OptionHeight = 420.0f;
	constexpr float IconSize = 112.0f;
	constexpr float SelectionBrushWidth = 76.0f;
	constexpr float HoverScale = 1.03f;

	const TCHAR* SelectionBrushPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_SelectBrush.T_UI_Reward_SelectBrush");
	const TCHAR* HoverInkPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_HoverInk.T_UI_Reward_HoverInk");
	const TCHAR* SmallDividerPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_SmallDivider.T_UI_Reward_SmallDivider");

	FText GetSkillName(EPlayerSkillID SkillID)
	{
		switch (SkillID)
		{
		case EPlayerSkillID::TripleProjectile:
			return FText::FromString(TEXT("三连墨矢"));
		case EPlayerSkillID::CircularSlash:
			return FText::FromString(TEXT("环斩"));
		default:
			return FText::FromString(TEXT("技能"));
		}
	}

	FText GetRewardCategory(const FRoguelikeRewardOption& Option)
	{
		switch (Option.RewardType)
		{
		case ERoguelikeRewardType::Currency:
			return FText::FromString(TEXT("纯墨"));
		case ERoguelikeRewardType::Health:
			return FText::FromString(TEXT("生命恢复"));
		default:
			return FText::FromString(TEXT("技能构筑"));
		}
	}

	FText GetTargetSkill(const FRoguelikeRewardOption& Option)
	{
		if (!Option.TargetSkill.IsEmpty())
		{
			return Option.TargetSkill;
		}

		if (Option.RewardType == ERoguelikeRewardType::Currency || Option.RewardType == ERoguelikeRewardType::Health)
		{
			return FText::GetEmpty();
		}

		const TCHAR* InputSlot = Option.SkillID == EPlayerSkillID::CircularSlash ? TEXT("E") : TEXT("Q");
		return FText::FromString(FString::Printf(TEXT("%s  %s"), InputSlot, *GetSkillName(Option.SkillID).ToString()));
	}

	FText GetBuildType(const FRoguelikeRewardOption& Option)
	{
		if (!Option.BuildType.IsEmpty())
		{
			return Option.BuildType;
		}

		if (Option.RewardType == ERoguelikeRewardType::Currency || Option.RewardType == ERoguelikeRewardType::Health)
		{
			return FText::GetEmpty();
		}

		return FText::FromString(TEXT("强化构筑"));
	}

	FText GetPrimaryValue(const FRoguelikeRewardOption& Option)
	{
		if (!Option.PrimaryValue.IsEmpty())
		{
			return Option.PrimaryValue;
		}

		if (Option.RewardType == ERoguelikeRewardType::Currency)
		{
			return FText::FromString(FString::Printf(TEXT("+%d"), Option.CurrencyAmount > 0 ? Option.CurrencyAmount : Option.StackDelta));
		}

		if (Option.RewardType == ERoguelikeRewardType::Health)
		{
			return FText::FromString(FString::Printf(TEXT("+%.0f"), Option.HealthRestoreAmount));
		}

		if (!Option.OldValue.IsEmpty() || !Option.NewValue.IsEmpty())
		{
			return FText::FromString(FString::Printf(TEXT("%s → %s"),
				*Option.OldValue.ToString(), *Option.NewValue.ToString()));
		}

		if (FMath::IsNearlyZero(Option.BeforeValue) && FMath::IsNearlyZero(Option.AfterValue))
		{
			return FText::GetEmpty();
		}

		return FText::FromString(FString::Printf(TEXT("%.0f → %.0f"), Option.BeforeValue, Option.AfterValue));
	}

	FText GetDescription(const FRoguelikeRewardOption& Option)
	{
		if (!Option.ShortDescription.IsEmpty())
		{
			return Option.ShortDescription;
		}
		return Option.Description;
	}

	UTexture2D* LoadFallbackIcon(const FRoguelikeRewardOption& Option)
	{
		const TCHAR* IconPath = TEXT("/Game/RawContent/UI/Texture/Icon_Cooldown.Icon_Cooldown");
		if (Option.RewardType == ERoguelikeRewardType::Currency)
		{
			IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_PureInk.T_UI_Reward_PureInk");
		}
		else if (Option.RewardType == ERoguelikeRewardType::Health)
		{
			IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Reward_Health.T_UI_Reward_Health");
		}
		else if (Option.RewardType == ERoguelikeRewardType::Modifier)
		{
			switch (Option.ModifierID)
			{
			case ESkillModifierID::AddProjectile:
				IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileCount.T_UI_Build_ProjectileCount");
				break;
			case ESkillModifierID::InkGrenade:
				IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_InkGrenade.T_UI_Build_InkGrenade");
				break;
			case ESkillModifierID::ExtraExplosion:
				IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ExtraExplosion.T_UI_Build_ExtraExplosion");
				break;
			case ESkillModifierID::TwinSlash:
				IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_TwinSlash.T_UI_Build_TwinSlash");
				break;
			case ESkillModifierID::NullRing:
				IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_ProjectileErase.T_UI_Build_ProjectileErase");
				break;
			case ESkillModifierID::RadiusUp:
				IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_Radius.T_UI_Build_Radius");
				break;
			case ESkillModifierID::CooldownDown:
				IconPath = TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_Cooldown.T_UI_Build_Cooldown");
				break;
			default:
				break;
			}
		}
		else if (Option.SkillID == EPlayerSkillID::CircularSlash)
		{
			IconPath = TEXT("/Game/RawContent/UI/Texture/Icon_CircularSlash.Icon_CircularSlash");
		}
		else if (Option.SkillID == EPlayerSkillID::TripleProjectile)
		{
			IconPath = TEXT("/Game/RawContent/UI/Texture/Icon_TripleProjectile.Icon_TripleProjectile");
		}

		return LoadObject<UTexture2D>(nullptr, IconPath);
	}

	UWidgetAnimation* CreateTransformAnimation(
		UUserWidget* Owner,
		UWidget* Target,
		const FName& AnimationName,
		float Duration,
		const FVector2D& StartTranslation,
		const FVector2D& EndTranslation,
		const FVector2D& StartScale,
		const FVector2D& EndScale)
	{
		if (!Owner || !Target)
		{
			return nullptr;
		}

		const int32 FrameCount = FMath::Max(1, FMath::RoundToInt(Duration * 60.0f));
		UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(Owner, AnimationName);
		if (!Animation)
		{
			return nullptr;
		}

		Animation->MovieScene = NewObject<UMovieScene>(Animation, NAME_None);
		UMovieScene* MovieScene = Animation->MovieScene;
		MovieScene->SetTickResolutionDirectly(FFrameRate(60, 1));
		MovieScene->SetDisplayRate(FFrameRate(60, 1));
		MovieScene->SetPlaybackRange(FFrameNumber(0), FrameCount);

		const FGuid PossessableGuid = MovieScene->AddPossessable(Target->GetName(), Target->GetClass());
		Animation->BindPossessableObject(PossessableGuid, *Target, Owner);

		UMovieScene2DTransformTrack* Track = MovieScene->AddTrack<UMovieScene2DTransformTrack>(PossessableGuid);
		if (!Track)
		{
			return nullptr;
		}

		Track->SetPropertyNameAndPath(FName(TEXT("RenderTransform")), TEXT("RenderTransform"));
		UMovieScene2DTransformSection* Section = Cast<UMovieScene2DTransformSection>(Track->CreateNewSection());
		if (!Section)
		{
			return nullptr;
		}

		Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(FrameCount)));
		Section->SetMask(FMovieScene2DTransformMask(
			EMovieScene2DTransformChannel::Translation | EMovieScene2DTransformChannel::Scale));
		Section->Translation[0].AddLinearKey(FFrameNumber(0), StartTranslation.X);
		Section->Translation[0].AddLinearKey(FFrameNumber(FrameCount), EndTranslation.X);
		Section->Translation[1].AddLinearKey(FFrameNumber(0), StartTranslation.Y);
		Section->Translation[1].AddLinearKey(FFrameNumber(FrameCount), EndTranslation.Y);
		Section->Scale[0].AddLinearKey(FFrameNumber(0), StartScale.X);
		Section->Scale[0].AddLinearKey(FFrameNumber(FrameCount), EndScale.X);
		Section->Scale[1].AddLinearKey(FFrameNumber(0), StartScale.Y);
		Section->Scale[1].AddLinearKey(FFrameNumber(FrameCount), EndScale.Y);
		Track->AddSection(*Section);
		return Animation;
	}

	UWidgetAnimation* CreateOpacityAnimation(
		UUserWidget* Owner,
		UWidget* Target,
		const FName& AnimationName,
		float Duration,
		float StartOpacity,
		float EndOpacity)
	{
		if (!Owner || !Target)
		{
			return nullptr;
		}

		const int32 FrameCount = FMath::Max(1, FMath::RoundToInt(Duration * 60.0f));
		UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(Owner, AnimationName);
		if (!Animation)
		{
			return nullptr;
		}

		Animation->MovieScene = NewObject<UMovieScene>(Animation, NAME_None);
		UMovieScene* MovieScene = Animation->MovieScene;
		MovieScene->SetTickResolutionDirectly(FFrameRate(60, 1));
		MovieScene->SetDisplayRate(FFrameRate(60, 1));
		MovieScene->SetPlaybackRange(FFrameNumber(0), FrameCount);

		const FGuid PossessableGuid = MovieScene->AddPossessable(Target->GetName(), Target->GetClass());
		Animation->BindPossessableObject(PossessableGuid, *Target, Owner);

		UMovieSceneFloatTrack* Track = MovieScene->AddTrack<UMovieSceneFloatTrack>(PossessableGuid);
		if (!Track)
		{
			return nullptr;
		}

		Track->SetPropertyNameAndPath(FName(TEXT("RenderOpacity")), TEXT("RenderOpacity"));
		UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());
		if (!Section)
		{
			return nullptr;
		}

		Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(FrameCount)));
		Section->GetChannel().AddLinearKey(FFrameNumber(0), StartOpacity);
		Section->GetChannel().AddLinearKey(FFrameNumber(FrameCount), EndOpacity);
		Track->AddSection(*Section);
		return Animation;
	}
}

URoguelikeRewardOptionWidget::URoguelikeRewardOptionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> SelectionBrushFinder(SelectionBrushPath);
	if (SelectionBrushFinder.Succeeded())
	{
		SelectionBrushTexture = SelectionBrushFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UTexture2D> HoverInkFinder(HoverInkPath);
	if (HoverInkFinder.Succeeded())
	{
		HoverInkTexture = HoverInkFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UTexture2D> SmallDividerFinder(SmallDividerPath);
	if (SmallDividerFinder.Succeeded())
	{
		SmallDividerTexture = SmallDividerFinder.Object;
	}
}

TSharedRef<SWidget> URoguelikeRewardOptionWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void URoguelikeRewardOptionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	BuildDefaultWidgetTree();
	SetInteractionEnabled(true);
	SetHoverState(false);
	if (ImageSelectionBrush)
	{
		ImageSelectionBrush->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (HoverInkImage)
	{
		HoverInkImage->SetRenderOpacity(0.0f);
	}
	if (SmallDividerImage)
	{
		SmallDividerImage->SetRenderOpacity(0.55f);
	}
}

void URoguelikeRewardOptionWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionHoldTimer);
	}
	SelectionFinishedCallback.Unbind();
	OnOptionClicked.Unbind();
	OnOptionHovered.Unbind();
	OnOptionUnhovered.Unbind();
	Super::NativeDestruct();
}

void URoguelikeRewardOptionWidget::InitializeRewardOption(const FRoguelikeRewardOption& InOption, int32 InOptionIndex)
{
	BuildDefaultWidgetTree();
	RewardOption = InOption;
	OptionIndex = InOptionIndex;
	bSelectionPlaying = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionHoldTimer);
	}

	if (TextRewardCategory)
	{
		TextRewardCategory->SetText(GetRewardCategory(RewardOption));
		// The formal HUD hierarchy starts at icon/title. A separate category
		// label duplicates titles such as "纯墨" and adds no decision value.
		TextRewardCategory->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TextRewardTitle)
	{
		TextRewardTitle->SetText(RewardOption.Title.IsEmpty() ? GetRewardCategory(RewardOption) : RewardOption.Title);
	}
	if (TextSkillInfo)
	{
		TextSkillInfo->SetText(GetTargetSkill(RewardOption));
		TextSkillInfo->SetVisibility(GetTargetSkill(RewardOption).IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}
	if (TextBuildType)
	{
		TextBuildType->SetText(GetBuildType(RewardOption));
		TextBuildType->SetVisibility(GetBuildType(RewardOption).IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}
	if (TextValueChange)
	{
		TextValueChange->SetText(GetPrimaryValue(RewardOption));
		TextValueChange->SetVisibility(GetPrimaryValue(RewardOption).IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}
	if (TextDescription)
	{
		TextDescription->SetText(GetDescription(RewardOption));
	}

	if (ImageRewardIcon)
	{
		UTexture2D* Icon = RewardOption.RewardIcon
			? RewardOption.RewardIcon.Get()
			: LoadFallbackIcon(RewardOption);
		if (Icon)
		{
			ImageRewardIcon->SetBrushFromTexture(Icon, true);
			ImageRewardIcon->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ImageRewardIcon->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogRoguelike, Warning, TEXT("Reward option %d has no icon: Title=%s."), OptionIndex, *RewardOption.Title.ToString());
		}
	}

	SetInteractionEnabled(true);
	SetVisualScale(1.0f);
	SetRenderOpacity(1.0f);
	if (ImageSelectionBrush)
	{
		ImageSelectionBrush->SetVisibility(ESlateVisibility::Collapsed);
		ImageSelectionBrush->SetRenderOpacity(1.0f);
		ImageSelectionBrush->SetRenderTransform(FWidgetTransform());
	}
	SetHoverState(false);
	ForceLayoutPrepass();
}

bool URoguelikeRewardOptionWidget::PlaySelectionFeedback()
{
	if (bSelectionPlaying || !ImageSelectionBrush)
	{
		return false;
	}

	bSelectionPlaying = true;
	SetInteractionEnabled(false);
	SetHoverState(false);
	SetRenderOpacity(1.0f);
	ImageSelectionBrush->SetVisibility(ESlateVisibility::HitTestInvisible);
	ImageSelectionBrush->SetRenderOpacity(1.0f);
	ImageSelectionBrush->SetRenderTransform(FWidgetTransform());

	const float BrushTravel = OptionHeight + 24.0f;
	if (SelectionBrushAnimation)
	{
		ImageSelectionBrush->SetRenderTransform(FWidgetTransform(
			FVector2D(0.0f, -BrushTravel),
			FVector2D(1.0f, 1.0f),
			FVector2D::ZeroVector,
			0.0f));
		// This animation is created at runtime. Completion is intentionally
		// timer-driven below; binding a dynamic UMG delegate here requires a
		// reflected UFUNCTION and can assert in editor builds.
		PlayAnimation(SelectionBrushAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
	else
	{
		UE_LOG(LogRoguelike, Warning, TEXT("Reward selection animation unavailable for option %d; using completion timer."), OptionIndex);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SelectionHoldTimer,
			this,
			&URoguelikeRewardOptionWidget::HandleSelectionHoldFinished,
			FMath::Max(0.1f, SelectionSweepDuration + SelectionHoldDuration),
			false);
	}
	else
	{
		FinishSelectionFeedback();
	}
	return true;
}

void URoguelikeRewardOptionWidget::PlayFadeOut()
{
	SetInteractionEnabled(false);
	if (FadeOutAnimation)
	{
		PlayAnimation(FadeOutAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
	else
	{
		SetRenderOpacity(0.0f);
	}
}

void URoguelikeRewardOptionWidget::SetHoverState(bool bHovered)
{
	if (bSelectionPlaying)
	{
		return;
	}

	if (HoverInkImage)
	{
		HoverInkImage->SetRenderOpacity(bHovered ? 0.18f : 0.0f);
	}
	if (SmallDividerImage)
	{
		SmallDividerImage->SetRenderOpacity(bHovered ? 0.88f : 0.55f);
	}

	if (bHovered && HoverInAnimation)
	{
		PlayAnimation(HoverInAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
	else if (!bHovered && HoverOutAnimation)
	{
		PlayAnimation(HoverOutAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
	else if (!bHovered)
	{
		SetVisualScale(1.0f);
	}
}

void URoguelikeRewardOptionWidget::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;
	if (ButtonHitArea)
	{
		ButtonHitArea->SetIsEnabled(bEnabled);
	}
}

void URoguelikeRewardOptionWidget::FocusOption()
{
	if (ButtonHitArea && ButtonHitArea->GetVisibility() == ESlateVisibility::Visible)
	{
		ButtonHitArea->SetKeyboardFocus();
	}
}

void URoguelikeRewardOptionWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || RootSizeBox)
	{
		return;
	}

	RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RewardOptionSize"));
	RootSizeBox->SetWidthOverride(OptionWidth);
	RootSizeBox->SetHeightOverride(OptionHeight);
	WidgetTree->RootWidget = RootSizeBox;

	ButtonHitArea = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RewardOptionHitArea"));
	SetButtonStyle();
	RootSizeBox->AddChild(ButtonHitArea);

	UOverlay* OptionOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RewardOptionOverlay"));
	ButtonHitArea->SetContent(OptionOverlay);

	// The hover mark sits behind the content and only becomes visible for the
	// hovered option; it must never read as a card background.
	HoverInkImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardHoverInk"));
	if (!HoverInkTexture)
	{
		HoverInkTexture = LoadObject<UTexture2D>(nullptr, HoverInkPath);
	}
	if (HoverInkTexture)
	{
		HoverInkImage->SetBrushFromTexture(HoverInkTexture, true);
	}
	HoverInkImage->SetColorAndOpacity(FLinearColor::White);
	HoverInkImage->SetRenderOpacity(0.0f);
	HoverInkImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	HoverInkImage->SetDesiredSizeOverride(FVector2D(184.0f, 184.0f));
	if (UOverlaySlot* HoverSlot = OptionOverlay->AddChildToOverlay(HoverInkImage))
	{
		HoverSlot->SetHorizontalAlignment(HAlign_Center);
		HoverSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* ContentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RewardOptionContentSize"));
	ContentSize->SetWidthOverride(OptionWidth);
	ContentSize->SetHeightOverride(OptionHeight);
	ContentGroup = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RewardOptionContentGroup"));
	ContentSize->SetContent(ContentGroup);
	if (UOverlaySlot* ContentSlot = OptionOverlay->AddChildToOverlay(ContentSize))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Center);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}

	TextRewardCategory = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardCategory"));
	TextRewardCategory->SetJustification(ETextJustify::Center);
	SetTextStyle(TextRewardCategory, 14, FLinearColor(0.22f, 0.22f, 0.20f, 1.0f));
	if (UVerticalBoxSlot* VerticalSlot = ContentGroup->AddChildToVerticalBox(TextRewardCategory))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Center);
		VerticalSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 6.0f));
	}

	USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RewardIconSize"));
	IconSizeBox->SetWidthOverride(IconSize);
	IconSizeBox->SetHeightOverride(IconSize);
	ImageRewardIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardIcon"));
	ImageRewardIcon->SetColorAndOpacity(FLinearColor::White);
	IconSizeBox->SetContent(ImageRewardIcon);
	if (UVerticalBoxSlot* VerticalSlot = ContentGroup->AddChildToVerticalBox(IconSizeBox))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Center);
		VerticalSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 12.0f));
	}

	TextRewardTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardTitle"));
	TextRewardTitle->SetJustification(ETextJustify::Center);
	SetTextStyle(TextRewardTitle, 25, FLinearColor(0.035f, 0.030f, 0.025f, 1.0f));
	if (UVerticalBoxSlot* VerticalSlot = ContentGroup->AddChildToVerticalBox(TextRewardTitle))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Center);
		VerticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	TextSkillInfo = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardSkillInfo"));
	TextSkillInfo->SetJustification(ETextJustify::Center);
	SetTextStyle(TextSkillInfo, 17, FLinearColor(0.20f, 0.20f, 0.18f, 1.0f));
	if (UVerticalBoxSlot* VerticalSlot = ContentGroup->AddChildToVerticalBox(TextSkillInfo))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Center);
		VerticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}

	TextBuildType = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardBuildType"));
	TextBuildType->SetJustification(ETextJustify::Center);
	SetTextStyle(TextBuildType, 15, FLinearColor(0.25f, 0.25f, 0.22f, 1.0f));
	if (UVerticalBoxSlot* VerticalSlot = ContentGroup->AddChildToVerticalBox(TextBuildType))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Center);
		VerticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	TextValueChange = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardValueChange"));
	TextValueChange->SetJustification(ETextJustify::Center);
	SetTextStyle(TextValueChange, 21, FLinearColor(0.48f, 0.32f, 0.10f, 1.0f));
	if (UVerticalBoxSlot* VerticalSlot = ContentGroup->AddChildToVerticalBox(TextValueChange))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Center);
		VerticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	TextDescription = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardDescription"));
	TextDescription->SetJustification(ETextJustify::Center);
	TextDescription->SetAutoWrapText(true);
	TextDescription->SetWrapTextAt(230.0f);
	SetTextStyle(TextDescription, 16, FLinearColor(0.28f, 0.28f, 0.25f, 1.0f));
	if (UVerticalBoxSlot* VerticalSlot = ContentGroup->AddChildToVerticalBox(TextDescription))
	{
		VerticalSlot->SetHorizontalAlignment(HAlign_Center);
		VerticalSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 10.0f));
	}

	SmallDividerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardSmallDivider"));
	if (!SmallDividerTexture)
	{
		SmallDividerTexture = LoadObject<UTexture2D>(nullptr, SmallDividerPath);
	}
	if (SmallDividerTexture)
	{
		SmallDividerImage->SetBrushFromTexture(SmallDividerTexture, true);
	}
	SmallDividerImage->SetColorAndOpacity(FLinearColor::White);
	SmallDividerImage->SetRenderOpacity(0.55f);
	SmallDividerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	SmallDividerImage->SetDesiredSizeOverride(FVector2D(130.0f, 14.0f));
	if (UOverlaySlot* DividerSlot = OptionOverlay->AddChildToOverlay(SmallDividerImage))
	{
		DividerSlot->SetHorizontalAlignment(HAlign_Center);
		DividerSlot->SetVerticalAlignment(VAlign_Bottom);
		DividerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));
	}

	ImageSelectionBrush = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RewardSelectionBrush"));
	if (!SelectionBrushTexture)
	{
		SelectionBrushTexture = LoadObject<UTexture2D>(nullptr, SelectionBrushPath);
	}
	if (SelectionBrushTexture)
	{
		ImageSelectionBrush->SetBrushFromTexture(SelectionBrushTexture, true);
	}
	ImageSelectionBrush->SetColorAndOpacity(FLinearColor::White);
	ImageSelectionBrush->SetDesiredSizeOverride(FVector2D(SelectionBrushWidth, OptionHeight));
	ImageSelectionBrush->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* BrushSlot = OptionOverlay->AddChildToOverlay(ImageSelectionBrush))
	{
		BrushSlot->SetHorizontalAlignment(HAlign_Center);
		BrushSlot->SetVerticalAlignment(VAlign_Center);
	}

	ButtonHitArea->OnClicked.AddDynamic(this, &URoguelikeRewardOptionWidget::HandleButtonClicked);
	ButtonHitArea->OnHovered.AddDynamic(this, &URoguelikeRewardOptionWidget::HandleButtonHovered);
	ButtonHitArea->OnUnhovered.AddDynamic(this, &URoguelikeRewardOptionWidget::HandleButtonUnhovered);
	BuildAnimations();
}

void URoguelikeRewardOptionWidget::BuildAnimations()
{
	if (SelectionBrushAnimation || !RootSizeBox || !ImageSelectionBrush)
	{
		return;
	}

	SelectionBrushAnimation = CreateTransformAnimation(
		this,
		ImageSelectionBrush,
		TEXT("RewardSelectionBrushSweep"),
		SelectionSweepDuration,
		FVector2D(0.0f, -(OptionHeight + 24.0f)),
		FVector2D(0.0f, 0.0f),
		FVector2D(1.0f, 1.0f),
		FVector2D(1.0f, 1.0f));

	HoverInAnimation = CreateTransformAnimation(
		this,
		RootSizeBox,
		TEXT("RewardOptionHoverIn"),
		0.15f,
		FVector2D::ZeroVector,
		FVector2D::ZeroVector,
		FVector2D(1.0f, 1.0f),
		FVector2D(HoverScale, HoverScale));

	HoverOutAnimation = CreateTransformAnimation(
		this,
		RootSizeBox,
		TEXT("RewardOptionHoverOut"),
		0.15f,
		FVector2D::ZeroVector,
		FVector2D::ZeroVector,
		FVector2D(HoverScale, HoverScale),
		FVector2D(1.0f, 1.0f));

	FadeOutAnimation = CreateOpacityAnimation(
		this,
		RootSizeBox,
		TEXT("RewardOptionFadeOut"),
		FadeOutDuration,
		1.0f,
		0.0f);

	if (!SelectionBrushAnimation || !HoverInAnimation || !HoverOutAnimation || !FadeOutAnimation)
	{
		UE_LOG(LogRoguelike, Warning, TEXT("Reward option runtime animation setup incomplete: Option=%d Selection=%s HoverIn=%s HoverOut=%s Fade=%s."),
			OptionIndex,
			SelectionBrushAnimation ? TEXT("true") : TEXT("false"),
			HoverInAnimation ? TEXT("true") : TEXT("false"),
			HoverOutAnimation ? TEXT("true") : TEXT("false"),
			FadeOutAnimation ? TEXT("true") : TEXT("false"));
	}
}

void URoguelikeRewardOptionWidget::SetTextStyle(UTextBlock* TextBlock, int32 FontSize, const FLinearColor& Color) const
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

void URoguelikeRewardOptionWidget::SetButtonStyle()
{
	if (!ButtonHitArea)
	{
		return;
	}

	FButtonStyle Style = ButtonHitArea->GetStyle();
	Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
	ButtonHitArea->SetStyle(Style);
	ButtonHitArea->SetBackgroundColor(FLinearColor::Transparent);
}

void URoguelikeRewardOptionWidget::SetVisualScale(float Scale)
{
	if (!RootSizeBox)
	{
		return;
	}

	FWidgetTransform Transform;
	Transform.Scale = FVector2D(Scale, Scale);
	RootSizeBox->SetRenderTransform(Transform);
}

void URoguelikeRewardOptionWidget::FinishSelectionFeedback()
{
	if (!bSelectionPlaying)
	{
		return;
	}

	bSelectionPlaying = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionHoldTimer);
	}
	SelectionFinishedCallback.ExecuteIfBound();
}

void URoguelikeRewardOptionWidget::HandleSelectionHoldFinished()
{
	FinishSelectionFeedback();
}

void URoguelikeRewardOptionWidget::HandleFadeAnimationFinished()
{
	SetRenderOpacity(0.0f);
}

void URoguelikeRewardOptionWidget::HandleButtonClicked()
{
	if (bInteractionEnabled && !bSelectionPlaying && OptionIndex != INDEX_NONE)
	{
		OnOptionClicked.ExecuteIfBound(OptionIndex);
	}
}

void URoguelikeRewardOptionWidget::HandleButtonHovered()
{
	if (bInteractionEnabled && !bSelectionPlaying && OptionIndex != INDEX_NONE)
	{
		OnOptionHovered.ExecuteIfBound(OptionIndex);
	}
}

void URoguelikeRewardOptionWidget::HandleButtonUnhovered()
{
	if (bInteractionEnabled && !bSelectionPlaying && OptionIndex != INDEX_NONE)
	{
		OnOptionUnhovered.ExecuteIfBound(OptionIndex);
	}
}

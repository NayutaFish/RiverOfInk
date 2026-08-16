// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerSkillSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace
{
	static const FName CooldownProgressParameter(TEXT("CooldownProgress"));
	static const FName InkTextureParameter(TEXT("InkTexture"));
	static const TCHAR* CooldownMaterialPath = TEXT("/Game/Blueprint/GameSystem/UI/Skill/M_UI_SkillCooldown.M_UI_SkillCooldown");
	static const TCHAR* CooldownTexturePath = TEXT("/Game/RawContent/UI/Texture/T_UI_SkillCooldown_Ink.T_UI_SkillCooldown_Ink");

	constexpr float ReadyFeedbackDuration = 0.20f;
	constexpr float FadeOutDuration = 0.65f;
	constexpr float ReadyPulseScale = 1.04f;
}

TSharedRef<SWidget> UPlayerSkillSlotWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UPlayerSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(false);
	BuildDefaultWidgetTree();
	EnsureCooldownMaterial();
	// Ready slots are hidden independently. Starting a cooldown reveals only this slot.
	SetVisibility(ESlateVisibility::Collapsed);
	SetRenderOpacity(0.0f);
	SetVisualScale(1.0f);
	SetCooldownProgress(0.0f);
}

void UPlayerSkillSlotWidget::NativeDestruct()
{
	ClearFeedbackTimer();
	Super::NativeDestruct();
}

void UPlayerSkillSlotWidget::InitializeSkillSlot(EPlayerSkillID InSkillID, const FText& InKeyText)
{
	SkillID = InSkillID;
	SetKeyText(InKeyText);
	EnsureCooldownMaterial();
	UE_LOG(LogTemp, Verbose, TEXT("Skill slot initialized: Skill=%s Key=%s MID=%s."),
		*UEnum::GetValueAsString(SkillID),
		*InKeyText.ToString(),
		*GetNameSafe(CooldownMaterialInstance));
}

void UPlayerSkillSlotWidget::SetSkillIcon(UTexture2D* InSkillIcon)
{
	if (!ImageSkillIcon)
	{
		return;
	}

	ImageSkillIcon->SetBrushFromTexture(InSkillIcon, true);
	ImageSkillIcon->SetColorAndOpacity(SkillIconColor);
	if (!InSkillIcon)
	{
		UE_LOG(LogTemp, Warning, TEXT("Skill slot icon is missing for %s."), *UEnum::GetValueAsString(SkillID));
	}
}

void UPlayerSkillSlotWidget::SetKeyText(const FText& InKeyText)
{
	if (TextKey)
	{
		TextKey->SetText(InKeyText);
		TextKey->SetColorAndOpacity(KeyTextColor);
	}
}

void UPlayerSkillSlotWidget::StartCooldown(float InCooldownDuration)
{
	CooldownDuration = FMath::Max(0.0f, InCooldownDuration);
	bCooldownActive = CooldownDuration > KINDA_SMALL_NUMBER;
	SlotState = bCooldownActive ? EPlayerSkillHudSlotState::Cooldown : EPlayerSkillHudSlotState::Hidden;
	ClearFeedbackTimer();

	if (!bCooldownActive)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		SetRenderOpacity(0.0f);
		SetVisualScale(1.0f);
		SetCooldownProgress(0.0f);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(1.0f);
	SetVisualScale(1.0f);
	// The ring represents completed cooldown: a newly cast skill starts with no ink.
	SetCooldownProgress(0.0f);
}

void UPlayerSkillSlotWidget::UpdateCooldown(float InCooldownRemaining, float InCooldownDuration)
{
	const float SafeDuration = FMath::Max(0.0f, InCooldownDuration);
	const float SafeRemaining = FMath::Max(0.0f, InCooldownRemaining);

	if (SafeRemaining > KINDA_SMALL_NUMBER && SafeDuration > KINDA_SMALL_NUMBER)
	{
		if (!bCooldownActive || SlotState == EPlayerSkillHudSlotState::Hidden || SlotState == EPlayerSkillHudSlotState::ReadyFeedback || SlotState == EPlayerSkillHudSlotState::FadeOut)
		{
			StartCooldown(SafeDuration);
		}

		CooldownDuration = SafeDuration;
		SetCooldownProgress(1.0f - (SafeRemaining / SafeDuration));
		return;
	}

	if (bCooldownActive)
	{
		FinishCooldown();
	}
}

void UPlayerSkillSlotWidget::FinishCooldown()
{
	if (!bCooldownActive)
	{
		return;
	}

	bCooldownActive = false;
	SetCooldownProgress(1.0f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(1.0f);
	SetVisualScale(1.0f);
	SlotState = EPlayerSkillHudSlotState::ReadyFeedback;

	if (UWorld* World = GetWorld())
	{
		FeedbackStartTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(
			FeedbackTimer,
			this,
			&UPlayerSkillSlotWidget::UpdateReadyFeedback,
			0.016f,
			true);
	}
}

void UPlayerSkillSlotWidget::SetCooldownProgress(float InProgress)
{
	CooldownProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	EnsureCooldownMaterial();
	if (CooldownMaterialInstance)
	{
		CooldownMaterialInstance->SetScalarParameterValue(CooldownProgressParameter, CooldownProgress);
	}
}

void UPlayerSkillSlotWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSize"));
	SlotSizeBox->SetWidthOverride(SlotSize);
	SlotSizeBox->SetHeightOverride(SlotSize);
	WidgetTree->RootWidget = SlotSizeBox;

	OverlayRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Root"));
	SlotSizeBox->AddChild(OverlayRoot);

	CooldownRingBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CooldownRing"));
	CooldownRingBox->SetWidthOverride(CooldownInkSize);
	CooldownRingBox->SetHeightOverride(CooldownInkSize);
	ImageCooldownInk = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_CooldownInk"));
	ImageCooldownInk->SetColorAndOpacity(CooldownInkColor);
	CooldownRingBox->AddChild(ImageCooldownInk);
	if (UOverlaySlot* InkSlot = OverlayRoot->AddChildToOverlay(CooldownRingBox))
	{
		InkSlot->SetHorizontalAlignment(HAlign_Center);
		InkSlot->SetVerticalAlignment(VAlign_Center);
	}

	SkillIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SkillIcon"));
	SkillIconBox->SetWidthOverride(IconSize);
	SkillIconBox->SetHeightOverride(IconSize);
	ImageSkillIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_SkillIcon"));
	ImageSkillIcon->SetColorAndOpacity(SkillIconColor);
	SkillIconBox->AddChild(ImageSkillIcon);
	if (UOverlaySlot* IconSlot = OverlayRoot->AddChildToOverlay(SkillIconBox))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	TextKey = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Key"));
	TextKey->SetText(FText::FromString(TEXT("Q")));
	TextKey->SetColorAndOpacity(KeyTextColor);
	TextKey->SetJustification(ETextJustify::Center);
	FSlateFontInfo KeyFont = TextKey->GetFont();
	KeyFont.Size = KeyFontSize;
	TextKey->SetFont(KeyFont);
	if (UOverlaySlot* KeySlot = OverlayRoot->AddChildToOverlay(TextKey))
	{
		KeySlot->SetHorizontalAlignment(HAlign_Right);
		KeySlot->SetVerticalAlignment(VAlign_Bottom);
		KeySlot->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 1.0f));
	}
}

void UPlayerSkillSlotWidget::EnsureCooldownMaterial()
{
	if (CooldownMaterialInstance || !ImageCooldownInk)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = CooldownMaterial;
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, CooldownMaterialPath);
	}
	LoadedCooldownTexture = CooldownTexture;
	if (!LoadedCooldownTexture)
	{
		LoadedCooldownTexture = LoadObject<UTexture2D>(nullptr, CooldownTexturePath);
	}
	if (!BaseMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("Skill cooldown material missing: %s."), CooldownMaterialPath);
		return;
	}

	CooldownMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (!CooldownMaterialInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("Skill cooldown MID creation failed for %s."), *GetNameSafe(this));
		return;
	}

	if (LoadedCooldownTexture)
	{
		CooldownMaterialInstance->SetTextureParameterValue(InkTextureParameter, LoadedCooldownTexture);
	}
	CooldownMaterialInstance->SetScalarParameterValue(CooldownProgressParameter, CooldownProgress);
	ImageCooldownInk->SetBrushFromMaterial(CooldownMaterialInstance);
	ImageCooldownInk->SetColorAndOpacity(CooldownInkColor);
	UE_LOG(LogTemp, Verbose, TEXT("Skill cooldown MID ready: Slot=%s MID=%s Texture=%s."),
		*GetNameSafe(this),
		*GetNameSafe(CooldownMaterialInstance),
		*GetNameSafe(LoadedCooldownTexture));
}

void UPlayerSkillSlotWidget::SetVisualScale(float Scale)
{
	FWidgetTransform Transform;
	Transform.Scale = FVector2D(Scale, Scale);
	SetRenderTransform(Transform);
}

void UPlayerSkillSlotWidget::UpdateReadyFeedback()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		ClearFeedbackTimer();
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (SlotState == EPlayerSkillHudSlotState::ReadyFeedback)
	{
		const float Elapsed = Now - FeedbackStartTime;
		if (Elapsed < ReadyFeedbackDuration)
		{
			const float Alpha = FMath::Clamp(Elapsed / ReadyFeedbackDuration, 0.0f, 1.0f);
			SetVisualScale(1.0f + (ReadyPulseScale - 1.0f) * FMath::Sin(Alpha * PI));
			return;
		}

		SlotState = EPlayerSkillHudSlotState::FadeOut;
		FadeStartTime = Now;
		SetVisualScale(1.0f);
	}

	if (SlotState == EPlayerSkillHudSlotState::FadeOut)
	{
		const float FadeAlpha = FMath::Clamp((Now - FadeStartTime) / FadeOutDuration, 0.0f, 1.0f);
		SetRenderOpacity(1.0f - FadeAlpha);
		if (FadeAlpha >= 1.0f)
		{
			ClearFeedbackTimer();
			SlotState = EPlayerSkillHudSlotState::Hidden;
			SetVisibility(ESlateVisibility::Collapsed);
			SetRenderOpacity(0.0f);
			SetVisualScale(1.0f);
			SetCooldownProgress(0.0f);
		}
	}
}

void UPlayerSkillSlotWidget::ClearFeedbackTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FeedbackTimer);
	}
}

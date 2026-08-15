// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerHealthWidget.h"
#include "RiverOfInk.h"

#include "Blueprint/WidgetTree.h"
#include "Common/HealthComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SafeZone.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Player/PlayerCharacter.h"
#include "Styling/SlateTypes.h"

UPlayerHealthWidget::UPlayerHealthWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , HealthWidgetSize(340.0f, 34.0f)
    , HealthWidgetMargin(36.0f, 32.0f)
    , CurrentHealthColor(FLinearColor::FromSRGBColor(FColor(255, 255, 255, 255)))
    , RecentDamageColor(FLinearColor::FromSRGBColor(FColor(188, 64, 36, 255)))
    , EmptyHealthColor(FLinearColor::FromSRGBColor(FColor(42, 39, 37, 255)))
    , HealthFrameColor(FLinearColor::FromSRGBColor(FColor(18, 16, 15, 255)))
    , RecentDamageHoldTime(0.35f)
    , RecentDamageCollapseDuration(1.15f)
#if UE_BUILD_SHIPPING
    , bShowHealthDebugText(false)
#else
    , bShowHealthDebugText(true)
#endif
{
}

TSharedRef<SWidget> UPlayerHealthWidget::RebuildWidget()
{
    // Build the fallback tree before UUserWidget creates its Slate widget.
    // A WBP subclass with its own tree skips this path and binds the same names.
    BuildDefaultWidgetTree();
    return Super::RebuildWidget();
}

void UPlayerHealthWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BuildDefaultWidgetTree();
    ConfigureWidgetTree();
    UE_LOG(LogRiverOfInk, Log,
        TEXT("Health HUD tree: Empty=%s RecentDamage=%s Current=%s Frame=%s Debug=%s."),
        ProgressBar_EmptyHealth ? TEXT("valid") : TEXT("null"),
        ProgressBar_RecentDamage ? TEXT("valid") : TEXT("null"),
        ProgressBar_CurrentHealth ? TEXT("valid") : TEXT("null"),
        Image_HealthFrame ? TEXT("valid") : TEXT("null"),
        Text_HealthDebug ? TEXT("valid") : TEXT("null"));
}

void UPlayerHealthWidget::NativeDestruct()
{
    StopDamageTrailTimer();
    UnbindFromHealthComponent();
    ObservedPlayer = nullptr;
    Super::NativeDestruct();
}

void UPlayerHealthWidget::InitializeForPlayer(APlayerCharacter* InPlayer)
{
    UnbindFromHealthComponent();
    ObservedPlayer = InPlayer;

    if (!IsValid(ObservedPlayer) || !IsValid(ObservedPlayer->GetHealthComponent()))
    {
        ResetHealthState(0.0f, 1.0f);
        UE_LOG(LogRiverOfInk, Warning, TEXT("Health HUD initialization skipped: player or HealthComponent is invalid."));
        return;
    }

    UHealthComponent* HealthComponent = ObservedPlayer->GetHealthComponent();
    ResetHealthState(HealthComponent->GetCurrentHealth(), HealthComponent->GetMaxHealth());
    BindToHealthComponent(HealthComponent);
    UE_LOG(LogRiverOfInk, Log, TEXT("Health HUD bound directly to %s: HP=%.1f/%.1f."),
        *GetNameSafe(HealthComponent),
        HealthComponent->GetCurrentHealth(),
        HealthComponent->GetMaxHealth());
}

void UPlayerHealthWidget::RefreshHealth(float InMaxHealth, float InCurrentHealth)
{
    // This public method is a forced snapshot for existing Blueprint callers;
    // gameplay changes use HandleHealthChanged so damage/healing can animate correctly.
    ResetHealthState(InCurrentHealth, InMaxHealth);
}

void UPlayerHealthWidget::SetShowHealthDebugText(bool bInShow)
{
#if UE_BUILD_SHIPPING
    bShowHealthDebugText = false;
#else
    bShowHealthDebugText = bInShow;
#endif
    RefreshDebugText();
}

void UPlayerHealthWidget::BuildDefaultWidgetTree()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    RootSafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("HealthSafeZone"));
    RootSafeZone->SetSidesToPad(true, false, true, false);
    WidgetTree->RootWidget = RootSafeZone;

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HealthCanvas"));
    RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
    RootSafeZone->SetContent(RootCanvas);

    HealthSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_Health"));
    HealthSizeBox->SetWidthOverride(HealthWidgetSize.X);
    HealthSizeBox->SetHeightOverride(HealthWidgetSize.Y);
    HealthSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);

    if (UCanvasPanelSlot* HealthSlot = RootCanvas->AddChildToCanvas(HealthSizeBox))
    {
        HealthSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
        HealthSlot->SetAlignment(FVector2D::ZeroVector);
        HealthSlot->SetPosition(HealthWidgetMargin);
        HealthSlot->SetSize(HealthWidgetSize);
        HealthSlot->SetZOrder(10);
    }

    HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Health"));
    HealthSizeBox->SetContent(HealthOverlay);

    ProgressBar_EmptyHealth = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_EmptyHealth"));
    HealthOverlay->AddChildToOverlay(ProgressBar_EmptyHealth);

    ProgressBar_RecentDamage = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_RecentDamage"));
    HealthOverlay->AddChildToOverlay(ProgressBar_RecentDamage);

    ProgressBar_CurrentHealth = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_CurrentHealth"));
    HealthOverlay->AddChildToOverlay(ProgressBar_CurrentHealth);

    Image_HealthFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_HealthFrame"));
    HealthOverlay->AddChildToOverlay(Image_HealthFrame);

    Text_HealthDebug = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_HealthDebug"));
    Text_HealthDebug->SetJustification(ETextJustify::Center);
    Text_HealthDebug->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Text_HealthDebug->SetVisibility(ESlateVisibility::Collapsed);
    if (UOverlaySlot* DebugSlot = HealthOverlay->AddChildToOverlay(Text_HealthDebug))
    {
        DebugSlot->SetHorizontalAlignment(HAlign_Center);
        DebugSlot->SetVerticalAlignment(VAlign_Center);
    }
}

void UPlayerHealthWidget::ConfigureWidgetTree()
{
    if (HealthSizeBox)
    {
        HealthSizeBox->SetWidthOverride(HealthWidgetSize.X);
        HealthSizeBox->SetHeightOverride(HealthWidgetSize.Y);
    }

    ConfigureProgressBar(ProgressBar_EmptyHealth, EmptyHealthColor, 1.0f);
    ConfigureProgressBar(ProgressBar_RecentDamage, RecentDamageColor, DamageGhostRatio);
    ConfigureProgressBar(ProgressBar_CurrentHealth, CurrentHealthColor, CurrentHealthRatio);

    if (Image_HealthFrame)
    {
        FSlateBrush FrameBrush = FProgressBarStyle::GetDefault().FillImage;
        FrameBrush.DrawAs = ESlateBrushDrawType::Border;
        FrameBrush.Margin = FMargin(0.2f);
        FrameBrush.TintColor = FSlateColor(HealthFrameColor);
        Image_HealthFrame->SetBrush(FrameBrush);
        Image_HealthFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    RefreshDebugText();
    ApplyHealthVisuals();
}

void UPlayerHealthWidget::ConfigureProgressBar(UProgressBar* InProgressBar, const FLinearColor& InFillColor, float InPercent)
{
    if (!InProgressBar)
    {
        return;
    }

    FProgressBarStyle Style = FProgressBarStyle::GetDefault();
    Style.BackgroundImage = FSlateBrush();
    Style.BackgroundImage.DrawAs = ESlateBrushDrawType::NoDrawType;
    Style.FillImage.DrawAs = ESlateBrushDrawType::Box;
    Style.FillImage.Margin = FMargin(0.0f);
    Style.MarqueeImage = Style.FillImage;
    Style.EnableFillAnimation = false;

    InProgressBar->SetWidgetStyle(Style);
    InProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);
    InProgressBar->SetBarFillStyle(EProgressBarFillStyle::Scale);
    InProgressBar->SetIsMarquee(false);
    InProgressBar->SetBorderPadding(FVector2D::ZeroVector);
    InProgressBar->SetFillColorAndOpacity(InFillColor);
    InProgressBar->SetPercent(FMath::Clamp(InPercent, 0.0f, 1.0f));
    InProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPlayerHealthWidget::BindToHealthComponent(UHealthComponent* InHealthComponent)
{
    if (!IsValid(InHealthComponent))
    {
        return;
    }

    ObservedHealthComponent = InHealthComponent;
    ObservedHealthComponent->OnHealthChanged.AddDynamic(this, &UPlayerHealthWidget::HandleHealthChanged);
}

void UPlayerHealthWidget::UnbindFromHealthComponent()
{
    if (IsValid(ObservedHealthComponent))
    {
        ObservedHealthComponent->OnHealthChanged.RemoveDynamic(this, &UPlayerHealthWidget::HandleHealthChanged);
    }
    ObservedHealthComponent = nullptr;
}

void UPlayerHealthWidget::HandleHealthChanged(float InCurrentHealth, float InMaxHealth)
{
    const float SafeMaxHealth = FMath::Max(1.0f, InMaxHealth);
    const float SafeCurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, SafeMaxHealth);

    if (!bHasHealthBaseline)
    {
        ResetHealthState(SafeCurrentHealth, SafeMaxHealth);
        return;
    }

    const float OldCurrentHealth = LastKnownCurrentHealth;
    const bool bMaxHealthChanged = !FMath::IsNearlyEqual(SafeMaxHealth, LastKnownMaxHealth, KINDA_SMALL_NUMBER);
    LastKnownMaxHealth = SafeMaxHealth;
    LastKnownCurrentHealth = SafeCurrentHealth;

    // A capacity change is not damage. Resetting the visual ghost avoids a
    // false red trail when Rogue/runtime data changes MaxHealth.
    if (bMaxHealthChanged)
    {
        DamageGhostHealth = SafeCurrentHealth;
        DamageCollapseStartHealth = SafeCurrentHealth;
        DamageTrailElapsed = 0.0f;
        DamageTrailState = EHealthDamageTrailState::Idle;
        StopDamageTrailTimer();
    }
    else if (SafeCurrentHealth < OldCurrentHealth - KINDA_SMALL_NUMBER)
    {
        StartDamageTrail(OldCurrentHealth);
    }
    else if (SafeCurrentHealth > OldCurrentHealth + KINDA_SMALL_NUMBER)
    {
        // Healing advances the white bar immediately. It may not leave red over
        // health that has already been restored.
        if (SafeCurrentHealth >= DamageGhostHealth - KINDA_SMALL_NUMBER)
        {
            DamageGhostHealth = SafeCurrentHealth;
            DamageCollapseStartHealth = SafeCurrentHealth;
            DamageTrailState = EHealthDamageTrailState::Idle;
            DamageTrailElapsed = 0.0f;
            StopDamageTrailTimer();
        }
    }

    ApplyHealthVisuals();
    RefreshDebugText();
}

void UPlayerHealthWidget::ResetHealthState(float InCurrentHealth, float InMaxHealth)
{
    LastKnownMaxHealth = FMath::Max(1.0f, InMaxHealth);
    LastKnownCurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, LastKnownMaxHealth);
    DamageGhostHealth = LastKnownCurrentHealth;
    DamageCollapseStartHealth = LastKnownCurrentHealth;
    DamageTrailElapsed = 0.0f;
    DamageTrailState = EHealthDamageTrailState::Idle;
    bHasHealthBaseline = true;
    StopDamageTrailTimer();
    ApplyHealthVisuals();
    RefreshDebugText();
}

void UPlayerHealthWidget::ApplyHealthVisuals()
{
    CurrentHealthRatio = GetSafeHealthRatio(LastKnownCurrentHealth, LastKnownMaxHealth);
    DamageGhostHealth = FMath::Clamp(FMath::Max(DamageGhostHealth, LastKnownCurrentHealth), LastKnownCurrentHealth, LastKnownMaxHealth);
    DamageGhostRatio = GetSafeHealthRatio(DamageGhostHealth, LastKnownMaxHealth);

    if (ProgressBar_EmptyHealth)
    {
        ProgressBar_EmptyHealth->SetPercent(1.0f);
    }
    if (ProgressBar_RecentDamage)
    {
        ProgressBar_RecentDamage->SetPercent(DamageGhostRatio);
    }
    if (ProgressBar_CurrentHealth)
    {
        ProgressBar_CurrentHealth->SetPercent(CurrentHealthRatio);
    }
}

void UPlayerHealthWidget::RefreshDebugText()
{
    if (!Text_HealthDebug)
    {
        return;
    }

    if (!IsHealthDebugTextEnabled())
    {
        Text_HealthDebug->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    Text_HealthDebug->SetVisibility(ESlateVisibility::HitTestInvisible);
    Text_HealthDebug->SetText(FText::Format(
        NSLOCTEXT("PlayerHealthWidget", "HealthDebugFormat", "{0} / {1}"),
        FText::AsNumber(FMath::RoundToInt(LastKnownCurrentHealth)),
        FText::AsNumber(FMath::RoundToInt(LastKnownMaxHealth))));
}

void UPlayerHealthWidget::StartDamageTrail(float OldCurrentHealth)
{
    // Preserve the currently displayed ghost when another hit arrives during
    // Hold or Collapse; it must never jump backward toward the white bar.
    DamageGhostHealth = FMath::Max(DamageGhostHealth, OldCurrentHealth);
    DamageCollapseStartHealth = DamageGhostHealth;
    DamageTrailElapsed = 0.0f;
    DamageTrailState = RecentDamageHoldTime > KINDA_SMALL_NUMBER
        ? EHealthDamageTrailState::Hold
        : EHealthDamageTrailState::Collapse;

    if (DamageTrailState == EHealthDamageTrailState::Collapse && RecentDamageCollapseDuration <= KINDA_SMALL_NUMBER)
    {
        DamageGhostHealth = LastKnownCurrentHealth;
        DamageTrailState = EHealthDamageTrailState::Idle;
        StopDamageTrailTimer();
        return;
    }

    StartDamageTrailTimer();
}

void UPlayerHealthWidget::AdvanceDamageTrail()
{
    if (DamageTrailState == EHealthDamageTrailState::Idle)
    {
        StopDamageTrailTimer();
        return;
    }

    DamageTrailElapsed += DamageTrailUpdateInterval;

    if (DamageTrailState == EHealthDamageTrailState::Hold)
    {
        if (DamageTrailElapsed < RecentDamageHoldTime)
        {
            return;
        }

        DamageTrailState = EHealthDamageTrailState::Collapse;
        DamageTrailElapsed = 0.0f;
        DamageCollapseStartHealth = DamageGhostHealth;
    }

    if (DamageTrailState == EHealthDamageTrailState::Collapse)
    {
        if (RecentDamageCollapseDuration <= KINDA_SMALL_NUMBER)
        {
            DamageGhostHealth = LastKnownCurrentHealth;
            DamageTrailState = EHealthDamageTrailState::Idle;
            StopDamageTrailTimer();
        }
        else
        {
            const float Alpha = FMath::Clamp(DamageTrailElapsed / RecentDamageCollapseDuration, 0.0f, 1.0f);
            const float EaseOutAlpha = 1.0f - FMath::Square(1.0f - Alpha);
            DamageGhostHealth = FMath::Lerp(DamageCollapseStartHealth, LastKnownCurrentHealth, EaseOutAlpha);
            DamageGhostHealth = FMath::Max(DamageGhostHealth, LastKnownCurrentHealth);

            if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
            {
                DamageGhostHealth = LastKnownCurrentHealth;
                DamageTrailState = EHealthDamageTrailState::Idle;
                StopDamageTrailTimer();
            }
        }
    }

    ApplyHealthVisuals();
}

void UPlayerHealthWidget::StopDamageTrail()
{
    DamageGhostHealth = LastKnownCurrentHealth;
    DamageCollapseStartHealth = LastKnownCurrentHealth;
    DamageTrailElapsed = 0.0f;
    DamageTrailState = EHealthDamageTrailState::Idle;
    StopDamageTrailTimer();
    ApplyHealthVisuals();
}

void UPlayerHealthWidget::StartDamageTrailTimer()
{
    if (DamageTrailTimerHandle.IsValid())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DamageTrailTimerHandle,
            FTimerDelegate::CreateUObject(this, &UPlayerHealthWidget::AdvanceDamageTrail),
            DamageTrailUpdateInterval,
            true);
    }
}

void UPlayerHealthWidget::StopDamageTrailTimer()
{
    if (DamageTrailTimerHandle.IsValid())
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(DamageTrailTimerHandle);
        }
        DamageTrailTimerHandle.Invalidate();
    }
}

bool UPlayerHealthWidget::IsHealthDebugTextEnabled() const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return bShowHealthDebugText;
#endif
}

float UPlayerHealthWidget::GetSafeHealthRatio(float InCurrentHealth, float InMaxHealth)
{
    const float SafeMaxHealth = FMath::Max(1.0f, InMaxHealth);
    return FMath::Clamp(InCurrentHealth / SafeMaxHealth, 0.0f, 1.0f);
}

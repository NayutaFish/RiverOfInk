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
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Player/PlayerCharacter.h"
#include "Styling/SlateTypes.h"

UPlayerHealthWidget::UPlayerHealthWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , HealthWidgetSize(350.0f, 30.0f)
    , HealthWidgetMargin(36.0f, 32.0f)
    , HealthBarInset(2.0f)
    , HealthFrameExpansion(8.0f, 5.0f)
    , CurrentHealthColor(FLinearColor::FromSRGBColor(FColor(255, 255, 255, 255)))
    , RecentDamageColor(FLinearColor::FromSRGBColor(FColor(188, 64, 36, 255)))
    , EmptyHealthColor(FLinearColor::FromSRGBColor(FColor(42, 39, 37, 255)))
    , HealthFrameColor(FLinearColor::FromSRGBColor(FColor(18, 16, 15, 255)))
    , HealthFrameTexture(nullptr)
    , HealthPaperTexture(nullptr)
    , HealthFrameSliceFraction(0.12f, 0.0f)
    , HealthFrameHorizontalUVCrop(0.06f, 0.0f)
    , HealthTextColor(FLinearColor::FromSRGBColor(FColor(255, 255, 255, 255)))
    , RecentDamageHoldTime(0.35f)
    , RecentDamageCollapseDuration(1.15f)
#if UE_BUILD_SHIPPING
    , bShowHealthDebugText(false)
#else
    , bShowHealthDebugText(true)
#endif
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> FrameTexture(TEXT("/Game/RawContent/UI/Health/Textures/T_UI_PlayerHealth_Frame.T_UI_PlayerHealth_Frame"));
    if (FrameTexture.Succeeded())
    {
        HealthFrameTexture = FrameTexture.Object;
    }

    static ConstructorHelpers::FObjectFinder<UTexture2D> PaperTexture(TEXT("/Game/RawContent/UI/Health/Textures/T_UI_PlayerHealth_Paper.T_UI_PlayerHealth_Paper"));
    if (PaperTexture.Succeeded())
    {
        HealthPaperTexture = PaperTexture.Object;
    }
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

    HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_HealthRoot"));
    HealthSizeBox->SetContent(HealthOverlay);

    HealthBarLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_HealthBar"));
    HealthFrameLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_HealthFrame"));

    auto AddOverlayChild = [](UOverlay* InParent, UWidget* InWidget, const FMargin& InPadding) -> UOverlaySlot*
    {
        if (!InParent || !InWidget)
        {
            return nullptr;
        }

        UOverlaySlot* Slot = InParent->AddChildToOverlay(InWidget);
        if (Slot)
        {
            Slot->SetHorizontalAlignment(HAlign_Fill);
            Slot->SetVerticalAlignment(VAlign_Fill);
            Slot->SetPadding(InPadding);
        }
        return Slot;
    };

    const FVector2D ClampedFrameExpansion(
        FMath::Max(0.0f, HealthFrameExpansion.X),
        FMath::Max(0.0f, HealthFrameExpansion.Y));
    const FMargin FrameLayerPadding(
        -ClampedFrameExpansion.X, -ClampedFrameExpansion.Y,
        -ClampedFrameExpansion.X, -ClampedFrameExpansion.Y);

    AddOverlayChild(HealthOverlay, HealthBarLayer, HealthBarInset);
    AddOverlayChild(HealthOverlay, HealthFrameLayer, FrameLayerPadding);

    ProgressBar_EmptyHealth = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_EmptyHealth"));
    AddOverlayChild(HealthBarLayer, ProgressBar_EmptyHealth, FMargin(0.0f));

    ProgressBar_RecentDamage = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_RecentDamage"));
    AddOverlayChild(HealthBarLayer, ProgressBar_RecentDamage, FMargin(0.0f));

    ProgressBar_CurrentHealth = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar_CurrentHealth"));
    AddOverlayChild(HealthBarLayer, ProgressBar_CurrentHealth, FMargin(0.0f));

    Image_HealthFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_HealthFrame"));
    AddOverlayChild(HealthFrameLayer, Image_HealthFrame, FMargin(0.0f));

    Text_HealthDebug = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_HealthDebug"));
    Text_HealthDebug->SetJustification(ETextJustify::Center);
    Text_HealthDebug->SetColorAndOpacity(FSlateColor(HealthTextColor));
    Text_HealthDebug->SetVisibility(ESlateVisibility::Collapsed);
    if (UOverlaySlot* DebugSlot = AddOverlayChild(HealthOverlay, Text_HealthDebug, FMargin(0.0f)))
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

        if (UCanvasPanelSlot* HealthSlot = Cast<UCanvasPanelSlot>(HealthSizeBox->Slot))
        {
            HealthSlot->SetSize(HealthWidgetSize);
        }
    }

    if (UOverlaySlot* HealthBarLayerSlot = Cast<UOverlaySlot>(HealthBarLayer ? HealthBarLayer->Slot : nullptr))
    {
        HealthBarLayerSlot->SetPadding(HealthBarInset);
    }

    if (UOverlaySlot* HealthFrameLayerSlot = Cast<UOverlaySlot>(HealthFrameLayer ? HealthFrameLayer->Slot : nullptr))
    {
        const FVector2D ClampedFrameExpansion(
            FMath::Max(0.0f, HealthFrameExpansion.X),
            FMath::Max(0.0f, HealthFrameExpansion.Y));
        HealthFrameLayerSlot->SetPadding(FMargin(
            -ClampedFrameExpansion.X, -ClampedFrameExpansion.Y,
            -ClampedFrameExpansion.X, -ClampedFrameExpansion.Y));
    }

    ConfigureProgressBar(ProgressBar_EmptyHealth, EmptyHealthColor, 1.0f);
    ConfigureProgressBar(ProgressBar_RecentDamage, RecentDamageColor, DamageGhostRatio);
    ConfigureProgressBar(ProgressBar_CurrentHealth, CurrentHealthColor, CurrentHealthRatio, HealthPaperTexture);

    if (Image_HealthFrame)
    {
        FSlateBrush FrameBrush;
        if (HealthFrameTexture)
        {
            FrameBrush.SetResourceObject(HealthFrameTexture);
            FrameBrush.DrawAs = ESlateBrushDrawType::Box;
            const float HorizontalUVCrop = FMath::Clamp(HealthFrameHorizontalUVCrop.X, 0.0f, 0.49f);
            FrameBrush.SetUVRegion(FBox2f(
                FVector2f(HorizontalUVCrop, 0.0f),
                FVector2f(1.0f - HorizontalUVCrop, 1.0f)));
            const float HorizontalSlice = FMath::Clamp(HealthFrameSliceFraction.X, 0.0f, 0.5f);
            FrameBrush.Margin = FMargin(HorizontalSlice, 0.0f, HorizontalSlice, 0.0f);
        }
        else
        {
            // Keep a visible fallback when the optional imported texture is absent.
            FrameBrush = FProgressBarStyle::GetDefault().FillImage;
            FrameBrush.DrawAs = ESlateBrushDrawType::Border;
            FrameBrush.Margin = FMargin(0.2f);
        }
        FrameBrush.TintColor = FSlateColor(HealthFrameColor);
        Image_HealthFrame->SetBrush(FrameBrush);
        Image_HealthFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    if (Text_HealthDebug)
    {
        Text_HealthDebug->SetColorAndOpacity(FSlateColor(HealthTextColor));
    }

    RefreshDebugText();
    ApplyHealthVisuals();
}

void UPlayerHealthWidget::ConfigureProgressBar(UProgressBar* InProgressBar, const FLinearColor& InFillColor, float InPercent, UTexture2D* InFillTexture)
{
    if (!InProgressBar)
    {
        return;
    }

    FProgressBarStyle Style = FProgressBarStyle::GetDefault();
    Style.BackgroundImage = FSlateBrush();
    Style.BackgroundImage.DrawAs = ESlateBrushDrawType::NoDrawType;
    if (InFillTexture)
    {
        Style.FillImage = FSlateBrush();
        Style.FillImage.SetResourceObject(InFillTexture);
    }
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

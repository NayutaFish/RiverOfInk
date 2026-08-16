// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/EnemyHealthWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Styling/SlateTypes.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UEnemyHealthWidget::UEnemyHealthWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NormalWidgetSize(100.0f, 10.0f)
	, EliteWidgetSize(130.0f, 16.0f)
	, NormalBarInset(8.0f, 3.0f, 8.0f, 3.0f)
	, EliteBarInset(14.0f, 4.0f, 14.0f, 4.0f)
	, HealthFrameExpansion(0.0f, 0.0f)
	, CurrentHealthColor(FLinearColor::FromSRGBColor(FColor(248, 246, 241, 255)))
	, RecentDamageColor(FLinearColor::FromSRGBColor(FColor(188, 64, 36, 255)))
	, EmptyHealthColor(FLinearColor::FromSRGBColor(FColor(42, 42, 42, 255)))
	, NormalFrameTexture(nullptr)
	, EliteFrameTexture(nullptr)
	, HealthPaperTexture(nullptr)
	, RecentDamageHoldTime(0.15f)
	, RecentDamageCollapseDuration(0.45f)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> NormalFrameTextureFinder(
		TEXT("/Game/RawContent/UI/Health/Textures/T_UI_EnemyHealth_Normal_Frame.T_UI_EnemyHealth_Normal_Frame"));
	if (NormalFrameTextureFinder.Succeeded())
	{
		NormalFrameTexture = NormalFrameTextureFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> EliteFrameTextureFinder(
		TEXT("/Game/RawContent/UI/Health/Textures/T_UI_EnemyHealth_Elite_Frame.T_UI_EnemyHealth_Elite_Frame"));
	if (EliteFrameTextureFinder.Succeeded())
	{
		EliteFrameTexture = EliteFrameTextureFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> HealthPaperTextureFinder(
		TEXT("/Game/RawContent/UI/Health/Textures/T_UI_PlayerHealth_Paper.T_UI_PlayerHealth_Paper"));
	if (HealthPaperTextureFinder.Succeeded())
	{
		HealthPaperTexture = HealthPaperTextureFinder.Object;
	}
}

TSharedRef<SWidget> UEnemyHealthWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UEnemyHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildDefaultWidgetTree();
	ConfigureWidgetTree();
	SetVisibility(ESlateVisibility::Collapsed);

	UE_LOG(LogTemp, Log,
		TEXT("Enemy health widget tree: RootCanvas=%s BarLayer=%s FrameLayer=%s Empty=%s RecentDamage=%s Current=%s Frame=%s."),
		RootCanvas ? TEXT("valid") : TEXT("null"),
		HealthBarLayer ? TEXT("valid") : TEXT("null"),
		HealthFrameLayer ? TEXT("valid") : TEXT("null"),
		ProgressBar_EmptyHealth ? TEXT("valid") : TEXT("null"),
		ProgressBar_RecentDamage ? TEXT("valid") : TEXT("null"),
		ProgressBar_CurrentHealth ? TEXT("valid") : TEXT("null"),
		Image_HealthFrame ? TEXT("valid") : TEXT("null"));
}

void UEnemyHealthWidget::NativeDestruct()
{
	StopDamageTrailTimer();
	UnbindFromEnemy();
	ObservedEnemy = nullptr;
	Super::NativeDestruct();
}

void UEnemyHealthWidget::InitializeForEnemy(AEnemyBase* InEnemy)
{
	UnbindFromEnemy();
	ObservedEnemy = InEnemy;

	if (!IsValid(ObservedEnemy))
	{
		ResetHealthState(0.0f, 1.0f);
		SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Warning, TEXT("Enemy health widget initialization skipped: enemy is invalid."));
		return;
	}

	EnemyRank = ObservedEnemy->GetEnemyRank();
	BuildDefaultWidgetTree();
	ConfigureWidgetTree();
	ResetHealthState(ObservedEnemy->GetCurrentHealth(), ObservedEnemy->GetMaxHealth());
	BindToEnemy();
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(LogTemp, Log,
		TEXT("Enemy health widget visuals: Enemy=%s Rank=%d Size=(%.0f,%.0f) Frame=%s Current=(%.3f,%.3f,%.3f) Empty=(%.3f,%.3f,%.3f) RecentDamage=(%.3f,%.3f,%.3f)."),
		*GetNameSafe(ObservedEnemy),
		static_cast<int32>(EnemyRank),
		EnemyRank == EEnemyRank::Elite ? EliteWidgetSize.X : NormalWidgetSize.X,
		EnemyRank == EEnemyRank::Elite ? EliteWidgetSize.Y : NormalWidgetSize.Y,
		*GetNameSafe(EnemyRank == EEnemyRank::Elite ? EliteFrameTexture.Get() : NormalFrameTexture.Get()),
		CurrentHealthColor.R,
		CurrentHealthColor.G,
		CurrentHealthColor.B,
		EmptyHealthColor.R,
		EmptyHealthColor.G,
		EmptyHealthColor.B,
		RecentDamageColor.R,
		RecentDamageColor.G,
		RecentDamageColor.B);
}

void UEnemyHealthWidget::RefreshHealth(float InCurrentHealth, float InMaxHealth)
{
	ResetHealthState(InCurrentHealth, InMaxHealth);
	if (IsValid(ObservedEnemy))
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}

void UEnemyHealthWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!WidgetTree->RootWidget)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("EnemyHealthCanvas"));
		RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
		WidgetTree->RootWidget = RootCanvas;

		HealthSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("EnemyHealthSizeBox"));
		HealthSizeBox->SetWidthOverride(NormalWidgetSize.X);
		HealthSizeBox->SetHeightOverride(NormalWidgetSize.Y);
		HealthSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* HealthSlot = RootCanvas->AddChildToCanvas(HealthSizeBox))
		{
			HealthSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			HealthSlot->SetAlignment(FVector2D::ZeroVector);
			HealthSlot->SetPosition(FVector2D::ZeroVector);
			HealthSlot->SetSize(NormalWidgetSize);
			HealthSlot->SetZOrder(10);
		}

		HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), TEXT("EnemyHealthOverlay"));
		HealthSizeBox->SetContent(HealthOverlay);

		HealthBarLayer = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), TEXT("EnemyHealthBarLayer"));
		HealthFrameLayer = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), TEXT("EnemyHealthFrameLayer"));

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
			-ClampedFrameExpansion.X,
			-ClampedFrameExpansion.Y,
			-ClampedFrameExpansion.X,
			-ClampedFrameExpansion.Y);

		AddOverlayChild(HealthOverlay, HealthBarLayer, NormalBarInset);
		AddOverlayChild(HealthOverlay, HealthFrameLayer, FrameLayerPadding);

		ProgressBar_EmptyHealth = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("ProgressBar_EmptyHealth"));
		ProgressBar_RecentDamage = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("ProgressBar_RecentDamage"));
		ProgressBar_CurrentHealth = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("ProgressBar_CurrentHealth"));
		Image_HealthFrame = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("Image_HealthFrame"));

		AddOverlayChild(HealthBarLayer, ProgressBar_EmptyHealth, FMargin(0.0f));
		AddOverlayChild(HealthBarLayer, ProgressBar_RecentDamage, FMargin(0.0f));
		AddOverlayChild(HealthBarLayer, ProgressBar_CurrentHealth, FMargin(0.0f));
		AddOverlayChild(HealthFrameLayer, Image_HealthFrame, FMargin(0.0f));
		return;
	}

	// A future WBP_EnemyHealth subclass can provide the same named layers.
	// Keep the native fallback and the Blueprint tree on the same data path.
	RootCanvas = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("EnemyHealthCanvas")));
	HealthSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("EnemyHealthSizeBox")));
	HealthOverlay = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("EnemyHealthOverlay")));
	HealthBarLayer = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("EnemyHealthBarLayer")));
	HealthFrameLayer = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("EnemyHealthFrameLayer")));
	ProgressBar_EmptyHealth = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("ProgressBar_EmptyHealth")));
	ProgressBar_RecentDamage = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("ProgressBar_RecentDamage")));
	ProgressBar_CurrentHealth = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("ProgressBar_CurrentHealth")));
	Image_HealthFrame = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_HealthFrame")));
}

void UEnemyHealthWidget::ConfigureWidgetTree()
{
	ApplyRankLayout();

	const FMargin& BarInset = EnemyRank == EEnemyRank::Elite
		? EliteBarInset
		: NormalBarInset;
	if (UOverlaySlot* HealthBarLayerSlot = Cast<UOverlaySlot>(HealthBarLayer ? HealthBarLayer->Slot : nullptr))
	{
		HealthBarLayerSlot->SetPadding(BarInset);
		HealthBarLayerSlot->SetHorizontalAlignment(HAlign_Fill);
		HealthBarLayerSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (UOverlaySlot* HealthFrameLayerSlot = Cast<UOverlaySlot>(HealthFrameLayer ? HealthFrameLayer->Slot : nullptr))
	{
		const FVector2D ClampedFrameExpansion(
			FMath::Max(0.0f, HealthFrameExpansion.X),
			FMath::Max(0.0f, HealthFrameExpansion.Y));
		HealthFrameLayerSlot->SetPadding(FMargin(
			-ClampedFrameExpansion.X,
			-ClampedFrameExpansion.Y,
			-ClampedFrameExpansion.X,
			-ClampedFrameExpansion.Y));
		HealthFrameLayerSlot->SetHorizontalAlignment(HAlign_Fill);
		HealthFrameLayerSlot->SetVerticalAlignment(VAlign_Fill);
	}

	ConfigureProgressBar(ProgressBar_EmptyHealth, EmptyHealthColor, 1.0f);
	ConfigureProgressBar(ProgressBar_RecentDamage, RecentDamageColor, DamageGhostRatio);
	ConfigureProgressBar(ProgressBar_CurrentHealth, CurrentHealthColor, CurrentHealthRatio, HealthPaperTexture.Get());

	if (Image_HealthFrame)
	{
		UTexture2D* FrameTexture = EnemyRank == EEnemyRank::Elite
			? EliteFrameTexture.Get()
			: NormalFrameTexture.Get();
		if (FrameTexture)
		{
			FSlateBrush FrameBrush;
			FrameBrush.SetResourceObject(FrameTexture);
			FrameBrush.DrawAs = ESlateBrushDrawType::Image;
			FrameBrush.TintColor = FSlateColor(FLinearColor::White);
			Image_HealthFrame->SetBrush(FrameBrush);
			Image_HealthFrame->SetColorAndOpacity(FLinearColor::White);
			Image_HealthFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_HealthFrame->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Warning,
				TEXT("Enemy health frame texture missing: Rank=%d Widget=%s."),
				static_cast<int32>(EnemyRank),
				*GetNameSafe(this));
		}
	}
}

void UEnemyHealthWidget::ConfigureProgressBar(
	UProgressBar* InProgressBar,
	const FLinearColor& InFillColor,
	float InPercent,
	UTexture2D* InFillTexture)
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

void UEnemyHealthWidget::ApplyRankLayout()
{
	if (!HealthSizeBox)
	{
		return;
	}

	const FVector2D WidgetSize = EnemyRank == EEnemyRank::Elite
		? EliteWidgetSize
		: NormalWidgetSize;
	HealthSizeBox->SetWidthOverride(WidgetSize.X);
	HealthSizeBox->SetHeightOverride(WidgetSize.Y);

	if (UCanvasPanelSlot* HealthSlot = Cast<UCanvasPanelSlot>(HealthSizeBox->Slot))
	{
		HealthSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		HealthSlot->SetAlignment(FVector2D::ZeroVector);
		HealthSlot->SetPosition(FVector2D::ZeroVector);
		HealthSlot->SetSize(WidgetSize);
	}
}

void UEnemyHealthWidget::BindToEnemy()
{
	if (bEnemyHealthEventBound || !IsValid(ObservedEnemy))
	{
		return;
	}

	ObservedEnemy->OnEnemyHealthChanged.AddDynamic(
		this,
		&UEnemyHealthWidget::HandleEnemyHealthChanged);
	bEnemyHealthEventBound = true;
}

void UEnemyHealthWidget::UnbindFromEnemy()
{
	if (IsValid(ObservedEnemy))
	{
		ObservedEnemy->OnEnemyHealthChanged.RemoveDynamic(
			this,
			&UEnemyHealthWidget::HandleEnemyHealthChanged);
	}
	bEnemyHealthEventBound = false;
}

void UEnemyHealthWidget::HandleEnemyHealthChanged(
	float InPreviousHealth,
	float InCurrentHealth,
	float InMaxHealth,
	EEnemyHealthChangeReason InChangeReason)
{
	const float SafeMaxHealth = FMath::Max(1.0f, InMaxHealth);
	const float SafeCurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, SafeMaxHealth);

	if (!bHasHealthBaseline || InChangeReason == EEnemyHealthChangeReason::Initialize)
	{
		ResetHealthState(SafeCurrentHealth, SafeMaxHealth);
		SetVisibility(ESlateVisibility::Visible);
		return;
	}

	const float OldCurrentHealth = FMath::Clamp(InPreviousHealth, 0.0f, SafeMaxHealth);
	const float OldCurrentRatio = GetSafeHealthRatio(OldCurrentHealth, SafeMaxHealth);
	const bool bMaxHealthChanged = !FMath::IsNearlyEqual(
		SafeMaxHealth,
		LastKnownMaxHealth,
		KINDA_SMALL_NUMBER);

	LastKnownMaxHealth = SafeMaxHealth;
	LastKnownCurrentHealth = SafeCurrentHealth;
	CurrentHealthRatio = GetSafeHealthRatio(SafeCurrentHealth, SafeMaxHealth);

	if (bMaxHealthChanged || InChangeReason == EEnemyHealthChangeReason::ExternalSet)
	{
		DamageGhostRatio = CurrentHealthRatio;
		DamageCollapseStartRatio = CurrentHealthRatio;
		DamageTrailElapsed = 0.0f;
		DamageTrailState = EEnemyHealthTrailState::Idle;
		StopDamageTrailTimer();
	}
	else if (InChangeReason == EEnemyHealthChangeReason::Heal)
	{
		// Healing advances Current immediately. It never raises the existing
		// ghost or restarts its timer; only crossing the ghost closes the trail.
		if (CurrentHealthRatio >= DamageGhostRatio - KINDA_SMALL_NUMBER)
		{
			DamageGhostRatio = CurrentHealthRatio;
			DamageCollapseStartRatio = CurrentHealthRatio;
			DamageTrailState = EEnemyHealthTrailState::Idle;
			DamageTrailElapsed = 0.0f;
			StopDamageTrailTimer();
		}
	}
	else if (InChangeReason == EEnemyHealthChangeReason::Damage
		|| InChangeReason == EEnemyHealthChangeReason::Death)
	{
		if (SafeCurrentHealth < OldCurrentHealth - KINDA_SMALL_NUMBER)
		{
			StartDamageTrail(OldCurrentRatio);
		}
	}

	ApplyHealthVisuals();
	SetVisibility(ESlateVisibility::Visible);
}

void UEnemyHealthWidget::ResetHealthState(float InCurrentHealth, float InMaxHealth)
{
	LastKnownMaxHealth = FMath::Max(1.0f, InMaxHealth);
	LastKnownCurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, LastKnownMaxHealth);
	CurrentHealthRatio = GetSafeHealthRatio(LastKnownCurrentHealth, LastKnownMaxHealth);
	DamageGhostRatio = CurrentHealthRatio;
	DamageCollapseStartRatio = CurrentHealthRatio;
	DamageTrailElapsed = 0.0f;
	DamageTrailState = EEnemyHealthTrailState::Idle;
	bHasHealthBaseline = true;
	StopDamageTrailTimer();
	ApplyHealthVisuals();
}

void UEnemyHealthWidget::ApplyHealthVisuals()
{
	CurrentHealthRatio = GetSafeHealthRatio(LastKnownCurrentHealth, LastKnownMaxHealth);
	DamageGhostRatio = FMath::Clamp(
		FMath::Max(DamageGhostRatio, CurrentHealthRatio),
		CurrentHealthRatio,
		1.0f);

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

void UEnemyHealthWidget::StartDamageTrail(float OldCurrentHealthRatio)
{
	// Preserve the current ghost when another hit arrives while the previous
	// trail is holding or collapsing; it must never jump backward.
	DamageGhostRatio = FMath::Max(
		DamageGhostRatio,
		FMath::Max(OldCurrentHealthRatio, CurrentHealthRatio));
	DamageCollapseStartRatio = DamageGhostRatio;
	DamageTrailElapsed = 0.0f;
	DamageTrailState = RecentDamageHoldTime > KINDA_SMALL_NUMBER
		? EEnemyHealthTrailState::Hold
		: EEnemyHealthTrailState::Collapse;

	if (DamageTrailState == EEnemyHealthTrailState::Collapse
		&& RecentDamageCollapseDuration <= KINDA_SMALL_NUMBER)
	{
		DamageGhostRatio = CurrentHealthRatio;
		DamageTrailState = EEnemyHealthTrailState::Idle;
		StopDamageTrailTimer();
		return;
	}

	ApplyHealthVisuals();
	StartDamageTrailTimer();
}

void UEnemyHealthWidget::AdvanceDamageTrail()
{
	if (DamageTrailState == EEnemyHealthTrailState::Idle)
	{
		StopDamageTrailTimer();
		return;
	}

	DamageTrailElapsed += DamageTrailUpdateInterval;

	if (DamageTrailState == EEnemyHealthTrailState::Hold)
	{
		if (DamageTrailElapsed < RecentDamageHoldTime)
		{
			return;
		}

		DamageTrailState = EEnemyHealthTrailState::Collapse;
		DamageTrailElapsed = 0.0f;
		DamageCollapseStartRatio = DamageGhostRatio;
	}

	if (DamageTrailState == EEnemyHealthTrailState::Collapse)
	{
		if (RecentDamageCollapseDuration <= KINDA_SMALL_NUMBER)
		{
			DamageGhostRatio = CurrentHealthRatio;
			DamageTrailState = EEnemyHealthTrailState::Idle;
			StopDamageTrailTimer();
		}
		else
		{
			const float Alpha = FMath::Clamp(
				DamageTrailElapsed / RecentDamageCollapseDuration,
				0.0f,
				1.0f);
			const float EaseOutAlpha = 1.0f - FMath::Square(1.0f - Alpha);
			DamageGhostRatio = FMath::Lerp(
				DamageCollapseStartRatio,
				CurrentHealthRatio,
				EaseOutAlpha);
			DamageGhostRatio = FMath::Max(DamageGhostRatio, CurrentHealthRatio);

			if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
			{
				DamageGhostRatio = CurrentHealthRatio;
				DamageTrailState = EEnemyHealthTrailState::Idle;
				StopDamageTrailTimer();
			}
		}
	}

	ApplyHealthVisuals();
}

void UEnemyHealthWidget::StopDamageTrail()
{
	DamageGhostRatio = CurrentHealthRatio;
	DamageCollapseStartRatio = CurrentHealthRatio;
	DamageTrailElapsed = 0.0f;
	DamageTrailState = EEnemyHealthTrailState::Idle;
	StopDamageTrailTimer();
	ApplyHealthVisuals();
}

void UEnemyHealthWidget::StartDamageTrailTimer()
{
	if (DamageTrailTimerHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DamageTrailTimerHandle,
			FTimerDelegate::CreateUObject(this, &UEnemyHealthWidget::AdvanceDamageTrail),
			DamageTrailUpdateInterval,
			true);
	}
}

void UEnemyHealthWidget::StopDamageTrailTimer()
{
	if (!DamageTrailTimerHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTrailTimerHandle);
	}
	DamageTrailTimerHandle.Invalidate();
}

float UEnemyHealthWidget::GetSafeHealthRatio(float InCurrentHealth, float InMaxHealth)
{
	const float SafeMaxHealth = FMath::Max(1.0f, InMaxHealth);
	return FMath::Clamp(InCurrentHealth / SafeMaxHealth, 0.0f, 1.0f);
}

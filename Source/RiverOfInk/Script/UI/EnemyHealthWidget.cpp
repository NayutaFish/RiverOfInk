// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/EnemyHealthWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/World.h"
#include "Styling/SlateTypes.h"
#include "TimerManager.h"

UEnemyHealthWidget::UEnemyHealthWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NormalWidgetSize(180.0f, 24.0f)
	, EliteWidgetSize(216.0f, 28.0f)
	, CurrentHealthColor(FLinearColor::FromSRGBColor(FColor(220, 70, 50, 255)))
	, RecentDamageColor(FLinearColor::FromSRGBColor(FColor(55, 28, 22, 255)))
	, EmptyHealthColor(FLinearColor::FromSRGBColor(FColor(24, 20, 18, 230)))
	, RecentDamageHoldTime(0.15f)
	, RecentDamageCollapseDuration(0.45f)
{
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

	UE_LOG(LogTemp, Verbose,
		TEXT("Enemy health widget tree: Empty=%s RecentDamage=%s Current=%s."),
		ProgressBar_EmptyHealth ? TEXT("valid") : TEXT("null"),
		ProgressBar_RecentDamage ? TEXT("valid") : TEXT("null"),
		ProgressBar_CurrentHealth ? TEXT("valid") : TEXT("null"));
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
		HealthSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("EnemyHealthSizeBox"));
		HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), TEXT("EnemyHealthOverlay"));
		WidgetTree->RootWidget = HealthSizeBox;
		HealthSizeBox->SetContent(HealthOverlay);

		auto AddOverlayChild = [](UOverlay* InParent, UWidget* InWidget) -> UOverlaySlot*
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
				Slot->SetPadding(FMargin(0.0f));
			}
			return Slot;
		};

		ProgressBar_EmptyHealth = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("ProgressBar_EmptyHealth"));
		ProgressBar_RecentDamage = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("ProgressBar_RecentDamage"));
		ProgressBar_CurrentHealth = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("ProgressBar_CurrentHealth"));

		AddOverlayChild(HealthOverlay, ProgressBar_EmptyHealth);
		AddOverlayChild(HealthOverlay, ProgressBar_RecentDamage);
		AddOverlayChild(HealthOverlay, ProgressBar_CurrentHealth);
		return;
	}

	// A future WBP_EnemyHealth subclass can provide the same named layers.
	// Keep the native fallback and the Blueprint tree on the same data path.
	HealthSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("EnemyHealthSizeBox")));
	HealthOverlay = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("EnemyHealthOverlay")));
	ProgressBar_EmptyHealth = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("ProgressBar_EmptyHealth")));
	ProgressBar_RecentDamage = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("ProgressBar_RecentDamage")));
	ProgressBar_CurrentHealth = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("ProgressBar_CurrentHealth")));
}

void UEnemyHealthWidget::ConfigureWidgetTree()
{
	ApplyRankLayout();
	ConfigureProgressBar(ProgressBar_EmptyHealth, EmptyHealthColor, 1.0f);
	ConfigureProgressBar(ProgressBar_RecentDamage, RecentDamageColor, DamageGhostRatio);
	ConfigureProgressBar(ProgressBar_CurrentHealth, CurrentHealthColor, CurrentHealthRatio);
}

void UEnemyHealthWidget::ConfigureProgressBar(
	UProgressBar* InProgressBar,
	const FLinearColor& InFillColor,
	float InPercent)
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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/EnemyHealthWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Enemy/EnemyBase/EnemyBase.h"

TSharedRef<SWidget> UEnemyHealthWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UEnemyHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildDefaultWidgetTree();
	SetVisibility(ESlateVisibility::Collapsed);
	BindToEnemy();
}

void UEnemyHealthWidget::NativeDestruct()
{
	UnbindFromEnemy();
	Super::NativeDestruct();
}

void UEnemyHealthWidget::InitializeForEnemy(AEnemyBase* InEnemy)
{
	UnbindFromEnemy();
	ObservedEnemy = InEnemy;
	BindToEnemy();

	if (IsValid(ObservedEnemy))
	{
		RefreshHealth(ObservedEnemy->GetCurrentHealth(), ObservedEnemy->GetMaxHealth());
	}
}

void UEnemyHealthWidget::RefreshHealth(float InCurrentHealth, float InMaxHealth)
{
	const float SafeMaxHealth = FMath::Max(1.0f, InMaxHealth);
	const float SafeCurrentHealth = FMath::Clamp(InCurrentHealth, 0.0f, SafeMaxHealth);
	const float HealthPercent = SafeCurrentHealth / SafeMaxHealth;

	if (HealthBar)
	{
		HealthBar->SetPercent(HealthPercent);
		HealthBar->SetFillColorAndOpacity(
			HealthPercent <= 0.0f
				? FLinearColor(0.35f, 0.02f, 0.02f, 1.0f)
				: FLinearColor(0.75f, 0.08f, 0.08f, 1.0f));
	}

	// Full health is the hidden state. Once an effective hit arrives, the
	// owning EnemyBase makes the WidgetComponent visible and this event keeps
	// the widget visible until the actor dies or is destroyed.
	if (SafeCurrentHealth < SafeMaxHealth)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}

void UEnemyHealthWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EnemyHealthCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("EnemyHealthBar"));
	HealthBar->SetPercent(1.0f);
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.75f, 0.08f, 0.08f, 1.0f));

	if (UCanvasPanelSlot* HealthBarSlot = RootCanvas->AddChildToCanvas(HealthBar))
	{
		HealthBarSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		HealthBarSlot->SetOffsets(FMargin(0.0f));
	}
}

void UEnemyHealthWidget::BindToEnemy()
{
	if (bEnemyHealthEventBound || !IsValid(ObservedEnemy))
	{
		return;
	}

	ObservedEnemy->OnEnemyHealthChanged.AddDynamic(this, &UEnemyHealthWidget::HandleEnemyHealthChanged);
	bEnemyHealthEventBound = true;
}

void UEnemyHealthWidget::UnbindFromEnemy()
{
	if (!bEnemyHealthEventBound || !IsValid(ObservedEnemy))
	{
		bEnemyHealthEventBound = false;
		return;
	}

	ObservedEnemy->OnEnemyHealthChanged.RemoveDynamic(this, &UEnemyHealthWidget::HandleEnemyHealthChanged);
	bEnemyHealthEventBound = false;
}

void UEnemyHealthWidget::HandleEnemyHealthChanged(float InCurrentHealth, float InMaxHealth)
{
	RefreshHealth(InCurrentHealth, InMaxHealth);
}
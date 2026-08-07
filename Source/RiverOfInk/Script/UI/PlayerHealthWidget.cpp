// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerHealthWidget.h"
#include "RiverOfInk.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Common/HealthComponent.h"
#include "Player/PlayerCharacter.h"

TSharedRef<SWidget> UPlayerHealthWidget::RebuildWidget()
{
	// Build the native tree before UUserWidget creates its Slate widget.
	// Doing this in NativeConstruct is too late for a widget with no BP tree.
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UPlayerHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildDefaultWidgetTree();
	UE_LOG(LogRiverOfInk, Log, TEXT("Health HUD tree: Canvas=%s Bar=%s Text=%s."),
		RootCanvas ? TEXT("valid") : TEXT("null"),
		HealthBar ? TEXT("valid") : TEXT("null"),
		HealthText ? TEXT("valid") : TEXT("null"));
	SubscribeToHealthEvents();
}

void UPlayerHealthWidget::NativeDestruct()
{
	UnsubscribeFromHealthEvents();
	Super::NativeDestruct();
}

void UPlayerHealthWidget::InitializeForPlayer(APlayerCharacter* InPlayer)
{
	ObservedPlayer = InPlayer;
	if (IsValid(ObservedPlayer) && IsValid(ObservedPlayer->GetHealthComponent()))
	{
		const UHealthComponent* HealthComponent = ObservedPlayer->GetHealthComponent();
		RefreshHealth(HealthComponent->GetMaxHealth(), HealthComponent->GetCurrentHealth());
	}
}

void UPlayerHealthWidget::RefreshHealth(float InMaxHealth, float InCurrentHealth)
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
				: FLinearColor(0.05f, 0.75f, 0.18f, 1.0f));
	}

	if (HealthText)
	{
		HealthText->SetText(FText::Format(
			FText::FromString(TEXT("HP {0} / {1}")),
			FMath::RoundToInt(SafeCurrentHealth),
			FMath::RoundToInt(SafeMaxHealth)));
	}
}

void UPlayerHealthWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HealthCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	// Health is display-only. Keep the full-screen HUD from becoming a mouse
	// hit-test blocker for modal widgets such as the reward cards.
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetPercent(1.0f);
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.05f, 0.75f, 0.18f, 1.0f));

	if (UCanvasPanelSlot* HealthBarSlot = RootCanvas->AddChildToCanvas(HealthBar))
	{
		HealthBarSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		HealthBarSlot->SetOffsets(FMargin(32.0f, 32.0f, 300.0f, 26.0f));
	}

	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	HealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	HealthText->SetJustification(ETextJustify::Center);
	HealthText->SetText(FText::FromString(TEXT("HP 0 / 0")));

	if (UCanvasPanelSlot* HealthTextSlot = RootCanvas->AddChildToCanvas(HealthText))
	{
		HealthTextSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		HealthTextSlot->SetOffsets(FMargin(32.0f, 34.0f, 300.0f, 22.0f));
	}
}

void UPlayerHealthWidget::SubscribeToHealthEvents()
{
	if (bHealthEventSubscribed)
	{
		return;
	}

	TWeakObjectPtr<UPlayerHealthWidget> WeakThis(this);
	HealthChangedHandle = FEventBus::Subscribe<FPlayerHealthChangedEvent>(
		[WeakThis](const FPlayerHealthChangedEvent& Event)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandleHealthChanged(Event);
			}
		});
	bHealthEventSubscribed = true;
}

void UPlayerHealthWidget::UnsubscribeFromHealthEvents()
{
	if (!bHealthEventSubscribed)
	{
		return;
	}

	FEventBus::Unsubscribe<FPlayerHealthChangedEvent>(HealthChangedHandle);
	HealthChangedHandle.Reset();
	bHealthEventSubscribed = false;
}

void UPlayerHealthWidget::HandleHealthChanged(const FPlayerHealthChangedEvent& Event)
{
	RefreshHealth(static_cast<float>(Event.MaxHealth), static_cast<float>(Event.CurrentHealth));
}

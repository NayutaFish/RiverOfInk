// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerSkillWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Player/PlayerCharacter.h"
#include "Player/Skill/SkillComponent.h"
#include "UI/PlayerSkillSlotWidget.h"
#include "TimerManager.h"

TSharedRef<SWidget> UPlayerSkillWidget::RebuildWidget()
{
	BuildDefaultWidgetTree();
	return Super::RebuildWidget();
}

void UPlayerSkillWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildDefaultWidgetTree();
	BindSkillEvents();
	RefreshSkills();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CooldownRefreshTimer,
			this,
			&UPlayerSkillWidget::RefreshCooldowns,
			0.05f,
			true);
	}

	UE_LOG(LogSkill, Log, TEXT("Compact skill HUD tree: Canvas=%s Bar=%s QSlot=%s ESlot=%s."),
		RootCanvas ? TEXT("valid") : TEXT("null"),
		SkillBar ? TEXT("valid") : TEXT("null"),
		QSkillSlot ? TEXT("valid") : TEXT("null"),
		ESkillSlot ? TEXT("valid") : TEXT("null"));
}

void UPlayerSkillWidget::NativeDestruct()
{
	UnbindSkillEvents();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownRefreshTimer);
	}
	Super::NativeDestruct();
}

void UPlayerSkillWidget::InitializeForPlayer(APlayerCharacter* InPlayer)
{
	UnbindSkillEvents();
	ObservedPlayer = InPlayer;
	ObservedSkillComponent = IsValid(ObservedPlayer) ? ObservedPlayer->SkillComponent : nullptr;
	BindSkillEvents();
	RefreshSkills();

	UE_LOG(LogSkill, Log, TEXT("Compact skill HUD bound to %s. QTexture=%s ETexture=%s."),
		*GetNameSafe(ObservedPlayer),
		*GetNameSafe(TripleProjectileIcon),
		*GetNameSafe(CircularSlashIcon));
}

void UPlayerSkillWidget::RefreshSkills()
{
	if (!IsValid(ObservedSkillComponent))
	{
		return;
	}

	const FPlayerSkillSlot EmptySlot;
	const FPlayerSkillSlot& QSlot = ObservedSkillComponent->SkillSlots.IsValidIndex(0)
		? ObservedSkillComponent->SkillSlots[0]
		: EmptySlot;
	const FPlayerSkillSlot& ESlot = ObservedSkillComponent->SkillSlots.IsValidIndex(1)
		? ObservedSkillComponent->SkillSlots[1]
		: EmptySlot;

	RefreshSlot(QSlot, EPlayerSkillID::TripleProjectile, QSkillSlot, TEXT("Q"));
	RefreshSlot(ESlot, EPlayerSkillID::CircularSlash, ESkillSlot, TEXT("E"));
	RefreshCooldowns();
}

void UPlayerSkillWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SkillCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	SkillBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Skills"));
	if (UCanvasPanelSlot* SkillBarSlot = RootCanvas->AddChildToCanvas(SkillBar))
	{
		// Bottom-left anchor + desired size keeps this stable across resolutions
		// while still allowing the project DPI curve to scale the widget.
		SkillBarSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		SkillBarSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		SkillBarSlot->SetPosition(FVector2D(42.0f, -32.0f));
		SkillBarSlot->SetAutoSize(true);
	}

	TSubclassOf<UPlayerSkillSlotWidget> SkillSlotClass = UPlayerSkillSlotWidget::StaticClass();
	if (UClass* BlueprintSlotClass = LoadClass<UPlayerSkillSlotWidget>(nullptr, TEXT("/Game/Blueprint/GameSystem/UI/Skill/WBP_SkillSlot.WBP_SkillSlot_C")))
	{
		SkillSlotClass = BlueprintSlotClass;
	}

	QSkillSlot = WidgetTree->ConstructWidget<UPlayerSkillSlotWidget>(SkillSlotClass, TEXT("WBP_SkillSlot_Q"));
	if (UHorizontalBoxSlot* QSlot = SkillBar->AddChildToHorizontalBox(QSkillSlot))
	{
		QSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		QSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ESkillSlot = WidgetTree->ConstructWidget<UPlayerSkillSlotWidget>(SkillSlotClass, TEXT("WBP_SkillSlot_E"));
	if (UHorizontalBoxSlot* ESlot = SkillBar->AddChildToHorizontalBox(ESkillSlot))
	{
		ESlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void UPlayerSkillWidget::BindSkillEvents()
{
	if (bSkillEventSubscribed || !IsValid(ObservedSkillComponent))
	{
		return;
	}

	ObservedSkillComponent->OnSkillStateChanged.AddUObject(this, &UPlayerSkillWidget::HandleSkillStateChanged);
	bSkillEventSubscribed = true;
}

void UPlayerSkillWidget::UnbindSkillEvents()
{
	if (bSkillEventSubscribed && IsValid(ObservedSkillComponent))
	{
		ObservedSkillComponent->OnSkillStateChanged.RemoveAll(this);
	}
	bSkillEventSubscribed = false;
}

void UPlayerSkillWidget::HandleSkillStateChanged()
{
	// The component has already accepted the cast and recorded its real time.
	// Refreshing here makes the slot visible immediately without synthesizing a
	// UI-only cooldown.
	RefreshSkills();
}

void UPlayerSkillWidget::RefreshCooldowns()
{
	if (!IsValid(ObservedSkillComponent))
	{
		return;
	}

	const auto RefreshSlotCooldown = [this](EPlayerSkillID SkillID, UPlayerSkillSlotWidget* SkillSlotWidget)
	{
		if (!SkillSlotWidget)
		{
			return;
		}

		SkillSlotWidget->UpdateCooldown(
			ObservedSkillComponent->GetRemainingSkillCooldown(SkillID),
			ObservedSkillComponent->GetSkillCooldown(SkillID));
	};

	RefreshSlotCooldown(EPlayerSkillID::TripleProjectile, QSkillSlot);
	RefreshSlotCooldown(EPlayerSkillID::CircularSlash, ESkillSlot);
}

void UPlayerSkillWidget::RefreshSlot(
	const FPlayerSkillSlot& SkillSlot,
	EPlayerSkillID FallbackSkillID,
	UPlayerSkillSlotWidget* SkillSlotWidget,
	const TCHAR* KeyLabel)
{
	if (!SkillSlotWidget)
	{
		return;
	}

	const EPlayerSkillID SkillID = SkillSlot.SkillID == EPlayerSkillID::None ? FallbackSkillID : SkillSlot.SkillID;
	SkillSlotWidget->InitializeSkillSlot(SkillID, FText::FromString(KeyLabel));
	SkillSlotWidget->SetSkillIcon(LoadSkillIcon(SkillID));
}

UTexture2D* UPlayerSkillWidget::LoadSkillIcon(EPlayerSkillID SkillID)
{
	switch (SkillID)
	{
	case EPlayerSkillID::TripleProjectile:
		if (!TripleProjectileIcon)
		{
			TripleProjectileIcon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RawContent/UI/Texture/Icon_TripleProjectile.Icon_TripleProjectile"));
			UE_LOG(LogSkill, Log, TEXT("Compact skill HUD icon resolved: TripleProjectile=%s."), *GetNameSafe(TripleProjectileIcon));
		}
		return TripleProjectileIcon;
	case EPlayerSkillID::CircularSlash:
		if (!CircularSlashIcon)
		{
			CircularSlashIcon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RawContent/UI/Texture/Icon_CircularSlash.Icon_CircularSlash"));
			UE_LOG(LogSkill, Log, TEXT("Compact skill HUD icon resolved: CircularSlash=%s."), *GetNameSafe(CircularSlashIcon));
		}
		return CircularSlashIcon;
	default:
		return nullptr;
	}
}

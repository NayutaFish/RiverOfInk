// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerSkillWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Player/PlayerCharacter.h"
#include "Player/Skill/SkillComponent.h"
#include "TimerManager.h"

namespace
{
	FText GetSkillTitle(EPlayerSkillID SkillID)
	{
		switch (SkillID)
		{
		case EPlayerSkillID::TripleProjectile:
			return FText::FromString(TEXT("Triple Projectile"));
		case EPlayerSkillID::CircularSlash:
			return FText::FromString(TEXT("Circular Slash"));
		default:
			return FText::FromString(TEXT("Empty Slot"));
		}
	}

	FLinearColor GetSkillColor(EPlayerSkillID SkillID)
	{
		return SkillID == EPlayerSkillID::CircularSlash
			? FLinearColor(0.12f, 0.42f, 1.0f, 1.0f)
			: FLinearColor(1.0f, 0.66f, 0.12f, 1.0f);
	}
}

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
	if (RootCanvas)
	{
		RootCanvas->ForceLayoutPrepass();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CooldownRefreshTimer,
			this,
			&UPlayerSkillWidget::RefreshCooldowns,
			0.10f,
			true);
	}

	UE_LOG(LogSkill, Log, TEXT("Skill HUD tree: Canvas=%s QIcon=%s EIcon=%s Dock=%s."),
		RootCanvas ? TEXT("valid") : TEXT("null"),
		QIcon ? TEXT("valid") : TEXT("null"),
		EIcon ? TEXT("valid") : TEXT("null"),
		SkillBar ? TEXT("valid") : TEXT("null"));

	UE_LOG(LogSkill, Log,
		TEXT("Skill HUD layout: DockDesired=(%.0f,%.0f) QIconDesired=(%.0f,%.0f) EIconDesired=(%.0f,%.0f) DataBound=%s Visibility=%d."),
		SkillBar ? SkillBar->GetDesiredSize().X : 0.0f,
		SkillBar ? SkillBar->GetDesiredSize().Y : 0.0f,
		QIcon ? QIcon->GetDesiredSize().X : 0.0f,
		QIcon ? QIcon->GetDesiredSize().Y : 0.0f,
		EIcon ? EIcon->GetDesiredSize().X : 0.0f,
		EIcon ? EIcon->GetDesiredSize().Y : 0.0f,
		IsValid(ObservedSkillComponent) ? TEXT("yes") : TEXT("awaiting"),
		RootCanvas ? static_cast<int32>(RootCanvas->GetVisibility()) : -1);
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
	if (RootCanvas)
	{
		RootCanvas->ForceLayoutPrepass();
	}

	UE_LOG(LogSkill, Log,
		TEXT("Skill HUD bound to %s. QTexture=%s ETexture=%s DockDesired=(%.0f,%.0f)."),
		*GetNameSafe(ObservedPlayer),
		*GetNameSafe(TripleProjectileIcon),
		*GetNameSafe(CircularSlashIcon),
		SkillBar ? SkillBar->GetDesiredSize().X : 0.0f,
		SkillBar ? SkillBar->GetDesiredSize().Y : 0.0f);
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

	RefreshSlot(
		QSlot,
		EPlayerSkillID::TripleProjectile,
		QIcon,
		QTitle,
		QLevel,
		QCooldownBar,
		QCooldownText,
		TEXT("Q"));
	RefreshSlot(
		ESlot,
		EPlayerSkillID::CircularSlash,
		EIcon,
		ETitle,
		ELevel,
		ECooldownBar,
		ECooldownText,
		TEXT("E"));
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
	// The HUD is display-only; never let its full-screen canvas intercept player input.
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	SkillBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SkillBar"));
	if (UCanvasPanelSlot* SkillBarSlot = RootCanvas->AddChildToCanvas(SkillBar))
	{
		SkillBarSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		SkillBarSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		SkillBarSlot->SetPosition(FVector2D(0.0f, -28.0f));
		SkillBarSlot->SetSize(FVector2D(560.0f, 186.0f));
	}

	const auto AddText = [this](UVerticalBox* Card, const TCHAR* Name, const FText& Text, const FLinearColor& Color, int32 FontSize)
	{
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		if (UVerticalBoxSlot* TextSlot = Card->AddChildToVerticalBox(TextBlock))
		{
			TextSlot->SetHorizontalAlignment(HAlign_Center);
			TextSlot->SetPadding(FMargin(2.0f, 1.0f));
		}
		return TextBlock;
	};

	const auto AddCard = [this, &AddText](const TCHAR* Prefix, const TCHAR* KeyLabel, EPlayerSkillID SkillID, TObjectPtr<UBorder>& OutCardBorder, TObjectPtr<UImage>& OutIcon, TObjectPtr<UTextBlock>& OutTitle, TObjectPtr<UTextBlock>& OutLevel, TObjectPtr<UProgressBar>& OutCooldownBar, TObjectPtr<UTextBlock>& OutCooldownText)
	{
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sSize"), Prefix));
		CardSize->SetWidthOverride(256.0f);
		CardSize->SetHeightOverride(178.0f);
		if (UHorizontalBoxSlot* CardSlot = SkillBar->AddChildToHorizontalBox(CardSize))
		{
			CardSlot->SetPadding(FMargin(8.0f, 0.0f));
			CardSlot->SetHorizontalAlignment(HAlign_Center);
		}

		OutCardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sCard"), Prefix));
		OutCardBorder->SetBrushColor(SkillID == EPlayerSkillID::CircularSlash
			? FLinearColor(0.025f, 0.08f, 0.18f, 0.96f)
			: FLinearColor(0.16f, 0.075f, 0.015f, 0.96f));
		OutCardBorder->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 8.0f));
		CardSize->AddChild(OutCardBorder);

		UVerticalBox* Card = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("%sContent"), Prefix));
		OutCardBorder->AddChild(Card);

		AddText(Card, *FString::Printf(TEXT("%sKey"), Prefix), FText::FromString(KeyLabel), FLinearColor::White, 16);

		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sIconSize"), Prefix));
		IconSize->SetWidthOverride(80.0f);
		IconSize->SetHeightOverride(80.0f);
		OutIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("%sIcon"), Prefix));
		OutIcon->SetDesiredSizeOverride(FVector2D(76.0f, 76.0f));
		IconSize->AddChild(OutIcon);
		if (UVerticalBoxSlot* IconSlot = Card->AddChildToVerticalBox(IconSize))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetPadding(FMargin(2.0f, 1.0f));
		}

		OutTitle = AddText(Card, *FString::Printf(TEXT("%sTitle"), Prefix), GetSkillTitle(SkillID), GetSkillColor(SkillID), 18);
		OutLevel = AddText(Card, *FString::Printf(TEXT("%sLevel"), Prefix), FText::FromString(TEXT("Lv.1")), FLinearColor(0.86f, 0.9f, 0.96f, 1.0f), 14);

		OutCooldownBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), *FString::Printf(TEXT("%sCooldownBar"), Prefix));
		OutCooldownBar->SetPercent(0.0f);
		OutCooldownBar->SetFillColorAndOpacity(GetSkillColor(SkillID));
		USizeBox* CooldownSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("%sCooldownSize"), Prefix));
		CooldownSize->SetWidthOverride(204.0f);
		CooldownSize->SetHeightOverride(8.0f);
		CooldownSize->AddChild(OutCooldownBar);
		if (UVerticalBoxSlot* CooldownBarSlot = Card->AddChildToVerticalBox(CooldownSize))
		{
			CooldownBarSlot->SetHorizontalAlignment(HAlign_Center);
			CooldownBarSlot->SetPadding(FMargin(2.0f, 6.0f, 2.0f, 0.0f));
		}

		OutCooldownText = AddText(Card, *FString::Printf(TEXT("%sCooldownText"), Prefix), FText::FromString(TEXT("Ready")), FLinearColor(0.8f, 0.86f, 0.94f, 1.0f), 13);
	};

	AddCard(TEXT("QSkill"), TEXT("Q"), EPlayerSkillID::TripleProjectile, QCardBorder, QIcon, QTitle, QLevel, QCooldownBar, QCooldownText);
	AddCard(TEXT("ESkill"), TEXT("E"), EPlayerSkillID::CircularSlash, ECardBorder, EIcon, ETitle, ELevel, ECooldownBar, ECooldownText);
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
	RefreshSkills();
}

void UPlayerSkillWidget::RefreshCooldowns()
{
	if (!IsValid(ObservedSkillComponent))
	{
		return;
	}

	const auto RefreshBar = [this](EPlayerSkillID SkillID, UProgressBar* CooldownBar, UTextBlock* CooldownText)
	{
		if (!CooldownBar || !CooldownText)
		{
			return;
		}

		const float Cooldown = ObservedSkillComponent->GetSkillCooldown(SkillID);
		const float Remaining = ObservedSkillComponent->GetRemainingSkillCooldown(SkillID);
		const float Percent = Cooldown > KINDA_SMALL_NUMBER ? Remaining / Cooldown : 0.0f;
		CooldownBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
		CooldownText->SetText(Remaining > KINDA_SMALL_NUMBER
			? FText::Format(FText::FromString(TEXT("CD {0}s")), FText::AsNumber(FMath::RoundToFloat(Remaining * 10.0f) / 10.0f))
			: FText::FromString(TEXT("Ready")));
	};

	RefreshBar(EPlayerSkillID::TripleProjectile, QCooldownBar, QCooldownText);
	RefreshBar(EPlayerSkillID::CircularSlash, ECooldownBar, ECooldownText);
}

void UPlayerSkillWidget::RefreshSlot(
	const FPlayerSkillSlot& SkillSlot,
	EPlayerSkillID FallbackSkillID,
	UImage* Icon,
	UTextBlock* Title,
	UTextBlock* Level,
	UProgressBar* CooldownBar,
	UTextBlock* CooldownText,
	const TCHAR* KeyLabel)
{
	const EPlayerSkillID SkillID = SkillSlot.SkillID == EPlayerSkillID::None ? FallbackSkillID : SkillSlot.SkillID;
	if (Icon)
	{
		if (SkillID == EPlayerSkillID::TripleProjectile)
		{
			if (!TripleProjectileIcon)
			{
				TripleProjectileIcon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RawContent/UI/Texture/Icon_TripleProjectile.Icon_TripleProjectile"));
				UE_LOG(LogSkill, Log, TEXT("Skill HUD icon resolved: TripleProjectile=%s."), *GetNameSafe(TripleProjectileIcon));
			}
			Icon->SetBrushFromTexture(TripleProjectileIcon, true);
		}
		else if (SkillID == EPlayerSkillID::CircularSlash)
		{
			if (!CircularSlashIcon)
			{
				CircularSlashIcon = LoadObject<UTexture2D>(nullptr, TEXT("/Game/RawContent/UI/Texture/Icon_CircularSlash.Icon_CircularSlash"));
				UE_LOG(LogSkill, Log, TEXT("Skill HUD icon resolved: CircularSlash=%s."), *GetNameSafe(CircularSlashIcon));
			}
			Icon->SetBrushFromTexture(CircularSlashIcon, true);
		}

		if (Icon->GetBrush().GetResourceObject() == nullptr)
		{
			UE_LOG(LogSkill, Warning, TEXT("Skill HUD icon missing for %s; showing color placeholder."), *GetSkillTitle(SkillID).ToString());
			Icon->SetColorAndOpacity(GetSkillColor(SkillID));
		}
		else
		{
			Icon->SetColorAndOpacity(FLinearColor::White);
		}
	}

	if (Title)
	{
		Title->SetText(GetSkillTitle(SkillID));
		Title->SetColorAndOpacity(FSlateColor(GetSkillColor(SkillID)));
	}
	if (Level)
	{
		Level->SetText(FText::Format(FText::FromString(TEXT("{0}  Lv.{1}")), FText::FromString(KeyLabel), FMath::Max(1, SkillSlot.SkillLevel)));
	}

}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRewardWidget.h"

#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	FText GetSkillDisplayName(EPlayerSkillID SkillID)
	{
		switch (SkillID)
		{
		case EPlayerSkillID::TripleProjectile:
			return FText::FromString(TEXT("Triple Projectile"));
		case EPlayerSkillID::CircularSlash:
			return FText::FromString(TEXT("Circular Slash"));
		default:
			return FText::FromString(TEXT("Unknown Skill"));
		}
	}

	FText GetSkillFormDisplayName(EPlayerSkillID SkillID, EPlayerSkillForm SkillForm)
	{
		switch (SkillForm)
		{
		case EPlayerSkillForm::ThrownGrenade:
			return FText::FromString(TEXT("Ink Grenade"));
		case EPlayerSkillForm::NullRing:
			return FText::FromString(TEXT("Null Ring"));
		case EPlayerSkillForm::TwinSlash:
			return FText::FromString(TEXT("Twin Slash"));
		case EPlayerSkillForm::Default:
		default:
			return GetSkillDisplayName(SkillID);
		}
	}

	FText GetRewardCategoryText(const FRoguelikeRewardOption& Option)
	{
		const TCHAR* InputSlot = Option.SkillID == EPlayerSkillID::CircularSlash ? TEXT("E") : TEXT("Q");
		switch (Option.RewardType)
		{
		case ERoguelikeRewardType::ChangeSkillForm:
			return FText::FromString(FString::Printf(TEXT("%s Form Change"), InputSlot));
		case ERoguelikeRewardType::UpgradeSkill:
			return FText::FromString(FString::Printf(TEXT("%s Skill Upgrade · %s"),
				InputSlot,
				Option.UpgradeType == ESkillUpgradeType::Cooldown ? TEXT("Cooldown") : TEXT("Mechanic")));
		case ERoguelikeRewardType::GainSkill:
			return FText::FromString(FString::Printf(TEXT("%s Skill Unlock"), InputSlot));
		default:
			return FText::FromString(TEXT("Reward"));
		}
	}

	FText GetRewardEffectText(const FRoguelikeRewardOption& Option)
	{
		if (Option.RewardType != ERoguelikeRewardType::ChangeSkillForm)
		{
			return Option.Description;
		}

		return FText::FromString(FString::Printf(TEXT("%s → %s\n%s"),
			*GetSkillFormDisplayName(Option.SkillID, Option.CurrentSkillForm).ToString(),
			*GetSkillFormDisplayName(Option.SkillID, Option.TargetSkillForm).ToString(),
			*Option.Description.ToString()));
	}

	FText GetFallbackCardDescription(const FRoguelikeRewardOption& Option)
	{
		return FText::FromString(FString::Printf(TEXT("Current Form: %s\n%s\n%s\n%s"),
			*GetSkillFormDisplayName(Option.SkillID, Option.CurrentSkillForm).ToString(),
			*GetRewardCategoryText(Option).ToString(),
			*Option.Title.ToString(),
			*GetRewardEffectText(Option).ToString()));
	}

	FLinearColor GetRewardAccent(const FRoguelikeRewardOption& Option)
	{
		return Option.SkillID == EPlayerSkillID::CircularSlash
			? FLinearColor(0.12f, 0.42f, 1.0f, 1.0f)
			: FLinearColor(1.0f, 0.66f, 0.12f, 1.0f);
	}

	void StyleRewardCard(UButton* Button, UTextBlock* Title, UTextBlock* Description, const FRoguelikeRewardOption& Option)
	{
		const FLinearColor Accent = GetRewardAccent(Option);
		if (Button)
		{
			Button->SetBackgroundColor(FLinearColor(Accent.R * 0.16f, Accent.G * 0.16f, Accent.B * 0.16f, 0.97f));
			Button->SetColorAndOpacity(FLinearColor::White);
		}
		if (Title)
		{
			Title->SetColorAndOpacity(FSlateColor(Accent));
			FSlateFontInfo Font = Title->GetFont();
			Font.Size = 22;
			Title->SetFont(Font);
		}
		if (Description)
		{
			Description->SetColorAndOpacity(FSlateColor(FLinearColor(0.86f, 0.9f, 0.96f, 1.0f)));
			FSlateFontInfo Font = Description->GetFont();
			Font.Size = 15;
			Description->SetFont(Font);
		}
	}

	void StyleRewardMetadata(UTextBlock* SkillName, UTextBlock* CurrentForm, UTextBlock* Category, UTextBlock* Effect, const FRoguelikeRewardOption& Option)
	{
		const FLinearColor Accent = GetRewardAccent(Option);
		if (SkillName)
		{
			SkillName->SetColorAndOpacity(FSlateColor(Accent));
			FSlateFontInfo Font = SkillName->GetFont();
			Font.Size = 22;
			SkillName->SetFont(Font);
		}
		if (CurrentForm)
		{
			CurrentForm->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.86f, 0.92f, 1.0f)));
		}
		if (Category)
		{
			Category->SetColorAndOpacity(FSlateColor(Accent));
			FSlateFontInfo Font = Category->GetFont();
			Font.Size = 14;
			Category->SetFont(Font);
		}
		if (Effect)
		{
			Effect->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.94f, 0.98f, 1.0f)));
		}
	}

	void SetRewardCardText(
		UTextBlock* Title,
		UTextBlock* Description,
		UTextBlock* SkillName,
		UTextBlock* CurrentForm,
		UTextBlock* Category,
		UTextBlock* Effect,
		const FRoguelikeRewardOption& Option)
	{
		const bool bHasDetailedLayout = SkillName && CurrentForm && Category && Effect;
		if (Title)
		{
			Title->SetText(bHasDetailedLayout ? Option.Title : GetSkillDisplayName(Option.SkillID));
		}
		if (Description)
		{
			Description->SetText(bHasDetailedLayout ? Option.Description : GetFallbackCardDescription(Option));
		}
		if (SkillName)
		{
			SkillName->SetText(GetSkillDisplayName(Option.SkillID));
		}
		if (CurrentForm)
		{
			CurrentForm->SetText(FText::FromString(FString::Printf(TEXT("Current Form: %s"),
				*GetSkillFormDisplayName(Option.SkillID, Option.CurrentSkillForm).ToString())));
		}
		if (Category)
		{
			Category->SetText(GetRewardCategoryText(Option));
		}
		if (Effect)
		{
			Effect->SetText(GetRewardEffectText(Option));
		}
	}

	UTexture2D* LoadRewardIcon(const FRoguelikeRewardOption& Option)
	{
		const TCHAR* IconPath = TEXT("/Game/RawContent/UI/Texture/Icon_Cooldown.Icon_Cooldown");
		if (Option.SkillID == EPlayerSkillID::CircularSlash)
		{
			IconPath = Option.UpgradeType == ESkillUpgradeType::Mechanic
				? TEXT("/Game/RawContent/UI/Texture/Icon_Range.Icon_Range")
				: TEXT("/Game/RawContent/UI/Texture/Icon_CircularSlash.Icon_CircularSlash");
		}
		else if (Option.SkillID == EPlayerSkillID::TripleProjectile && Option.UpgradeType == ESkillUpgradeType::Mechanic)
		{
			IconPath = TEXT("/Game/RawContent/UI/Texture/Icon_ProjectileCount.Icon_ProjectileCount");
		}

		return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, IconPath));
	}

	void SetRewardIcon(UImage* Image, const FRoguelikeRewardOption* Option)
	{
		if (!Image)
		{
			return;
		}

		if (Option)
		{
			if (UTexture2D* Icon = LoadRewardIcon(*Option))
			{
				Image->SetBrushFromTexture(Icon, true);
				Image->SetDesiredSizeOverride(FVector2D(112.0f, 112.0f));
				Image->SetVisibility(ESlateVisibility::Visible);
				UE_LOG(LogRoguelike, Log, TEXT("Reward icon applied: Skill=%d Upgrade=%d Asset=%s."),
					static_cast<int32>(Option->SkillID),
					static_cast<int32>(Option->UpgradeType),
					*GetNameSafe(Icon));
				return;
			}

			UE_LOG(LogRoguelike, Warning, TEXT("Reward icon load failed: Skill=%d Upgrade=%d."),
				static_cast<int32>(Option->SkillID),
				static_cast<int32>(Option->UpgradeType));
		}

		Image->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URoguelikeRewardWidget::SetupRewardOptions(ARoguelikeRewardManager* InRewardManager, const TArray<FRoguelikeRewardOption>& InOptions)
{
	RewardManager = InRewardManager;
	RewardOptions = InOptions;
	// Let the Blueprint hook update any optional presentation first. The native
	// bindings below are the final source of truth for the whitebox cards.
	OnRewardOptionsSet(RewardOptions);

	const bool bHasOption0 = RewardOptions.IsValidIndex(0);
	const bool bHasOption1 = RewardOptions.IsValidIndex(1);
	if (Button_0)
	{
		Button_0->SetVisibility(bHasOption0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_0->SetIsEnabled(bHasOption0);
	}
	if (Button_1)
	{
		Button_1->SetVisibility(bHasOption1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_1->SetIsEnabled(bHasOption1);
	}
	if (bHasOption0 && bHasOption1 && Button_0 && Button_1)
	{
		// The cards are arranged horizontally. Explicit navigation keeps keyboard
		// and gamepad focus stable even if the Blueprint layout changes later.
		Button_0->SetNavigationRuleExplicit(EUINavigation::Right, Button_1);
		Button_1->SetNavigationRuleExplicit(EUINavigation::Left, Button_0);
	}

	if (bHasOption0)
	{
		SetRewardCardText(Title_0, Description_0, SkillName_0, CurrentForm_0, RewardCategory_0, Effect_0, RewardOptions[0]);
		StyleRewardCard(Button_0, Title_0, Description_0, RewardOptions[0]);
		StyleRewardMetadata(SkillName_0, CurrentForm_0, RewardCategory_0, Effect_0, RewardOptions[0]);
	}
	if (bHasOption1)
	{
		SetRewardCardText(Title_1, Description_1, SkillName_1, CurrentForm_1, RewardCategory_1, Effect_1, RewardOptions[1]);
		StyleRewardCard(Button_1, Title_1, Description_1, RewardOptions[1]);
		StyleRewardMetadata(SkillName_1, CurrentForm_1, RewardCategory_1, Effect_1, RewardOptions[1]);
	}
	for (int32 OptionIndex = 0; OptionIndex < RewardOptions.Num(); ++OptionIndex)
	{
		const FRoguelikeRewardOption& Option = RewardOptions[OptionIndex];
		UE_LOG(LogRoguelike, Log,
			TEXT("Reward card %d presentation: Skill=%s Current=%s Category=%s Effect=%s."),
			OptionIndex,
			*GetSkillDisplayName(Option.SkillID).ToString(),
			*GetSkillFormDisplayName(Option.SkillID, Option.CurrentSkillForm).ToString(),
			*GetRewardCategoryText(Option).ToString(),
			*GetRewardEffectText(Option).ToString());
	}
	SetRewardIcon(Icon_0, RewardOptions.IsValidIndex(0) ? &RewardOptions[0] : nullptr);
	SetRewardIcon(Icon_1, RewardOptions.IsValidIndex(1) ? &RewardOptions[1] : nullptr);

	// Setup can be called immediately after AddToViewport. Force a Slate
	// prepass only after the brush/text/visibility values have been applied so
	// the button content gets a real desired size before the first frame.
	ForceLayoutPrepass();
	UE_LOG(LogRoguelike, Log,
		TEXT("Reward widget bindings: Button0=%s Content0=%s Icon0=%s Title0=%s Description0=%s Button1=%s Content1=%s Icon1=%s Title1=%s Description1=%s."),
		*GetNameSafe(Button_0),
		Button_0 ? *GetNameSafe(Button_0->GetContent()) : TEXT("null"),
		*GetNameSafe(Icon_0),
		*GetNameSafe(Title_0),
		*GetNameSafe(Description_0),
		*GetNameSafe(Button_1),
		Button_1 ? *GetNameSafe(Button_1->GetContent()) : TEXT("null"),
		*GetNameSafe(Icon_1),
		*GetNameSafe(Title_1),
		*GetNameSafe(Description_1));
	const auto LogButtonContent = [](const TCHAR* Label, UButton* Button)
	{
		if (!Button)
		{
			return;
		}
		UWidget* Content = Button->GetContent();
		UE_LOG(LogRoguelike, Log, TEXT("Reward button content %s: Widget=%s Desired=(%.0f,%.0f) Visibility=%d Opacity=%.2f."),
			Label,
			*GetNameSafe(Content),
			Content ? Content->GetDesiredSize().X : 0.0f,
			Content ? Content->GetDesiredSize().Y : 0.0f,
			Content ? static_cast<int32>(Content->GetVisibility()) : -1,
			Content ? Content->GetRenderOpacity() : 0.0f);
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Content))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				UWidget* Child = Panel->GetChildAt(ChildIndex);
				UE_LOG(LogRoguelike, Log, TEXT("Reward button content %s child[%d]=%s Class=%s Desired=(%.0f,%.0f) Visibility=%d Opacity=%.2f."),
					Label,
					ChildIndex,
					*GetNameSafe(Child),
					Child ? *Child->GetClass()->GetName() : TEXT("null"),
					Child ? Child->GetDesiredSize().X : 0.0f,
					Child ? Child->GetDesiredSize().Y : 0.0f,
					Child ? static_cast<int32>(Child->GetVisibility()) : -1,
					Child ? Child->GetRenderOpacity() : 0.0f);
			}
		}
	};
	LogButtonContent(TEXT("Button0"), Button_0);
	LogButtonContent(TEXT("Button1"), Button_1);
	UE_LOG(LogRoguelike, Log,
		TEXT("Reward button interaction: Button0 Enabled=%s Focusable=%s ClickBound=%s Visibility=%d; Button1 Enabled=%s Focusable=%s ClickBound=%s Visibility=%d."),
		Button_0 && Button_0->GetIsEnabled() ? TEXT("true") : TEXT("false"),
		Button_0 && Button_0->GetIsFocusable() ? TEXT("true") : TEXT("false"),
		Button_0 && Button_0->OnClicked.IsBound() ? TEXT("true") : TEXT("false"),
		Button_0 ? static_cast<int32>(Button_0->GetVisibility()) : -1,
		Button_1 && Button_1->GetIsEnabled() ? TEXT("true") : TEXT("false"),
		Button_1 && Button_1->GetIsFocusable() ? TEXT("true") : TEXT("false"),
		Button_1 && Button_1->OnClicked.IsBound() ? TEXT("true") : TEXT("false"),
		Button_1 ? static_cast<int32>(Button_1->GetVisibility()) : -1);
}

void URoguelikeRewardWidget::FocusFirstOption()
{
	if (Button_0 && Button_0->GetVisibility() == ESlateVisibility::Visible)
	{
		Button_0->SetKeyboardFocus();
		return;
	}

	if (Button_1 && Button_1->GetVisibility() == ESlateVisibility::Visible)
	{
		Button_1->SetKeyboardFocus();
		return;
	}

	SetKeyboardFocus();
}

void URoguelikeRewardWidget::SelectOption(int32 OptionIndex)
{
	if (RewardManager)
	{
		RewardManager->SelectReward(OptionIndex);
	}
}

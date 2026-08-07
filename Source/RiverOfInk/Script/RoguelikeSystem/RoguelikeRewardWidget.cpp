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

	if (Title_0 && bHasOption0)
	{
		Title_0->SetText(RewardOptions[0].Title);
	}
	if (Description_0 && bHasOption0)
	{
		Description_0->SetText(RewardOptions[0].Description);
	}
	if (Title_1 && bHasOption1)
	{
		Title_1->SetText(RewardOptions[1].Title);
	}
	if (Description_1 && bHasOption1)
	{
		Description_1->SetText(RewardOptions[1].Description);
	}
	if (bHasOption0)
	{
		StyleRewardCard(Button_0, Title_0, Description_0, RewardOptions[0]);
	}
	if (bHasOption1)
	{
		StyleRewardCard(Button_1, Title_1, Description_1, RewardOptions[1]);
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

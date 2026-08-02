// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoguelikeSystem/RoguelikeRewardWidget.h"

#include "RoguelikeSystem/RoguelikeRewardManager.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	UTexture2D* LoadRewardIcon(const FRoguelikeRewardOption& Option)
	{
		const TCHAR* IconPath = TEXT("/Game/UI/Icons/Icon_Cooldown.Icon_Cooldown");
		if (Option.SkillID == EPlayerSkillID::CircularSlash)
		{
			IconPath = Option.UpgradeType == ESkillUpgradeType::Mechanic
				? TEXT("/Game/UI/Icons/Icon_Range.Icon_Range")
				: TEXT("/Game/UI/Icons/Icon_CircularSlash.Icon_CircularSlash");
		}
		else if (Option.SkillID == EPlayerSkillID::TripleProjectile && Option.UpgradeType == ESkillUpgradeType::Mechanic)
		{
			IconPath = TEXT("/Game/UI/Icons/Icon_ProjectileCount.Icon_ProjectileCount");
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
				Image->SetDesiredSizeOverride(FVector2D(96.0f, 96.0f));
				Image->SetVisibility(ESlateVisibility::Visible);
				return;
			}
		}

		Image->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URoguelikeRewardWidget::SetupRewardOptions(ARoguelikeRewardManager* InRewardManager, const TArray<FRoguelikeRewardOption>& InOptions)
{
	RewardManager = InRewardManager;
	RewardOptions = InOptions;
	const bool bHasOption0 = RewardOptions.IsValidIndex(0);
	const bool bHasOption1 = RewardOptions.IsValidIndex(1);
	if (Button_0)
	{
		Button_0->SetVisibility(bHasOption0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Button_1)
	{
		Button_1->SetVisibility(bHasOption1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
	SetRewardIcon(Icon_0, RewardOptions.IsValidIndex(0) ? &RewardOptions[0] : nullptr);
	SetRewardIcon(Icon_1, RewardOptions.IsValidIndex(1) ? &RewardOptions[1] : nullptr);

	OnRewardOptionsSet(RewardOptions);
}

void URoguelikeRewardWidget::SelectOption(int32 OptionIndex)
{
	if (RewardManager)
	{
		RewardManager->SelectReward(OptionIndex);
	}
}

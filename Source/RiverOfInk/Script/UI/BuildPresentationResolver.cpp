// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BuildPresentationResolver.h"

#include "Player/Skill/SkillComponent.h"

namespace
{
	FCombatBuildPresentationDefinition MakeDefinition(
		const TCHAR* BuildId,
		ECombatBuildCategory Category,
		ERoguelikeRewardType RewardType,
		EPlayerSkillID SkillID,
		ESkillUpgradeType UpgradeType,
		EPlayerSkillForm TargetSkillForm,
		ESkillModifierID ModifierID,
		const TCHAR* IconKey,
		const TCHAR* TitleKey,
		const TCHAR* DescriptionKey,
		int32 SortOrder)
	{
		FCombatBuildPresentationDefinition Definition;
		Definition.BuildId = FName(BuildId);
		Definition.Category = Category;
		Definition.CategoryId = FBuildPresentationResolver::GetCategoryId(Category);
		Definition.RewardType = RewardType;
		Definition.SkillID = SkillID;
		Definition.UpgradeType = UpgradeType;
		Definition.TargetSkillForm = TargetSkillForm;
		Definition.ModifierID = ModifierID;
		Definition.IconKey = FName(IconKey);
		Definition.TitleKey = FName(TitleKey);
		Definition.DescriptionKey = FName(DescriptionKey);
		Definition.SortOrder = SortOrder;
		return Definition;
	}

	FName GetSkillPrefix(EPlayerSkillID SkillID)
	{
		switch (SkillID)
		{
		case EPlayerSkillID::TripleProjectile:
			return FName(TEXT("Q"));
		case EPlayerSkillID::CircularSlash:
			return FName(TEXT("E"));
		default:
			return FName(TEXT("Skill"));
		}
	}

	FName GetModifierIdKey(ESkillModifierID ModifierID)
	{
		switch (ModifierID)
		{
		case ESkillModifierID::AddProjectile:
			return FName(TEXT("AddProjectile"));
		case ESkillModifierID::InkGrenade:
			return FName(TEXT("InkGrenade"));
		case ESkillModifierID::ExtraExplosion:
			return FName(TEXT("ExtraExplosion"));
		case ESkillModifierID::TwinSlash:
			return FName(TEXT("TwinSlash"));
		case ESkillModifierID::NullRing:
			return FName(TEXT("NullRing"));
		case ESkillModifierID::RadiusUp:
			return FName(TEXT("RadiusUp"));
		case ESkillModifierID::CooldownDown:
			return FName(TEXT("CooldownDown"));
		case ESkillModifierID::ProjectileHoming:
			return FName(TEXT("ProjectileHoming"));
		default:
			return FName(TEXT("Unknown"));
		}
	}

	FName GetUpgradeKey(ESkillUpgradeType UpgradeType)
	{
		switch (UpgradeType)
		{
		case ESkillUpgradeType::Mechanic:
			return FName(TEXT("Mechanic"));
		case ESkillUpgradeType::Cooldown:
			return FName(TEXT("Cooldown"));
		case ESkillUpgradeType::Damage:
			return FName(TEXT("Damage"));
		default:
			return FName(TEXT("Unknown"));
		}
	}

	FName GetFormKey(EPlayerSkillForm SkillForm)
	{
		switch (SkillForm)
		{
		case EPlayerSkillForm::TwoStageArc:
			return FName(TEXT("TwoStageArc"));
		case EPlayerSkillForm::TwinSlash:
			return FName(TEXT("TwinSlash"));
		case EPlayerSkillForm::NullRing:
			return FName(TEXT("NullRing"));
		case EPlayerSkillForm::ThrownGrenade:
			return FName(TEXT("InkGrenade"));
		default:
			return FName(TEXT("Default"));
		}
	}

	FText ResolveTitleText(FName TitleKey)
	{
		if (TitleKey == FName(TEXT("Build.Q.TripleProjectile.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.TripleProjectile.Title", "弹幕");
		}
		if (TitleKey == FName(TEXT("Build.Q.ProjectileCount.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.ProjectileCount.Title", "投射物增幅");
		}
		if (TitleKey == FName(TEXT("Build.Q.InkGrenade.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.InkGrenade.Title", "范围墨弹");
		}
		if (TitleKey == FName(TEXT("Build.Q.ExtraExplosion.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.ExtraExplosion.Title", "余烬连爆");
		}
		if (TitleKey == FName(TEXT("Build.Q.CooldownDown.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.CooldownDown.Title", "疾速回转");
		}
		if (TitleKey == FName(TEXT("Build.Q.ProjectileHoming.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.ProjectileHoming.Title", "引墨");
		}
		if (TitleKey == FName(TEXT("Build.E.CircularSlash.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "E.CircularSlash.Title", "环斩");
		}
		if (TitleKey == FName(TEXT("Build.E.TwoStageArc.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "E.TwoStageArc.Title", "两段弧斩");
		}
		if (TitleKey == FName(TEXT("Build.E.TwinSlash.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "E.TwinSlash.Title", "双重环斩");
		}
		if (TitleKey == FName(TEXT("Build.E.ProjectileErase.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "E.ProjectileErase.Title", "净墨环");
		}
		if (TitleKey == FName(TEXT("Build.E.RadiusUp.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "E.RadiusUp.Title", "扩展环斩");
		}
		if (TitleKey == FName(TEXT("Build.E.CooldownDown.Title")))
		{
			return NSLOCTEXT("BuildPresentation", "E.CooldownDown.Title", "回锋");
		}
		return FText::FromName(TitleKey);
	}

	FText ResolveDescriptionText(FName DescriptionKey)
	{
		if (DescriptionKey == FName(TEXT("Build.Q.TripleProjectile.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.TripleProjectile.Description", "Q 释放三枚沿朝向飞行的弹幕。");
		}
		if (DescriptionKey == FName(TEXT("Build.Q.ProjectileCount.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.ProjectileCount.Description", "Q 增加一枚投射物，范围墨弹形态同样生效。");
		}
		if (DescriptionKey == FName(TEXT("Build.Q.InkGrenade.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.InkGrenade.Description", "Q 投射物变为延迟落点范围攻击。");
		}
		if (DescriptionKey == FName(TEXT("Build.Q.ExtraExplosion.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.ExtraExplosion.Description", "每个 Q 范围落点追加一次范围冲击。");
		}
		if (DescriptionKey == FName(TEXT("Build.Q.CooldownDown.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.CooldownDown.Description", "Q 冷却时间缩短 0.5 秒。");
		}
		if (DescriptionKey == FName(TEXT("Build.Q.ProjectileHoming.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "Q.ProjectileHoming.Description", "Q 投射物会修正方向追踪被标记的目标。");
		}
		if (DescriptionKey == FName(TEXT("Build.E.CircularSlash.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "E.CircularSlash.Description", "E 释放一次近距离环形斩击。");
		}
		if (DescriptionKey == FName(TEXT("Build.E.TwoStageArc.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "E.TwoStageArc.Description", "E 分为两段近距离弧形攻击，首段命中后解锁第二段。");
		}
		if (DescriptionKey == FName(TEXT("Build.E.TwinSlash.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "E.TwinSlash.Description", "E 每段增加一次独立伤害判定。");
		}
		if (DescriptionKey == FName(TEXT("Build.E.ProjectileErase.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "E.ProjectileErase.Description", "E 斩击区域会抹除其中的敌方投射物。");
		}
		if (DescriptionKey == FName(TEXT("Build.E.RadiusUp.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "E.RadiusUp.Description", "E 斩击半径增加 60。");
		}
		if (DescriptionKey == FName(TEXT("Build.E.CooldownDown.Description")))
		{
			return NSLOCTEXT("BuildPresentation", "E.CooldownDown.Description", "E 冷却时间缩短 0.4 秒。");
		}
		return FText::FromName(DescriptionKey);
	}

}

FName FBuildPresentationResolver::ResolveIconKey(const FBuildHistoryEntry& Entry)
{
	if (Entry.RewardType == ERoguelikeRewardType::ChangeSkillForm)
	{
		return Entry.NewSkillForm == EPlayerSkillForm::TwoStageArc
			? FName(TEXT("TwoStageArc"))
			: FName(TEXT("SkillForm"));
	}

	if (Entry.RewardType == ERoguelikeRewardType::GainSkill)
	{
		return Entry.SkillID == EPlayerSkillID::CircularSlash
			? FName(TEXT("CircularSlash"))
			: FName(TEXT("TripleProjectile"));
	}

	if (Entry.RewardType == ERoguelikeRewardType::UpgradeSkill)
	{
		switch (Entry.UpgradeType)
		{
		case ESkillUpgradeType::Cooldown:
			return FName(TEXT("Cooldown"));
		case ESkillUpgradeType::Damage:
			return FName(TEXT("Damage"));
		case ESkillUpgradeType::Mechanic:
		default:
			return FName(TEXT("ProjectileCount"));
		}
	}

	switch (Entry.ModifierID)
	{
	case ESkillModifierID::AddProjectile:
		return FName(TEXT("ProjectileCount"));
	case ESkillModifierID::InkGrenade:
		return FName(TEXT("InkGrenade"));
	case ESkillModifierID::ExtraExplosion:
		return FName(TEXT("ExtraExplosion"));
	case ESkillModifierID::TwinSlash:
		return FName(TEXT("TwinSlash"));
	case ESkillModifierID::NullRing:
		return FName(TEXT("ProjectileErase"));
	case ESkillModifierID::RadiusUp:
		return FName(TEXT("Radius"));
	case ESkillModifierID::CooldownDown:
		return FName(TEXT("Cooldown"));
	case ESkillModifierID::ProjectileHoming:
		return FName(TEXT("ProjectileHoming"));
	default:
		return FName(TEXT("ProjectileCount"));
	}
}

ECombatBuildIconPlaceholderKind FBuildPresentationResolver::ResolvePlaceholderKind(FName IconKey)
{
	if (IconKey == FName(TEXT("TwoStageArc")))
	{
		return ECombatBuildIconPlaceholderKind::TwoStageArc;
	}
	if (IconKey == FName(TEXT("TwinSlash")))
	{
		return ECombatBuildIconPlaceholderKind::TwinSlash;
	}
	if (IconKey == FName(TEXT("Cooldown")))
	{
		return ECombatBuildIconPlaceholderKind::Cooldown;
	}
	return ECombatBuildIconPlaceholderKind::Generic;
}

FName FBuildPresentationResolver::MakeBuildId(const FBuildHistoryEntry& Entry)
{
	const FName SkillPrefix = GetSkillPrefix(Entry.SkillID);
	if (Entry.RewardType == ERoguelikeRewardType::ChangeSkillForm)
	{
		return FName(*FString::Printf(TEXT("%s.%s"), *SkillPrefix.ToString(), *GetFormKey(Entry.NewSkillForm).ToString()));
	}
	if (Entry.RewardType == ERoguelikeRewardType::GainSkill)
	{
		const TCHAR* SkillName = Entry.SkillID == EPlayerSkillID::CircularSlash
			? TEXT("CircularSlash")
			: TEXT("TripleProjectile");
		return FName(*FString::Printf(TEXT("%s.%s"), *SkillPrefix.ToString(), SkillName));
	}
	if (Entry.RewardType == ERoguelikeRewardType::UpgradeSkill)
	{
		return FName(*FString::Printf(
			TEXT("%s.Upgrade.%s"),
			*SkillPrefix.ToString(),
			*GetUpgradeKey(Entry.UpgradeType).ToString()));
	}

	return FName(*FString::Printf(
		TEXT("%s.%s"),
		*SkillPrefix.ToString(),
		*GetModifierIdKey(Entry.ModifierID).ToString()));
}

const TArray<FCombatBuildPresentationDefinition>& FBuildPresentationResolver::GetBuildCatalog()
{
	static const TArray<FCombatBuildPresentationDefinition> Catalog = []()
	{
		TArray<FCombatBuildPresentationDefinition> Definitions;
		Definitions.Reserve(12);

		Definitions.Add(MakeDefinition(
			TEXT("Q.TripleProjectile"), ECombatBuildCategory::QSkill, ERoguelikeRewardType::GainSkill,
			EPlayerSkillID::TripleProjectile, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::None,
			TEXT("TripleProjectile"), TEXT("Build.Q.TripleProjectile.Title"), TEXT("Build.Q.TripleProjectile.Description"), 0));
		Definitions.Add(MakeDefinition(
			TEXT("Q.AddProjectile"), ECombatBuildCategory::QSkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::TripleProjectile, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::AddProjectile,
			TEXT("ProjectileCount"), TEXT("Build.Q.ProjectileCount.Title"), TEXT("Build.Q.ProjectileCount.Description"), 10));
		Definitions.Add(MakeDefinition(
			TEXT("Q.InkGrenade"), ECombatBuildCategory::QSkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::TripleProjectile, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::InkGrenade,
			TEXT("InkGrenade"), TEXT("Build.Q.InkGrenade.Title"), TEXT("Build.Q.InkGrenade.Description"), 20));
		Definitions.Add(MakeDefinition(
			TEXT("Q.ExtraExplosion"), ECombatBuildCategory::QSkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::TripleProjectile, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::ExtraExplosion,
			TEXT("ExtraExplosion"), TEXT("Build.Q.ExtraExplosion.Title"), TEXT("Build.Q.ExtraExplosion.Description"), 30));
		Definitions.Add(MakeDefinition(
			TEXT("Q.CooldownDown"), ECombatBuildCategory::QSkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::TripleProjectile, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::CooldownDown,
			TEXT("Cooldown"), TEXT("Build.Q.CooldownDown.Title"), TEXT("Build.Q.CooldownDown.Description"), 40));
		Definitions.Add(MakeDefinition(
			TEXT("Q.ProjectileHoming"), ECombatBuildCategory::QSkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::TripleProjectile, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::ProjectileHoming,
			TEXT("ProjectileHoming"), TEXT("Build.Q.ProjectileHoming.Title"), TEXT("Build.Q.ProjectileHoming.Description"), 50));

		Definitions.Add(MakeDefinition(
			TEXT("E.CircularSlash"), ECombatBuildCategory::ESkill, ERoguelikeRewardType::GainSkill,
			EPlayerSkillID::CircularSlash, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::None,
			TEXT("CircularSlash"), TEXT("Build.E.CircularSlash.Title"), TEXT("Build.E.CircularSlash.Description"), 0));
		Definitions.Add(MakeDefinition(
			TEXT("E.TwoStageArc"), ECombatBuildCategory::ESkill, ERoguelikeRewardType::ChangeSkillForm,
			EPlayerSkillID::CircularSlash, ESkillUpgradeType::None, EPlayerSkillForm::TwoStageArc, ESkillModifierID::None,
			TEXT("TwoStageArc"), TEXT("Build.E.TwoStageArc.Title"), TEXT("Build.E.TwoStageArc.Description"), 10));
		Definitions.Add(MakeDefinition(
			TEXT("E.TwinSlash"), ECombatBuildCategory::ESkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::CircularSlash, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::TwinSlash,
			TEXT("TwinSlash"), TEXT("Build.E.TwinSlash.Title"), TEXT("Build.E.TwinSlash.Description"), 20));
		Definitions.Add(MakeDefinition(
			TEXT("E.NullRing"), ECombatBuildCategory::ESkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::CircularSlash, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::NullRing,
			TEXT("ProjectileErase"), TEXT("Build.E.ProjectileErase.Title"), TEXT("Build.E.ProjectileErase.Description"), 30));
		Definitions.Add(MakeDefinition(
			TEXT("E.RadiusUp"), ECombatBuildCategory::ESkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::CircularSlash, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::RadiusUp,
			TEXT("Radius"), TEXT("Build.E.RadiusUp.Title"), TEXT("Build.E.RadiusUp.Description"), 40));
		Definitions.Add(MakeDefinition(
			TEXT("E.CooldownDown"), ECombatBuildCategory::ESkill, ERoguelikeRewardType::Modifier,
			EPlayerSkillID::CircularSlash, ESkillUpgradeType::None, EPlayerSkillForm::Default, ESkillModifierID::CooldownDown,
			TEXT("Cooldown"), TEXT("Build.E.CooldownDown.Title"), TEXT("Build.E.CooldownDown.Description"), 50));

		return Definitions;
	}();
	return Catalog;
}

bool FBuildPresentationResolver::FindDefinition(FName BuildId, FCombatBuildPresentationDefinition& OutDefinition)
{
	for (const FCombatBuildPresentationDefinition& Definition : GetBuildCatalog())
	{
		if (Definition.BuildId == BuildId)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	return false;
}

FCombatBuildDetailsViewModel FBuildPresentationResolver::BuildViewModel(const USkillComponent* SkillComponent)
{
	FCombatBuildDetailsViewModel ViewModel;
	TSet<FName> AcquiredBuildIds;
	if (SkillComponent)
	{
		ViewModel.SourceHistoryCount = SkillComponent->GetBuildHistory().Num();
		for (const FBuildHistoryEntry& Entry : SkillComponent->GetBuildHistory())
		{
			AcquiredBuildIds.Add(MakeBuildId(Entry));
		}
	}

	for (const FCombatBuildPresentationDefinition& Definition : GetBuildCatalog())
	{
		// The catalog supplies presentation metadata and the build-list order. It
		// is not the runtime display list: an entry only becomes visible after the
		// current run records its acquisition in BuildHistory.
		if (!AcquiredBuildIds.Contains(Definition.BuildId))
		{
			continue;
		}

		FCombatBuildDetailsItem Item;
		Item.BuildId = Definition.BuildId;
		Item.Category = Definition.Category;
		Item.CategoryId = Definition.CategoryId;
		Item.SkillID = Definition.SkillID;
		Item.UpgradeType = Definition.UpgradeType;
		Item.TargetSkillForm = Definition.TargetSkillForm;
		Item.ModifierID = Definition.ModifierID;
		Item.IconKey = Definition.IconKey;
		Item.TitleKey = Definition.TitleKey;
		Item.DescriptionKey = Definition.DescriptionKey;
		Item.Title = ResolveTitleText(Definition.TitleKey);
		Item.Description = ResolveDescriptionText(Definition.DescriptionKey);
		Item.SortOrder = Definition.SortOrder;
		Item.bOwned = true;
		Item.StackCount = GetDefinitionStackCount(Definition, SkillComponent);
		ViewModel.Items.Add(MoveTemp(Item));
	}

	return ViewModel;
}

FName FBuildPresentationResolver::GetCategoryId(ECombatBuildCategory Category)
{
	switch (Category)
	{
	case ECombatBuildCategory::BasicAttack:
		return FName(TEXT("BasicAttack"));
	case ECombatBuildCategory::Projectile:
		return FName(TEXT("Projectile"));
	case ECombatBuildCategory::QSkill:
		return FName(TEXT("QSkill"));
	case ECombatBuildCategory::ESkill:
		return FName(TEXT("ESkill"));
	case ECombatBuildCategory::General:
	default:
		return FName(TEXT("General"));
	}
}

FText FBuildPresentationResolver::GetCategoryLabel(ECombatBuildCategory Category)
{
	switch (Category)
	{
	case ECombatBuildCategory::BasicAttack:
		return NSLOCTEXT("BuildPresentation", "Category.BasicAttack", "左键近战");
	case ECombatBuildCategory::Projectile:
		return NSLOCTEXT("BuildPresentation", "Category.Projectile", "右键弹幕");
	case ECombatBuildCategory::QSkill:
		return NSLOCTEXT("BuildPresentation", "Category.QSkill", "Q 技能");
	case ECombatBuildCategory::ESkill:
		return NSLOCTEXT("BuildPresentation", "Category.ESkill", "E 技能");
	case ECombatBuildCategory::General:
	default:
		return NSLOCTEXT("BuildPresentation", "Category.General", "通用属性");
	}
}

FString FBuildPresentationResolver::GetRedrawnIconObjectPath(FName IconKey)
{
	const FString Stem = IconKey.ToString();
	return FString::Printf(
		TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_%s_Redrawn.T_UI_Build_%s_Redrawn"),
		*Stem,
		*Stem);
}

FString FBuildPresentationResolver::GetLegacyIconObjectPath(FName IconKey)
{
	if (IconKey == FName(TEXT("CircularSlash")) || IconKey == FName(TEXT("TwoStageArc")))
	{
		return TEXT("/Game/RawContent/UI/Texture/Icon_CircularSlash.Icon_CircularSlash");
	}
	if (IconKey == FName(TEXT("TripleProjectile")))
	{
		return TEXT("/Game/RawContent/UI/Texture/Icon_TripleProjectile.Icon_TripleProjectile");
	}

	const FString Stem = IconKey.ToString();
	return FString::Printf(
		TEXT("/Game/RawContent/UI/Reward/Textures/T_UI_Build_%s.T_UI_Build_%s"),
		*Stem,
		*Stem);
}

bool FBuildPresentationResolver::IsDefinitionOwned(
	const FCombatBuildPresentationDefinition& Definition,
	const USkillComponent* SkillComponent)
{
	if (!SkillComponent)
	{
		return false;
	}
	const bool bHasHistoryMatch = [&SkillComponent, &Definition]()
	{
		for (const FBuildHistoryEntry& Entry : SkillComponent->GetBuildHistory())
		{
			if (FBuildPresentationResolver::MakeBuildId(Entry) == Definition.BuildId)
			{
				return true;
			}
		}
		return false;
	}();

	if (Definition.ModifierID != ESkillModifierID::None)
	{
		return SkillComponent->GetModifierStack(Definition.SkillID, Definition.ModifierID) > 0
			|| bHasHistoryMatch;
	}

	if (Definition.TargetSkillForm != EPlayerSkillForm::Default)
	{
		return SkillComponent->GetSkillForm(Definition.SkillID) == Definition.TargetSkillForm
			|| bHasHistoryMatch;
	}

	if (Definition.UpgradeType != ESkillUpgradeType::None)
	{
		const FSkillUpgradeState State = SkillComponent->GetSkillUpgradeState(Definition.SkillID);
		switch (Definition.UpgradeType)
		{
		case ESkillUpgradeType::Mechanic:
			return State.MechanicLevel > 0 || bHasHistoryMatch;
		case ESkillUpgradeType::Cooldown:
			return State.CooldownLevel > 0 || bHasHistoryMatch;
		case ESkillUpgradeType::Damage:
			return State.DamageLevel > 0 || bHasHistoryMatch;
		default:
			break;
		}
	}

	if (SkillComponent->HasSkill(Definition.SkillID))
	{
		return true;
	}
	return bHasHistoryMatch;
}

int32 FBuildPresentationResolver::GetDefinitionStackCount(
	const FCombatBuildPresentationDefinition& Definition,
	const USkillComponent* SkillComponent)
{
	if (!SkillComponent)
	{
		return 0;
	}

	if (Definition.ModifierID != ESkillModifierID::None)
	{
		return SkillComponent->GetModifierStack(Definition.SkillID, Definition.ModifierID);
	}

	if (Definition.TargetSkillForm != EPlayerSkillForm::Default)
	{
		return SkillComponent->GetSkillForm(Definition.SkillID) == Definition.TargetSkillForm ? 1 : 0;
	}

	if (Definition.UpgradeType != ESkillUpgradeType::None)
	{
		const FSkillUpgradeState State = SkillComponent->GetSkillUpgradeState(Definition.SkillID);
		switch (Definition.UpgradeType)
		{
		case ESkillUpgradeType::Mechanic:
			return State.MechanicLevel;
		case ESkillUpgradeType::Cooldown:
			return State.CooldownLevel;
		case ESkillUpgradeType::Damage:
			return State.DamageLevel;
		default:
			break;
		}
	}

	const int32 SkillSlotIndex = SkillComponent->FindSkillSlot(Definition.SkillID);
	return SkillComponent->SkillSlots.IsValidIndex(SkillSlotIndex)
		? SkillComponent->SkillSlots[SkillSlotIndex].SkillLevel
		: 0;
}

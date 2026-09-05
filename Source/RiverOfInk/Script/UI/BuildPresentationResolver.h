// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "UI/CombatBuildIconPlaceholderWidget.h"
#include "BuildPresentationResolver.generated.h"

class USkillComponent;

/** Stable rows used by the build-details whitebox and the later data-driven UI. */
UENUM(BlueprintType)
enum class ECombatBuildCategory : uint8
{
	BasicAttack UMETA(DisplayName = "Basic Attack"),
	Projectile UMETA(DisplayName = "Projectile"),
	QSkill UMETA(DisplayName = "Q Skill"),
	ESkill UMETA(DisplayName = "E Skill"),
	General UMETA(DisplayName = "General")
};

/** Static presentation contract for one stable build entry. */
USTRUCT(BlueprintType)
struct FCombatBuildPresentationDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	FName BuildId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	ECombatBuildCategory Category = ECombatBuildCategory::General;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	FName CategoryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	ERoguelikeRewardType RewardType = ERoguelikeRewardType::Modifier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	EPlayerSkillID SkillID = EPlayerSkillID::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	ESkillUpgradeType UpgradeType = ESkillUpgradeType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	EPlayerSkillForm TargetSkillForm = EPlayerSkillForm::Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	ESkillModifierID ModifierID = ESkillModifierID::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	FName IconKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	FName TitleKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	FName DescriptionKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build|Presentation")
	int32 SortOrder = 0;
};

/** Runtime-owned, read-only presentation data for a build entry. */
USTRUCT(BlueprintType)
struct FCombatBuildDetailsItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	FName BuildId;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	ECombatBuildCategory Category = ECombatBuildCategory::General;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	FName CategoryId;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	EPlayerSkillID SkillID = EPlayerSkillID::None;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	ESkillUpgradeType UpgradeType = ESkillUpgradeType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	EPlayerSkillForm TargetSkillForm = EPlayerSkillForm::Default;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	ESkillModifierID ModifierID = ESkillModifierID::None;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	FName IconKey;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	FName TitleKey;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	FName DescriptionKey;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	int32 StackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	int32 SortOrder = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	bool bOwned = false;
};

/** Complete snapshot consumed by the details widget; it owns no gameplay state. */
USTRUCT(BlueprintType)
struct FCombatBuildDetailsViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	TArray<FCombatBuildDetailsItem> Items;

	UPROPERTY(BlueprintReadOnly, Category = "Build|Details")
	int32 SourceHistoryCount = 0;
};

/** Shared stable-ID, catalog, text and IconKey resolution for all build HUDs. */
class RIVEROFINK_API FBuildPresentationResolver
{
public:
	static FName ResolveIconKey(const FBuildHistoryEntry& Entry);
	static ECombatBuildIconPlaceholderKind ResolvePlaceholderKind(FName IconKey);
	static FName MakeBuildId(const FBuildHistoryEntry& Entry);

	static const TArray<FCombatBuildPresentationDefinition>& GetBuildCatalog();
	static bool FindDefinition(FName BuildId, FCombatBuildPresentationDefinition& OutDefinition);
	static FCombatBuildDetailsViewModel BuildViewModel(const USkillComponent* SkillComponent);

	static FName GetCategoryId(ECombatBuildCategory Category);
	static FText GetCategoryLabel(ECombatBuildCategory Category);
	static FString GetRedrawnIconObjectPath(FName IconKey);
	static FString GetLegacyIconObjectPath(FName IconKey);

private:
	static bool IsDefinitionOwned(
		const FCombatBuildPresentationDefinition& Definition,
		const USkillComponent* SkillComponent);
	static int32 GetDefinitionStackCount(
		const FCombatBuildPresentationDefinition& Definition,
		const USkillComponent* SkillComponent);
};

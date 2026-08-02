// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeRewardWidget.generated.h"

class ARoguelikeRewardManager;
class UTextBlock;
class UImage;
class UButton;

/** Base class for WBP_RoguelikeReward. Bind each button to SelectOption. */
UCLASS(Abstract, Blueprintable)
class RIVEROFINK_API URoguelikeRewardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Reward")
	void SetupRewardOptions(ARoguelikeRewardManager* InRewardManager, const TArray<FRoguelikeRewardOption>& InOptions);

	UFUNCTION(BlueprintCallable, Category = "Reward")
	void SelectOption(int32 OptionIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "Reward")
	void OnRewardOptionsSet(const TArray<FRoguelikeRewardOption>& InOptions);

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TObjectPtr<ARoguelikeRewardManager> RewardManager;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TArray<FRoguelikeRewardOption> RewardOptions;

	/** 可选绑定，控件树存在时由 C++ 负责写入运行时奖励标题和描述。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Title_0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Description_0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Title_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Description_1;

	/** Optional icon bindings used by the whitebox reward cards. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon_0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_1;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeRewardManager.generated.h"

class APlayerCharacter;
class USkillComponent;
class URoguelikeRewardWidget;
class ARoguelikeExitTrigger;

DECLARE_LOG_CATEGORY_EXTERN(LogRoguelike, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnRoguelikeRewardApplied,
	const FRoguelikeRewardOption&,
	Reward
);

/** Owns room-clear reward generation, UI display, and reward application. */
UCLASS(Blueprintable)
class RIVEROFINK_API ARoguelikeRewardManager : public AActor
{
	GENERATED_BODY()

public:
	ARoguelikeRewardManager();

	UFUNCTION(BlueprintCallable, Category = "Reward")
	void ShowRewardAfterRoomClear();

	UFUNCTION(BlueprintCallable, Category = "Reward")
	TArray<FRoguelikeRewardOption> GenerateRewardOptions();

	UFUNCTION(BlueprintCallable, Category = "Reward")
	void SelectReward(int32 OptionIndex);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|UI")
	TSubclassOf<URoguelikeRewardWidget> RewardWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Runtime")
	TArray<FRoguelikeRewardOption> CurrentRewardOptions;

	/** 当前关卡是否已经成功展示过一次奖励 UI。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reward|Runtime")
	bool bRewardShownForRoom = false;

	/** Broadcast after the selected reward has been applied and the reward UI is closed. */
	UPROPERTY(BlueprintAssignable, Category = "Reward|Events")
	FOnRoguelikeRewardApplied OnRewardApplied;

	/** Creates a whitebox exit when the level does not already contain one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Exit")
	bool bAutoCreateExitTrigger = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Exit")
	TSubclassOf<ARoguelikeExitTrigger> ExitTriggerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Exit")
	FVector ExitTriggerSpawnOffset = FVector(0.0f, 500.0f, 120.0f);

protected:
	virtual void BeginPlay() override;

private:
	bool ResolvePlayer();
	void EnsureExitTrigger();
	bool ApplyReward(const FRoguelikeRewardOption& Reward);
	void CloseRewardUI();
	FRoguelikeRewardOption MakeOption(
		ERoguelikeRewardType RewardType,
		EPlayerSkillID SkillID,
		ESkillUpgradeType UpgradeType,
		EPlayerSkillForm TargetSkillForm,
		const FText& Title,
		const FText& Description) const;
	bool bRewardSelectionInProgress = false;

	UPROPERTY(Transient)
	TObjectPtr<URoguelikeRewardWidget> ActiveRewardWidget;

	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> CachedPlayer;

	UPROPERTY(Transient)
	TObjectPtr<USkillComponent> CachedSkillComponent;

	UPROPERTY(Transient)
	TObjectPtr<ARoguelikeExitTrigger> ActiveExitTrigger;
};

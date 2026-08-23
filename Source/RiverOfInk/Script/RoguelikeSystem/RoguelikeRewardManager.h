// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RiverOfInk.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeRewardManager.generated.h"

class APlayerCharacter;
class USkillComponent;
class URoguelikeRewardWidget;
class ARoguelikeExitTrigger;

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

	/** Development-only helper that displays one legal reward selected by identifier. */
	UFUNCTION(BlueprintCallable, Category = "Reward|Debug")
	bool DebugShowSpecificReward(const FString& RewardIdentifier);

	/** Development-only helper that displays and immediately selects one legal reward. */
	UFUNCTION(BlueprintCallable, Category = "Reward|Debug")
	bool DebugSelectSpecificReward(const FString& RewardIdentifier);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|UI")
	TSubclassOf<URoguelikeRewardWidget> RewardWidgetClass;

	/** Number of choices shown by the generic reward row. The pool supports 2-3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|UI", meta = (ClampMin = "2", ClampMax = "3"))
	int32 RewardOptionCount = 3;

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

	/** 传送门视觉蓝图（赋值后生成出口时替换默认绿色圆柱；未设置则用圆柱） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Exit")
	TSubclassOf<AActor> ExitVisualClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Exit")
	FVector ExitTriggerSpawnOffset = FVector(0.0f, 500.0f, 120.0f);

protected:
	virtual void BeginPlay() override;

private:
	bool ResolvePlayer();
	void EnsureExitTrigger();
	bool ApplyReward(const FRoguelikeRewardOption& Reward);
	void CloseRewardUI();
	void FinishRewardSelection();
	FRoguelikeRewardOption MakeOption(
		ERoguelikeRewardType RewardType,
		EPlayerSkillID SkillID,
		ESkillUpgradeType UpgradeType,
		EPlayerSkillForm TargetSkillForm,
		const FText& Title,
		const FText& Description) const;
	FRoguelikeRewardOption MakeCurrencyOption(int32 Amount, const FText& Title, const FText& Description) const;
	FRoguelikeRewardOption MakeHealthOption(float RecoveryAmount, const FText& Title, const FText& Description) const;
	FRoguelikeRewardOption MakeModifierOption(
		EPlayerSkillID SkillID,
		ESkillModifierID ModifierID,
		int32 StackDelta,
		const FText& Title,
		const FText& Description) const;
	bool TryBuildDebugRewardOption(
		const FString& RewardIdentifier,
		FRoguelikeRewardOption& OutOption) const;
	void FillModifierPreview(FRoguelikeRewardOption& Option) const;
	void PopulateRewardPresentation(FRoguelikeRewardOption& Option) const;
	bool bRewardSelectionInProgress = false;

	/** One-shot override consumed by ShowRewardAfterRoomClear for PIE reward tests. */
	UPROPERTY(Transient)
	TArray<FRoguelikeRewardOption> DebugRewardOverrideOptions;

	UPROPERTY(Transient)
	FRoguelikeRewardOption PendingSelectedReward;

	UPROPERTY(Transient)
	TObjectPtr<URoguelikeRewardWidget> ActiveRewardWidget;

	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> CachedPlayer;

	UPROPERTY(Transient)
	TObjectPtr<USkillComponent> CachedSkillComponent;

	UPROPERTY(Transient)
	TObjectPtr<ARoguelikeExitTrigger> ActiveExitTrigger;
};

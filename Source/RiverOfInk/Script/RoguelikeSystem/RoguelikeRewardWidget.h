// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeRewardWidget.generated.h"

class ARoguelikeRewardManager;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UOverlay;
class UTextBlock;
class UTexture2D;
class URoguelikeRewardOptionWidget;

/** Native reward-selection screen used by WBP_RoguelikeReward. */
UCLASS(Abstract, Blueprintable)
class RIVEROFINK_API URoguelikeRewardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Reward")
	void SetupRewardOptions(ARoguelikeRewardManager* InRewardManager, const TArray<FRoguelikeRewardOption>& InOptions);

	UFUNCTION(BlueprintCallable, Category = "Reward")
	void SelectOption(int32 OptionIndex);

	UFUNCTION(BlueprintCallable, Category = "Reward")
	bool PlaySelectionFeedback(int32 OptionIndex);

	UFUNCTION(BlueprintCallable, Category = "Reward")
	void SetSelectionLocked(bool bLocked);

	void SetSelectionFinishedCallback(FSimpleDelegate InCallback)
	{
		SelectionFinishedCallback = MoveTemp(InCallback);
	}

	/** Give keyboard/gamepad navigation a deterministic initial target. */
	UFUNCTION(BlueprintCallable, Category = "Reward|Focus")
	void FocusFirstOption();

	UFUNCTION(BlueprintImplementableEvent, Category = "Reward")
	void OnRewardOptionsSet(const TArray<FRoguelikeRewardOption>& InOptions);

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TObjectPtr<ARoguelikeRewardManager> RewardManager;

	UPROPERTY(BlueprintReadOnly, Category = "Reward")
	TArray<FRoguelikeRewardOption> RewardOptions;

	/** Replaceable title divider art; the runtime fallback loads the placeholder texture by path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Style")
	TObjectPtr<UTexture2D> TitleDividerTexture;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void ConfigureWidgetTree();
	void HandleSelectionFinished();
	void HandleOptionHovered(int32 OptionIndex);
	void HandleOptionUnhovered(int32 OptionIndex);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BackgroundOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TitleDecoration;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> RewardOptionsRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URoguelikeRewardOptionWidget>> OptionWidgets;

	FSimpleDelegate SelectionFinishedCallback;
	bool bSelectionLocked = false;
	bool bNativeTreeBuilt = false;
};

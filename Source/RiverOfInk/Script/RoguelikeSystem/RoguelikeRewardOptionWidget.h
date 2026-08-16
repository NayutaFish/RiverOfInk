// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeRewardOptionWidget.generated.h"

class UButton;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidgetAnimation;

/** Native, data-driven presentation for one reward choice. */
DECLARE_DELEGATE_OneParam(FRoguelikeRewardOptionIndexDelegate, int32 /*OptionIndex*/);
DECLARE_DELEGATE(FRoguelikeRewardOptionFinishedDelegate);

/**
 * One generic reward option. The option owns its hit area and visual state so
 * a reward screen can display two or three choices without fixed card fields.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API URoguelikeRewardOptionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URoguelikeRewardOptionWidget(const FObjectInitializer& ObjectInitializer);

	void InitializeRewardOption(const FRoguelikeRewardOption& InOption, int32 InOptionIndex);

	bool PlaySelectionFeedback();
	void PlayFadeOut();
	void SetHoverState(bool bHovered);
	void SetInteractionEnabled(bool bEnabled);
	void FocusOption();

	void SetSelectionFinishedCallback(FRoguelikeRewardOptionFinishedDelegate InCallback)
	{
		SelectionFinishedCallback = MoveTemp(InCallback);
	}

	UButton* GetHitArea() const { return ButtonHitArea; }

	FRoguelikeRewardOptionIndexDelegate OnOptionClicked;
	FRoguelikeRewardOptionIndexDelegate OnOptionHovered;
	FRoguelikeRewardOptionIndexDelegate OnOptionUnhovered;

	/** Replaceable vertical ink brush used by the selection feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Style")
	TObjectPtr<UTexture2D> SelectionBrushTexture;

	/** Subtle dry-brush halo shown only while this option is hovered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Style")
	TObjectPtr<UTexture2D> HoverInkTexture;

	/** Small permanent divider beneath the option content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Style")
	TObjectPtr<UTexture2D> SmallDividerTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Style", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SelectionSweepDuration = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SelectionHoldDuration = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward|Style", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float FadeOutDuration = 0.65f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void BuildAnimations();
	void SetTextStyle(UTextBlock* TextBlock, int32 FontSize, const FLinearColor& Color) const;
	void SetButtonStyle();
	void SetVisualScale(float Scale);
	void FinishSelectionFeedback();
	void HandleSelectionHoldFinished();

	UFUNCTION()
	void HandleButtonClicked();

	UFUNCTION()
	void HandleButtonHovered();

	UFUNCTION()
	void HandleButtonUnhovered();

	UFUNCTION()
	void HandleFadeAnimationFinished();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ButtonHitArea;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContentGroup;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ImageRewardIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TextRewardCategory;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TextRewardTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TextSkillInfo;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TextBuildType;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TextValueChange;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TextDescription;

	UPROPERTY(Transient)
	TObjectPtr<UImage> HoverInkImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SmallDividerImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ImageSelectionBrush;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> SelectionBrushAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> HoverInAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> HoverOutAnimation;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> FadeOutAnimation;

	UPROPERTY(Transient)
	FRoguelikeRewardOption RewardOption;

	int32 OptionIndex = INDEX_NONE;
	bool bSelectionPlaying = false;
	bool bInteractionEnabled = true;
	bool bHovered = false;

	FRoguelikeRewardOptionFinishedDelegate SelectionFinishedCallback;
	FTimerHandle SelectionHoldTimer;
};

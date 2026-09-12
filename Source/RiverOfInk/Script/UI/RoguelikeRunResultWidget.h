// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/SettlementDataRecorder.h"
#include "RoguelikeSystem/RoguelikeRunTypes.h"
#include "RoguelikeRunResultWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UWidget;

/**
 * Native, fully opaque result page. It deliberately owns only presentation and
 * button intent; the GameInstance run-flow subsystem owns the result snapshot
 * and all state transitions.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API URoguelikeRunResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Read the already-frozen current-run snapshot and start its one-shot entrance sequence. */
	void OpenForCurrentRun();

	/** Stop visual callbacks and restore gameplay input before a successful map transition. */
	void CloseForTransition();

	/** First-pass timing defaults from the Result HUD technical plan. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.0"))
	float IntroDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "1.0"))
	float StatPulseMaxScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.01"))
	float StatFadeDuration = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.01"))
	float StatPulsePeakTime = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.01"))
	float StatPulseDuration = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.0"))
	float StatGap = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.0"))
	float RewardStartDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.01"))
	float RewardFadeDuration = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.0"))
	float RewardGap = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Animation", meta = (ClampMin = "0.01"))
	float ActionsFadeDuration = 0.20f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildDefaultWidgetTree();
	void PopulateSnapshot(const FRoguelikeRunResultSnapshot& Snapshot, ERoguelikeRunOutcome Outcome);
	void ResetEntranceState();
	void UpdateEntranceSequence(float ElapsedSeconds);
	void SetActionsEnabled(bool bEnabled);
	void ConfigureResultInput(bool bResultOpen);
	UFUNCTION()
	void RequestRestart();
	UFUNCTION()
	void RequestPreparationRoom();
	void BeginLeaveRequest();
	void RestoreAfterLeaveFailure(const FText& FailureMessage);
	UWidget* BuildRewardEntry(const FSettlementRewardPick& Pick, int32 DisplayIndex);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	/** Screen-anchored town art, kept outside the fixed 16:9 content canvas. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> TownDecorationLayer;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ContentRoot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutcomeTitleText;

	/** Transparent brush-calligraphy image; the text block remains a resilient fallback. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> OutcomeTitleImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutcomeSubtitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> StatRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StatValueTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> RewardEntries;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EmptyRewardsText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ActionRoot;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RestartButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PreparationButton;

	bool bNativeTreeBuilt = false;
	bool bSequencePlaying = false;
	bool bLeaveRequested = false;
	double SequenceStartSeconds = 0.0;
};

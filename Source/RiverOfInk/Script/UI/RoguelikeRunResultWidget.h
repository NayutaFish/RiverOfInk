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

	// ── 底部两个动作按钮的动效（鼠标悬停 / 手柄·键盘焦点）──

	/** 落在某一项上时的常态放大。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Action Buttons", meta = (ClampMin = "1.0"))
	float ActionActiveScale = 1.05f;

	/** 刚落到某一项上时额外弹一下的幅度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Action Buttons", meta = (ClampMin = "0.0"))
	float ActionPulseScale = 0.05f;

	/** 弹一下的时长（秒）；0 = 不做脉冲。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Action Buttons", meta = (ClampMin = "0.0", Units = "s"))
	float ActionPulseDuration = 0.22f;

	/** 脉冲期间向上抬起的像素（回落归零，不会残留位移）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Action Buttons", meta = (ClampMin = "0.0"))
	float ActionPulseLift = 7.0f;

	/** 停在该项上时的呼吸缩放幅度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Action Buttons", meta = (ClampMin = "0.0"))
	float ActionBreathScale = 0.008f;

	/** 呼吸一个来回的时长（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result|Action Buttons", meta = (ClampMin = "0.05", Units = "s"))
	float ActionBreathPeriod = 1.6f;

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

	// ── 底部按钮动效 ──

	/**
	 * 逐帧合成两个按钮的缩放与抬起：悬停或 Slate 焦点（手柄/键盘）都算“落点”，
	 * 落上时弹一下 + 常态放大 + 缓慢呼吸。只在数值真变了才写回，避免每帧打脏布局。
	 */
	void UpdateActionButtonVisuals();

	/** 落点变化时切换主按钮笔刷提亮 / 次按钮文字与下划线配色。 */
	void ApplyActionButtonHighlight(const UButton* Button, bool bHighlighted);

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

	/** 主按钮（再来一局）的笔刷图：落点上时暖色提亮。 */
	UPROPERTY(Transient)
	TObjectPtr<UImage> RestartBrushImage;

	/** 次按钮（返回准备区）的文字与下划线：落点上时改成落款红。 */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PreparationLabel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PreparationRule;

	/** 参与动效的两个按钮，顺序固定为 { 再来一局, 返回准备区 }。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ActionButtons;

	/** 各按钮在 ActionCanvas 上的基准位置（抬起动画结束后要还原）。 */
	TArray<FVector2D> ActionButtonBasePositions;

	/** 已经写回的缩放与抬起量，用来避免每帧重复写。 */
	TArray<float> ActionAppliedScale;
	TArray<float> ActionAppliedLift;

	/** 各按钮上一帧是否处于“落点上”，用来检测落点变化并触发脉冲。 */
	TArray<bool> ActionActiveFlags;

	/** 正在脉冲的按钮；INDEX_NONE = 没有。时间基准用真实时间，不受暂停/时间膨胀影响。 */
	int32 ActionPulseIndex = INDEX_NONE;
	double ActionPulseStartSeconds = 0.0;
	double ActionBreathStartSeconds = 0.0;

	bool bNativeTreeBuilt = false;
	bool bSequencePlaying = false;
	bool bLeaveRequested = false;
	double SequenceStartSeconds = 0.0;
};

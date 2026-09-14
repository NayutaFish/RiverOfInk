// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stage01IntroDirector.generated.h"

class ACineCameraActor;
class AInkOverlay;
class ALevelSequenceActor;
class UButton;
class ULevelSequence;
class ULevelSequencePlayer;
class UUserWidget;

UENUM(BlueprintType)
enum class EStage01IntroState : uint8
{
	Idle,
	Playing,
	EffectPlaying,
	Fading,
	Traveling,
	Failed,
	HudFading,
	PreviewComplete
};

/** Owns the one-shot Stage 1 menu intro and the transition into RunFlow. */
UCLASS(Blueprintable)
class RIVEROFINK_API AStage01IntroDirector : public AActor
{
	GENERATED_BODY()

public:
	AStage01IntroDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Stage Intro")
	bool PlayIntro();

	UFUNCTION(BlueprintCallable, Category = "Stage Intro")
	void HandleSequenceFinished();

	UFUNCTION(BlueprintCallable, Category = "Stage Intro")
	bool BeginTravelAfterFade();

	UFUNCTION(BlueprintCallable, Category = "Stage Intro")
	void AbortIntro();

	/** Reset the isolated ink preview without starting a map transition. */
	UFUNCTION(BlueprintCallable, Category = "Stage Intro|Preview")
	void ResetPreview();

	UFUNCTION(BlueprintPure, Category = "Stage Intro")
	EStage01IntroState GetIntroState() const { return IntroState; }

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Sequence")
	TObjectPtr<ULevelSequence> IntroSequence;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Scene")
	TObjectPtr<ACineCameraActor> IntroCamera;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Scene")
	TObjectPtr<AInkOverlay> InkOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Timing", meta = (ClampMin = "0.1"))
	float IntroDuration = 4.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Timing", meta = (ClampMin = "0.1"))
	float FadeDuration = 0.45f;

	/** Duration of the main-menu HUD fade before the intro sequence starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Timing", meta = (ClampMin = "0.0"))
	float MenuHudFadeDuration = 1.5f;

	/** Ease exponent for the HUD fade; 2.0 gives a smooth ease-in/ease-out curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Timing", meta = (ClampMin = "1.0"))
	float MenuHudFadeEaseExponent = 2.0f;

	// ── 主菜单手柄 / 键盘导航（十字键上下选择、A/回车确认）──

	/** 选中项脉冲动效时长（秒）。0 表示不做动效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Menu Input", meta = (ClampMin = "0.0", Units = "s"))
	float MenuSelectionPulseDuration = 0.22f;

	/** 脉冲期间额外放大的比例。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Menu Input", meta = (ClampMin = "0.0"))
	float MenuSelectionPulseScale = 0.06f;

	/** 脉冲期间向右轻推的像素（按钮靠左锚定，向左生长会顶出画面）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Menu Input")
	float MenuSelectionPulseOffsetX = 18.0f;

	/** 选中项静止时的缩放（比未选中项略大，配合 Focus 贴图做常驻“当前落点”提示）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Menu Input", meta = (ClampMin = "0.0"))
	float MenuSelectedRestScale = 1.03f;

	/** 选中项静止时的“呼吸”缩放幅度（叠在 MenuSelectedRestScale 上）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Menu Input", meta = (ClampMin = "0.0"))
	float MenuSelectionBreathScale = 0.008f;

	/** 呼吸一个来回的时长（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Menu Input", meta = (ClampMin = "0.05", Units = "s"))
	float MenuSelectionBreathPeriod = 1.6f;

	/**
	 * Deprecated. The intro no longer blends from the current PlayerController
	 * view; it cuts to the authored K0 camera pose before playing the sequence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Timing", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float CameraTransitionDuration = 2.25f;

	/**
	 * Deprecated. The sequence's authored K3 pose is kept as-is; no runtime
	 * translation is added after the camera track finishes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Camera")
	FVector CameraPushOffset = FVector(300.0f, 0.0f, -260.0f);

	/** The ink starts while the camera is still moving, preserving the 4.8-second opening beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Timing", meta = (ClampMin = "0.0"))
	float InkEffectStartTime = 0.8f;

	/** Duration of the layered ink effect from its first dot to full coverage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Timing", meta = (ClampMin = "0.0"))
	float InkEffectDuration = 3.5f;
	/** When enabled, the director stops after the ink reaches full coverage. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Preview")
	bool bPreviewOnly = false;

	/** Automatically starts the authored camera and ink preview after BeginPlay. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Preview")
	bool bAutoPlayOnBeginPlay = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Preview", meta = (ClampMin = "0.0"))
	float AutoPlayDelay = 0.25f;


	/** Preview-only: keep the straight-on acceptance camera instead of playing the production camera track. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Preview")
	bool bPreviewUseStaticCamera = false;
	/** Preview-only: restart after the full-ink beat so long PIE sessions remain inspectable. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Preview")
	bool bPreviewLoop = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Stage Intro|Preview", meta = (ClampMin = "0.0"))
	float PreviewLoopDelay = 0.35f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Intro|Runtime")
	bool bIntroPlaying = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Intro|Runtime")
	EStage01IntroState IntroState = EStage01IntroState::Idle;

protected:
	UFUNCTION()
	void OnNewGameClicked();

	UFUNCTION()
	void OnSequenceFinished();

	UFUNCTION()
	void HandleMenuBindTimer();

	UFUNCTION()
	void HandleFadeTimer();

	UFUNCTION()
	void HandleAutoPlayTimer();

	UFUNCTION()
	void StartSequenceAtAuthoredStart();

private:
	bool ResolveSceneReferences();
	bool CreateSequencePlayer();
	bool TryBindMainMenu();
	void ApplyMainMenuHudArt(UUserWidget* MenuWidget);
	void PrepareIntroCameraAtMenuStart();
	void MaintainIntroCameraOwnership();
	void SetIntroCameraOwnership(bool bOwnCamera);
	void SetMenuCinematicState(bool bCinematic);
	void RestoreMainMenuAfterFailure();
	void BeginMainMenuHudFade();
	void UpdateMainMenuHudFade(float DeltaTime);

	// ── 主菜单导航（手柄 / 键盘）──

	/** 给本导演挂一个玩家输入组件并绑定十字键/方向键/确认键（只做一次）。 */
	void SetupMenuInput();

	/** 菜单此刻是否可操作（绑定完成、没在过场/淡出/切图）。 */
	bool IsMenuInteractive() const;

	/** 选中项上移 / 下移（环绕）。 */
	UFUNCTION()
	void HandleMenuNavigateUp();

	UFUNCTION()
	void HandleMenuNavigateDown();

	/**
	 * 没落到 Slate 焦点上时的确认兜底：直接广播选中项自己的 OnClicked，
	 * 这样蓝图中挂在“设置/退出游戏”上的逻辑也能被手柄触发。
	 */
	UFUNCTION()
	void HandleMenuConfirm();

	/** 切换选中项：换 Focus/Normal 贴图、让 Slate 焦点跟上、必要时播一次脉冲。 */
	void SetMenuSelection(int32 Index, bool bPlayPulse);

	/** 按 HUD 贴图刷新三个按钮的选中/普通样式。 */
	void RefreshMenuSelectionArt();

	/** 开始选中项脉冲动效。 */
	void StartMenuSelectionPulse(int32 Index);

	/** Tick 驱动：同步 Slate 焦点到选中项 + 推进按钮动效。 */
	void UpdateMainMenuSelection(float DeltaTime);

	/** 十字键被按钮（Slate 焦点）自己吃掉时，把焦点同步回选中项状态。 */
	void SyncMenuSelectionFromFocus();

	/**
	 * 逐帧合成三个按钮的缩放入射与位移：选中项常驻放大 + 缓慢呼吸，切换选中时再叠一次脉冲。
	 * 只在数值真正变化时写回，避免每帧把画布布局打脏。
	 */
	void UpdateMenuButtonVisuals(float DeltaTime);
	void ClearPreviewWidgets();
	void UpdateInkEffect(float DeltaTime);
	void StartInkEffect();
	void FinishInkEffect();
	void ResetIntroCamera();
	void StartFadeToBlack();
	void ClearIntroTimers();

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BoundMainMenuWidget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> BoundNewGameButtons;

	/** 主菜单三个可选项（开始游戏 / 设置 / 退出游戏），手柄与键盘导航按这个顺序走。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> MenuButtons;

	/** 各按钮在 HUD 画布上的基准位置（动效结束后要还原）。 */
	TArray<FVector2D> MenuButtonBasePositions;

	/** 主菜单按钮的普通贴图与 Focus 贴图（选中项常驻用 Focus）。 */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MenuNormalButtonTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MenuFocusButtonTexture;

	/** 当前选中项；INDEX_NONE = 还没初始化为任何一项。 */
	int32 MenuSelectionIndex = INDEX_NONE;

	/** 正在播放脉冲的按钮；INDEX_NONE = 没有动效在跑。 */
	int32 MenuPulseIndex = INDEX_NONE;
	float MenuPulseElapsed = 0.0f;

	/** 呼吸动效累计时间（秒）。 */
	float MenuBreathElapsed = 0.0f;

	/** 已经写回按钮的缩放/位移，用来避免每帧重复写（重复写会把画布布局打脏）。 */
	TArray<float> MenuButtonAppliedScale;
	TArray<float> MenuButtonAppliedOffsetX;

	bool bMenuInputReady = false;

	FTimerHandle MainMenuBindTimer;
	FTimerHandle FadeTimer;
	FTimerHandle AutoPlayTimer;
	FTransform InitialCameraTransform;
	float HudFadeElapsed = 0.0f;
	float IntroElapsed = 0.0f;
	float InkEffectElapsed = 0.0f;
	int32 MainMenuBindAttempts = 0;
	bool bInkEffectActive = false;
	bool bInkEffectReadyToFinish = false;
	bool bMainMenuBound = false;
	bool bTravelRequested = false;
	bool bInitialCameraTransformCached = false;
	bool bIntroCameraOwnershipActive = false;
};

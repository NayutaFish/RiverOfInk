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
	void PrepareIntroCameraAtMenuStart();
	void MaintainIntroCameraOwnership();
	void SetIntroCameraOwnership(bool bOwnCamera);
	void SetMenuCinematicState(bool bCinematic);
	void RestoreMainMenuAfterFailure();
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

	FTimerHandle MainMenuBindTimer;
	FTimerHandle FadeTimer;
	FTimerHandle AutoPlayTimer;
	FTransform InitialCameraTransform;
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

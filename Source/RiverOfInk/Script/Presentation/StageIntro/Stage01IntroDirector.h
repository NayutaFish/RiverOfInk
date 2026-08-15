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
	Fading,
	Traveling,
	Failed
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

	/** Smooth handoff from the menu camera into the Stage 1 establishing shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Timing", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float CameraTransitionDuration = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Camera")
	FVector CameraPushOffset = FVector(300.0f, 0.0f, -260.0f);

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
	void StartSequenceAfterCameraTransition();

private:
	bool ResolveSceneReferences();
	bool CreateSequencePlayer();
	bool TryBindMainMenu();
	void SetMenuCinematicState(bool bCinematic);
	void RestoreMainMenuAfterFailure();
	void UpdateIntroPresentation(float SequenceSeconds);
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
	FTimerHandle CameraTransitionTimer;
	FTimerHandle FadeTimer;
	FTransform InitialCameraTransform;
	FTransform FinalCameraTransform;
	int32 MainMenuBindAttempts = 0;
	bool bMainMenuBound = false;
	bool bTravelRequested = false;
	bool bInitialCameraTransformCached = false;
};

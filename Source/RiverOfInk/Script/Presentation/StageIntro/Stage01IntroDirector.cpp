// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/StageIntro/Stage01IntroDirector.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CameraManager/CameraManager.h"
#include "CineCameraActor.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"
#include "Presentation/StageIntro/InkOverlay.h"
#include "RoguelikeSystem/RoguelikeRunFlowSubsystem.h"

AStage01IntroDirector::AStage01IntroDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Tags.Add(TEXT("Stage01IntroDirector"));
}

void AStage01IntroDirector::BeginPlay()
{
	Super::BeginPlay();

	IntroState = EStage01IntroState::Idle;
	bIntroPlaying = false;
	bTravelRequested = false;
	InkEffectElapsed = 0.0f;

	ResolveSceneReferences();
	// MainMenu already owns an authored CineCamera. Establish that camera before
	// the Level Blueprint creates the menu widget, so PIE/editor viewport state
	// can never become the first rendered frame.
	PrepareIntroCameraAtMenuStart();

	// The Level Blueprint creates WBP_MainMenu. Retry briefly so the director
	// does not depend on BeginPlay ordering between the map and the widget.
	if (GetWorld())
	{
		if (bPreviewOnly)
		{
			// MainMenu's Level Blueprint may still create WBP_MainMenu in the
			// duplicated preview map. Remove it before the first preview frame.
			ClearPreviewWidgets();
			if (bAutoPlayOnBeginPlay)
			{
				GetWorld()->GetTimerManager().SetTimer(
					AutoPlayTimer,
					this,
					&AStage01IntroDirector::HandleAutoPlayTimer,
					FMath::Max(0.0f, AutoPlayDelay),
					false);
			}
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(
				MainMenuBindTimer,
				this,
				&AStage01IntroDirector::HandleMenuBindTimer,
				0.10f,
				true,
				0.10f);
		}
	}
}

void AStage01IntroDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Sequencer may have possessed the camera when PIE is torn down. Remove
	// the delegate before stopping, then restore the authored K0 transform so
	// a later PIE starts from the same shot instead of an accumulated pose.
	if (IsValid(SequencePlayer))
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &AStage01IntroDirector::OnSequenceFinished);
		SequencePlayer->Stop();
	}

	ClearIntroTimers();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MainMenuBindTimer);
		GetWorld()->GetTimerManager().ClearTimer(AutoPlayTimer);
	}
	SetIntroCameraOwnership(false);
	ResetIntroCamera();
	SequencePlayer = nullptr;
	SequenceActor = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AStage01IntroDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	MaintainIntroCameraOwnership();

	if (!bIntroPlaying)
	{
		return;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (IntroState == EStage01IntroState::Playing)
	{
		IntroElapsed += SafeDeltaTime;
		if (!bInkEffectActive && !bInkEffectReadyToFinish && IntroElapsed >= InkEffectStartTime)
		{
			StartInkEffect();
		}
	}

	if (bInkEffectActive)
	{
		UpdateInkEffect(SafeDeltaTime);
	}
}
bool AStage01IntroDirector::ResolveSceneReferences()
{
	if (!IsValid(IntroSequence))
	{
		IntroSequence = LoadObject<ULevelSequence>(
			nullptr,
			TEXT("/Game/Presentation/StageIntro/LS_Stage01Intro.LS_Stage01Intro"));
	}

	if (!IsValid(IntroCamera))
	{
		TArray<AActor*> Cameras;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACineCameraActor::StaticClass(), Cameras);
		for (AActor* Candidate : Cameras)
		{
			if (IsValid(Candidate) && Candidate->ActorHasTag(TEXT("Stage01IntroCamera")))
			{
				IntroCamera = Cast<ACineCameraActor>(Candidate);
				break;
			}
		}
	}

	if (!IsValid(InkOverlay))
	{
		TArray<AActor*> Overlays;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AInkOverlay::StaticClass(), Overlays);
		for (AActor* Candidate : Overlays)
		{
			if (IsValid(Candidate) && Candidate->ActorHasTag(TEXT("Stage01InkOverlay")))
			{
				InkOverlay = Cast<AInkOverlay>(Candidate);
				break;
			}
		}
		if (!IsValid(InkOverlay) && Overlays.Num() > 0)
		{
			InkOverlay = Cast<AInkOverlay>(Overlays[0]);
		}
	}

	if (IsValid(IntroCamera))
	{
		if (!bInitialCameraTransformCached)
		{
			InitialCameraTransform = IntroCamera->GetActorTransform();
			bInitialCameraTransformCached = true;
		}
	}

	const bool bValid = IsValid(IntroSequence) && IsValid(IntroCamera) && IsValid(InkOverlay);
	if (!bValid)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Stage01 IntroDirector references invalid. Sequence=%s Camera=%s Overlay=%s."),
			IsValid(IntroSequence) ? TEXT("valid") : TEXT("null"),
			IsValid(IntroCamera) ? TEXT("valid") : TEXT("null"),
			IsValid(InkOverlay) ? TEXT("valid") : TEXT("null"));
	}
	return bValid;
}

void AStage01IntroDirector::PrepareIntroCameraAtMenuStart()
{
	if (!IsValid(IntroCamera))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 intro cannot prepare the menu camera: IntroCamera is null."));
		return;
	}

	SetIntroCameraOwnership(true);
	ResetIntroCamera();

	// Do not create a paused LevelSequencePlayer here. A camera cut player
	// captures the current view as pre-animated state; stopping that player at
	// the first click can restore the PIE/editor view and make it look like the
	// intro path starts from the editor camera. The menu only needs the authored
	// CineCamera at K0. The sequence player is created once, after that camera
	// has become the active runtime view target, when PlayIntro() is requested.
	MaintainIntroCameraOwnership();
	UE_LOG(LogTemp, Log, TEXT("Stage01 menu camera prepared on authored CineCamera K0: %s."), *IntroCamera->GetName());
}

void AStage01IntroDirector::MaintainIntroCameraOwnership()
{
	if (!bIntroCameraOwnershipActive || !GetWorld())
	{
		return;
	}

	// A legacy follow-camera actor can still exist when the global GameMode is
	// used in PIE. It must not reclaim the PlayerController during the menu or
	// while Sequencer is moving the authored CineCamera.
	for (TActorIterator<ACameraManager> It(GetWorld()); It; ++It)
	{
		It->SetActorTickEnabled(false);
	}

	if (IsValid(IntroCamera))
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->GetViewTarget() != IntroCamera)
			{
				PC->SetViewTarget(IntroCamera);
			}
		}
	}
}

void AStage01IntroDirector::SetIntroCameraOwnership(bool bOwnCamera)
{
	bIntroCameraOwnershipActive = bOwnCamera;

	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ACameraManager> It(GetWorld()); It; ++It)
	{
		It->SetActorTickEnabled(!bOwnCamera);
	}
}

bool AStage01IntroDirector::CreateSequencePlayer()
{
	if (!IsValid(IntroSequence) || !GetWorld())
	{
		return false;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false;
	// Keep the authored K3 pose after the sequence finishes. The following ink
	// effect is played while the camera is held on that exact final keyframe.
	PlaybackSettings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceKeepState;

	if (IsValid(SequencePlayer))
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &AStage01IntroDirector::OnSequenceFinished);
		SequencePlayer->Stop();
	}

	ALevelSequenceActor* CreatedSequenceActor = nullptr;
	SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		GetWorld(),
		IntroSequence,
		PlaybackSettings,
		CreatedSequenceActor);
	SequenceActor = CreatedSequenceActor;

	if (!IsValid(SequencePlayer))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 IntroDirector failed to create LevelSequencePlayer."));
		return false;
	}

	SequencePlayer->OnFinished.AddDynamic(this, &AStage01IntroDirector::OnSequenceFinished);
	return true;
}

bool AStage01IntroDirector::PlayIntro()
{
	if (bIntroPlaying || IntroState == EStage01IntroState::Playing
		|| IntroState == EStage01IntroState::Fading
		|| IntroState == EStage01IntroState::Traveling)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage01 IntroDirector rejected duplicate PlayIntro()."));
		return false;
	}

	if (!ResolveSceneReferences())
	{
		AbortIntro();
		return false;
	}

	bIntroPlaying = true;
	bTravelRequested = false;
	IntroElapsed = 0.0f;
	InkEffectElapsed = 0.0f;
	bInkEffectActive = false;
	bInkEffectReadyToFinish = false;
	IntroState = EStage01IntroState::Playing;
	ClearIntroTimers();

	if (bPreviewOnly)
	{
		ClearPreviewWidgets();
	}

	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(0.0f);
	}
	SetIntroCameraOwnership(IsValid(IntroCamera));
	// Stop every menu camera owner before selecting the authored intro camera.
	// This must happen before the cut, otherwise the current PlayerController
	// view (which can be seeded by the PIE/editor viewport) becomes an implicit
	// first keyframe.
	SetMenuCinematicState(true);
	if (IsValid(IntroCamera))
	{
		ResetIntroCamera();
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			// A hard cut establishes the authored K0. Do not blend from the
			// current view target or the editor/PIE viewport camera.
			PC->SetViewTarget(IntroCamera);
		}
	}

	if (bPreviewOnly && bPreviewUseStaticCamera)
	{
		// The isolated material check intentionally holds its straight-on camera.
		// The production camera track remains untouched for the MainMenu intro.
		UE_LOG(LogTemp, Log, TEXT("Stage01 static ink preview started."));
		return true;
	}
	// Create the player only after the authored camera owns the PlayerController.
	// This prevents Sequencer's pre-animated camera state from ever being seeded
	// by the editor/PIE viewport.
	if (!CreateSequencePlayer())
	{
		AbortIntro();
		return false;
	}

	StartSequenceAtAuthoredStart();
	return true;
}

void AStage01IntroDirector::StartSequenceAtAuthoredStart()
{
	if (!bIntroPlaying || IntroState != EStage01IntroState::Playing || !IsValid(SequencePlayer))
	{
		return;
	}

	// Evaluate frame zero explicitly before playback. This makes the first
	// rendered frame the sequence's authored K0 rather than the previous view
	// target's transform.
	SequencePlayer->SetPlaybackPosition(
		FMovieSceneSequencePlaybackParams(0.0f, EUpdatePositionMethod::Jump));
	SequencePlayer->Play();
	UE_LOG(LogTemp, Log, TEXT("Stage01 intro started at authored K0. Duration=%.2fs."), IntroDuration);
}

void AStage01IntroDirector::OnSequenceFinished()
{
	HandleSequenceFinished();
}

void AStage01IntroDirector::HandleSequenceFinished()
{
	if (!bIntroPlaying || IntroState != EStage01IntroState::Playing)
	{
		return;
	}

	// The camera reaches K3 at the same moment the 0.8s + 3.5s ink beat is
	// normally ready. If a designer lengthens the effect, hold the K3 camera
	// while the remaining wet ink completes instead of restarting the effect.
	IntroState = EStage01IntroState::EffectPlaying;
	if (bInkEffectReadyToFinish)
	{
		FinishInkEffect();
	}
	else if (!bInkEffectActive)
	{
		StartInkEffect();
	}
}
void AStage01IntroDirector::StartInkEffect()
{
	if (!bIntroPlaying || bInkEffectActive || bInkEffectReadyToFinish
		|| (IntroState != EStage01IntroState::Playing && IntroState != EStage01IntroState::EffectPlaying))
	{
		return;
	}

	InkEffectElapsed = 0.0f;
	bInkEffectActive = true;
	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(0.0f);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Stage01 starting layered ink at %.2fs. Duration=%.2fs."),
		IntroElapsed,
		FMath::Max(0.0f, InkEffectDuration));

	if (InkEffectDuration <= KINDA_SMALL_NUMBER)
	{
		bInkEffectActive = false;
		bInkEffectReadyToFinish = true;
		if (bPreviewOnly && bPreviewUseStaticCamera && IntroState == EStage01IntroState::Playing)
		{
			IntroState = EStage01IntroState::EffectPlaying;
			FinishInkEffect();
		}
		else if (IntroState == EStage01IntroState::EffectPlaying)
		{
			FinishInkEffect();
		}
	}
}
void AStage01IntroDirector::UpdateInkEffect(float DeltaTime)
{
	if (!bIntroPlaying || !bInkEffectActive)
	{
		return;
	}

	const float Duration = FMath::Max(KINDA_SMALL_NUMBER, InkEffectDuration);
	InkEffectElapsed = FMath::Min(InkEffectElapsed + FMath::Max(0.0f, DeltaTime), Duration);
	const float Progress = FMath::Clamp(InkEffectElapsed / Duration, 0.0f, 1.0f);

	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(Progress);
	}

	if (InkEffectElapsed >= Duration)
	{
		bInkEffectActive = false;
		bInkEffectReadyToFinish = true;
		if (bPreviewOnly && bPreviewUseStaticCamera && IntroState == EStage01IntroState::Playing)
		{
			IntroState = EStage01IntroState::EffectPlaying;
			FinishInkEffect();
		}
		else if (IntroState == EStage01IntroState::EffectPlaying)
		{
			FinishInkEffect();
		}
	}
}
void AStage01IntroDirector::FinishInkEffect()
{
	if (!bIntroPlaying || IntroState != EStage01IntroState::EffectPlaying)
	{
		return;
	}

	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(1.0f);
	}

	if (bPreviewOnly)
	{
		// The isolated preview never travels. When looping is enabled, a very
		// short full-ink hold is followed by a clean-paper restart.
		IntroState = EStage01IntroState::PreviewComplete;
		bIntroPlaying = false;
		InkEffectElapsed = FMath::Max(0.0f, InkEffectDuration);
		ClearIntroTimers();

		if (bPreviewLoop && bAutoPlayOnBeginPlay && GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				AutoPlayTimer,
				this,
				&AStage01IntroDirector::HandleAutoPlayTimer,
				FMath::Max(0.0f, PreviewLoopDelay),
				false);
			UE_LOG(LogTemp, Log, TEXT("Stage01 ink preview full coverage; restarting after %.2fs."), PreviewLoopDelay);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Stage01 ink preview complete; holding full coverage without travel."));
		}
		return;
	}
	IntroState = EStage01IntroState::Fading;
	StartFadeToBlack();
	UE_LOG(LogTemp, Log, TEXT("Stage01 ink effect finished; beginning fade before RunFlow travel."));
}

void AStage01IntroDirector::StartFadeToBlack()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!IsValid(PC) || !IsValid(PC->PlayerCameraManager))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 intro cannot fade: PlayerCameraManager is unavailable."));
		AbortIntro();
		return;
	}

	PC->PlayerCameraManager->StartCameraFade(
		0.0f,
		1.0f,
		FadeDuration,
		FLinearColor::Black,
		false,
		true);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			FadeTimer,
			this,
			&AStage01IntroDirector::HandleFadeTimer,
			FadeDuration,
			false);
	}
}

bool AStage01IntroDirector::BeginTravelAfterFade()
{
	if (bTravelRequested || IntroState != EStage01IntroState::Fading)
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	URoguelikeRunFlowSubsystem* RunFlow = GameInstance
		? GameInstance->GetSubsystem<URoguelikeRunFlowSubsystem>()
		: nullptr;
	if (!IsValid(RunFlow))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 intro cannot travel: RunFlow subsystem is unavailable."));
		AbortIntro();
		return false;
	}

	IntroState = EStage01IntroState::Traveling;
	bTravelRequested = true;
	const bool bStarted = RunFlow->StartNewRun();
	if (!bStarted)
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 intro RunFlow.StartNewRun() returned false; restoring MainMenu."));
		bTravelRequested = false;
		AbortIntro();
		return false;
	}

	const FRoguelikeRoomDefinition FirstRoom = RunFlow->GetCurrentRoomDefinition();
	UE_LOG(LogTemp, Log,
		TEXT("Stage01 intro travel requested only after fade. FirstRoom=%s Map=%s."),
		*FirstRoom.RoomId.ToString(),
		*FirstRoom.RoomMap.ToSoftObjectPath().ToString());
	return true;
}

void AStage01IntroDirector::AbortIntro()
{
	if (IsValid(SequencePlayer))
	{
		SequencePlayer->Stop();
		SequencePlayer->OnFinished.RemoveDynamic(this, &AStage01IntroDirector::OnSequenceFinished);
	}

	ClearIntroTimers();
	ResetIntroCamera();
	bIntroPlaying = false;
	bTravelRequested = false;
	IntroElapsed = 0.0f;
	InkEffectElapsed = 0.0f;
	bInkEffectActive = false;
	bInkEffectReadyToFinish = false;
	IntroState = EStage01IntroState::Failed;

	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(0.0f);
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (IsValid(PC->PlayerCameraManager))
		{
			PC->PlayerCameraManager->StartCameraFade(
				1.0f,
				0.0f,
				FMath::Min(FadeDuration, 0.25f),
				FLinearColor::Black,
				false,
				true);
		}
	}

	RestoreMainMenuAfterFailure();
	UE_LOG(LogTemp, Error, TEXT("Stage01 intro aborted; MainMenu input was restored."));
}

bool AStage01IntroDirector::TryBindMainMenu()
{
	if (bMainMenuBound || !GetWorld())
	{
		return bMainMenuBound;
	}

	++MainMenuBindAttempts;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		GetWorld(),
		Widgets,
		UUserWidget::StaticClass(),
		false);

	for (UUserWidget* Candidate : Widgets)
	{
		if (!IsValid(Candidate))
		{
			continue;
		}

		const FString ClassName = Candidate->GetClass()->GetName();
		if (!ClassName.Contains(TEXT("WBP_MainMenu")) && !Candidate->GetName().Contains(TEXT("MainMenu")))
		{
			continue;
		}

		UButton* FallbackButton = nullptr;
		Candidate->WidgetTree->ForEachWidget([this, &FallbackButton](UWidget* Widget)
		{
			UButton* Button = Cast<UButton>(Widget);
			if (!IsValid(Button))
			{
				return;
			}

			if (!IsValid(FallbackButton))
			{
				FallbackButton = Button;
			}

			const FString ButtonName = Button->GetName();
			const bool bLooksLikeNewGame =
				(ButtonName.Contains(TEXT("New"), ESearchCase::IgnoreCase)
					|| ButtonName.Contains(TEXT("Start"), ESearchCase::IgnoreCase)
					|| ButtonName.Contains(TEXT("Game"), ESearchCase::IgnoreCase)
					|| ButtonName.Equals(TEXT("Button_41"), ESearchCase::IgnoreCase))
				&& !ButtonName.Contains(TEXT("Quit"), ESearchCase::IgnoreCase)
				&& !ButtonName.Contains(TEXT("Exit"), ESearchCase::IgnoreCase)
				&& !ButtonName.Contains(TEXT("Setting"), ESearchCase::IgnoreCase);

			if (bLooksLikeNewGame)
			{
				Button->OnClicked.Clear();
				Button->OnClicked.AddDynamic(this, &AStage01IntroDirector::OnNewGameClicked);
				BoundNewGameButtons.Add(Button);
			}
		});

		if (BoundNewGameButtons.IsEmpty() && IsValid(FallbackButton))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Stage01 intro could not identify New Game by name; using first MainMenu button '%s' as fallback."),
				*FallbackButton->GetName());
			FallbackButton->OnClicked.Clear();
			FallbackButton->OnClicked.AddDynamic(this, &AStage01IntroDirector::OnNewGameClicked);
			BoundNewGameButtons.Add(FallbackButton);
		}

		if (!BoundNewGameButtons.IsEmpty())
		{
			BoundMainMenuWidget = Candidate;
			bMainMenuBound = true;
			GetWorld()->GetTimerManager().ClearTimer(MainMenuBindTimer);
				// The menu widget is created by the Level Blueprint. Explicitly
			// establish the UI input mode after binding so the first mouse click
			// is not spent only giving Slate focus to the viewport.
			SetMenuCinematicState(false);
			UE_LOG(LogTemp, Log,
				TEXT("Stage01 intro bound MainMenu widget '%s' to %d New Game button(s)."),
				*Candidate->GetName(), BoundNewGameButtons.Num());
			return true;
		}
	}

	if (MainMenuBindAttempts >= 30)
	{
		GetWorld()->GetTimerManager().ClearTimer(MainMenuBindTimer);
		UE_LOG(LogTemp, Error, TEXT("Stage01 intro failed to find WBP_MainMenu after %d attempts."), MainMenuBindAttempts);
	}
	return false;
}

void AStage01IntroDirector::OnNewGameClicked()
{
	PlayIntro();
}

void AStage01IntroDirector::HandleMenuBindTimer()
{
	TryBindMainMenu();
}

void AStage01IntroDirector::HandleFadeTimer()
{
	BeginTravelAfterFade();
}

void AStage01IntroDirector::HandleAutoPlayTimer()
{
	if (!bPreviewOnly || !bAutoPlayOnBeginPlay)
	{
		return;
	}

	ClearPreviewWidgets();
	PlayIntro();
}

void AStage01IntroDirector::SetMenuCinematicState(bool bCinematic)
{
	if (IsValid(BoundMainMenuWidget))
	{
		BoundMainMenuWidget->SetVisibility(bCinematic ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		BoundMainMenuWidget->SetIsEnabled(!bCinematic);
	}

	for (UButton* Button : BoundNewGameButtons)
	{
		if (IsValid(Button))
		{
			Button->SetIsEnabled(!bCinematic);
		}
	}

	// The menu game mode may own a follow camera actor that normally claims the
	// PlayerController every tick. Keep it disabled for the lifetime of the
	// authored intro camera, including the idle menu before the click.
	if (GetWorld())
	{
		for (TActorIterator<ACameraManager> It(GetWorld()); It; ++It)
		{
			It->SetActorTickEnabled(!bCinematic && !bIntroCameraOwnershipActive);
		}
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!IsValid(PC))
	{
		return;
	}

	if (bCinematic)
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		PC->bShowMouseCursor = false;
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
		FInputModeGameOnly InputMode;
		// Switching from UIOnly to GameOnly must not consume the first mouse
		// button press as a viewport recapture. The intro owns the input lock;
		// the target gameplay map will apply the same policy after travel.
		InputMode.SetConsumeCaptureMouseDown(false);
		PC->SetInputMode(InputMode);
	}
	else
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		// Keep the cursor outside viewport capture until the first New Game click
		// reaches the Slate button. No gameplay pawn exists on this map, so the
		// GameAndUI fallback is harmless while it keeps mouse routing reliable.
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
}

void AStage01IntroDirector::RestoreMainMenuAfterFailure()
{
	SetMenuCinematicState(false);
}

void AStage01IntroDirector::ClearPreviewWidgets()
{
	if (!bPreviewOnly || !GetWorld())
	{
		return;
	}

	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		GetWorld(),
		Widgets,
		UUserWidget::StaticClass(),
		false);

	for (UUserWidget* Widget : Widgets)
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromParent();
		}
	}
}

void AStage01IntroDirector::ResetPreview()
{
	if (!bPreviewOnly)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage01 ResetPreview() ignored because bPreviewOnly is false."));
		return;
	}

	if (IsValid(SequencePlayer))
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &AStage01IntroDirector::OnSequenceFinished);
		SequencePlayer->Stop();
	}

	ClearIntroTimers();
	ClearPreviewWidgets();
	ResetIntroCamera();
	SetIntroCameraOwnership(true);
	bIntroPlaying = false;
	bTravelRequested = false;
	IntroElapsed = 0.0f;
	InkEffectElapsed = 0.0f;
	bInkEffectActive = false;
	bInkEffectReadyToFinish = false;
	IntroState = EStage01IntroState::Idle;

	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(0.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("Stage01 ink preview reset."));
}

void AStage01IntroDirector::ResetIntroCamera()
{
	if (IsValid(IntroCamera) && bInitialCameraTransformCached)
	{
		IntroCamera->SetActorTransform(InitialCameraTransform);
	}
}

void AStage01IntroDirector::ClearIntroTimers()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FadeTimer);
		GetWorld()->GetTimerManager().ClearTimer(AutoPlayTimer);
	}
}

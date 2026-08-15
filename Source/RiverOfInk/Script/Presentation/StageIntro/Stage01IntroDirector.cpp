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

	ResolveSceneReferences();

	// The Level Blueprint creates WBP_MainMenu. Retry briefly so the director
	// does not depend on BeginPlay ordering between the map and the widget.
	if (GetWorld())
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

void AStage01IntroDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Sequencer may have possessed the camera when PIE is torn down. Remove
	// the delegate before stopping, then restore the editor-authored transform
	// so a later PIE starts from the same shot instead of an accumulated pose.
	if (IsValid(SequencePlayer))
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &AStage01IntroDirector::OnSequenceFinished);
		SequencePlayer->Stop();
	}

	ClearIntroTimers();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(MainMenuBindTimer);
	}
	ResetIntroCamera();
	SequencePlayer = nullptr;
	SequenceActor = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AStage01IntroDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIntroPlaying || IntroState != EStage01IntroState::Playing)
	{
		return;
	}

	float SequenceSeconds = 0.0f;
	if (IsValid(SequencePlayer))
	{
		SequenceSeconds = static_cast<float>(SequencePlayer->GetCurrentTime().AsSeconds());
	}

	UpdateIntroPresentation(SequenceSeconds);
	(void)DeltaTime;
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
			FinalCameraTransform = InitialCameraTransform;
			FinalCameraTransform.AddToTranslation(CameraPushOffset);
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

bool AStage01IntroDirector::CreateSequencePlayer()
{
	if (!IsValid(IntroSequence) || !GetWorld())
	{
		return false;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false;
	PlaybackSettings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;

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

	if (!ResolveSceneReferences() || !CreateSequencePlayer())
	{
		AbortIntro();
		return false;
	}

	bIntroPlaying = true;
	bTravelRequested = false;
	IntroState = EStage01IntroState::Playing;
	ClearIntroTimers();

	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(0.0f);
	}
	if (IsValid(IntroCamera))
	{
		ResetIntroCamera();
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetViewTargetWithBlend(IntroCamera, 0.15f);
		}
	}

	SetMenuCinematicState(true);
	SequencePlayer->Play();
	UE_LOG(LogTemp, Log, TEXT("Stage01 intro started. Duration=%.2fs."), IntroDuration);
	return true;
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

	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(1.0f);
	}
	if (IsValid(IntroCamera) && bInitialCameraTransformCached)
	{
		IntroCamera->SetActorTransform(FinalCameraTransform);
	}
	IntroState = EStage01IntroState::Fading;
	StartFadeToBlack();
	UE_LOG(LogTemp, Log, TEXT("Stage01 intro sequence finished; beginning fade before RunFlow travel."));
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

	// The menu game mode owns a follow camera actor that normally claims the
	// PlayerController every tick. Pause that one owner while the explicit
	// intro CineCamera is active, otherwise the camera cut is overwritten.
	if (GetWorld())
	{
		for (TActorIterator<ACameraManager> It(GetWorld()); It; ++It)
		{
			It->SetActorTickEnabled(!bCinematic);
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

void AStage01IntroDirector::UpdateIntroPresentation(float SequenceSeconds)
{
	// Match the review storyboard: hold the clean establishing shot for
	// 0.0-0.8s, reveal the first ink mark by 1.5s, then let the stain take
	// over as the camera pushes through 2.5-4.3s.
	float Progress = 0.0f;
	if (SequenceSeconds >= 0.8f && SequenceSeconds < 1.5f)
	{
		Progress = FMath::GetMappedRangeValueClamped(
			FVector2D(0.8f, 1.5f), FVector2D(0.0f, 0.2f), SequenceSeconds);
	}
	else if (SequenceSeconds >= 1.5f && SequenceSeconds < 2.5f)
	{
		Progress = FMath::GetMappedRangeValueClamped(
			FVector2D(1.5f, 2.5f), FVector2D(0.2f, 0.55f), SequenceSeconds);
	}
	else if (SequenceSeconds >= 2.5f && SequenceSeconds < 3.8f)
	{
		Progress = FMath::GetMappedRangeValueClamped(
			FVector2D(2.5f, 3.8f), FVector2D(0.55f, 0.88f), SequenceSeconds);
	}
	else if (SequenceSeconds >= 3.8f)
	{
		Progress = FMath::GetMappedRangeValueClamped(
			FVector2D(3.8f, FMath::Max(3.8f, IntroDuration)), FVector2D(0.88f, 1.0f), SequenceSeconds);
	}
	if (IsValid(InkOverlay))
	{
		InkOverlay->SetInkProgress(Progress);
	}
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
	}
}

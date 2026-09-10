// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/StageIntro/Stage01IntroDirector.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CameraManager/CameraManager.h"
#include "CineCameraActor.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
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
#include "Styling/SlateBrush.h"

namespace
{
	static const TCHAR* MainMenuTitleTexturePath =
		TEXT("/Game/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Title_MoranKaifeng.T_UI_MainMenuHUD_Title_MoranKaifeng");
	static const TCHAR* MainMenuButtonTexturePath =
		TEXT("/Game/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Button_Normal.T_UI_MainMenuHUD_Button_Normal");
	static const TCHAR* MainMenuFocusTexturePath =
		TEXT("/Game/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Button_Focus.T_UI_MainMenuHUD_Button_Focus");
	static const TCHAR* MainMenuInkPanelTexturePath =
		TEXT("/Game/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_InkPanel.T_UI_MainMenuHUD_InkPanel");
	static const TCHAR* MainMenuFontPath =
		TEXT("/Game/RawContent/UI/Fonts/AaGuDianKeBenSongYouMoBan_2_Font.AaGuDianKeBenSongYouMoBan_2_Font");

	UTextBlock* FindTextBlockRecursive(UWidget* Root)
	{
		if (!IsValid(Root))
		{
			return nullptr;
		}

		if (UTextBlock* TextBlock = Cast<UTextBlock>(Root))
		{
			return TextBlock;
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				if (UTextBlock* TextBlock = FindTextBlockRecursive(Panel->GetChildAt(ChildIndex)))
				{
					return TextBlock;
				}
			}
		}

		if (UContentWidget* Content = Cast<UContentWidget>(Root))
		{
			return FindTextBlockRecursive(Content->GetContent());
		}

		return nullptr;
	}

	UCanvasPanel* FindCanvasPanelRecursive(UWidget* Root)
	{
		if (!IsValid(Root))
		{
			return nullptr;
		}

		if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Root))
		{
			return Canvas;
		}

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				if (UCanvasPanel* Canvas = FindCanvasPanelRecursive(Panel->GetChildAt(ChildIndex)))
				{
					return Canvas;
				}
			}
		}

		if (UContentWidget* Content = Cast<UContentWidget>(Root))
		{
			return FindCanvasPanelRecursive(Content->GetContent());
		}

		return nullptr;
	}

	UButton* FindButtonByName(const TArray<UButton*>& Buttons, const TCHAR* ButtonName)
	{
		for (UButton* Button : Buttons)
		{
			if (IsValid(Button) && Button->GetName().Equals(ButtonName, ESearchCase::IgnoreCase))
			{
				return Button;
			}
		}
		return nullptr;
	}

	UButton* FindButtonByLabel(const TArray<UButton*>& Buttons, const TCHAR* Label)
	{
		for (UButton* Button : Buttons)
		{
			if (UTextBlock* TextBlock = FindTextBlockRecursive(Button))
			{
				if (TextBlock->GetText().ToString().Equals(Label, ESearchCase::CaseSensitive))
				{
					return Button;
				}
			}
		}
		return nullptr;
	}

	FSlateBrush MakeMainMenuBrush(UTexture2D* Texture, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		if (!IsValid(Texture))
		{
			return Brush;
		}

		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}
}

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
	HudFadeElapsed = 0.0f;
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
		|| IntroState == EStage01IntroState::HudFading
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

void AStage01IntroDirector::ApplyMainMenuHudArt(UUserWidget* MenuWidget)
{
	if (!IsValid(MenuWidget) || !MenuWidget->WidgetTree)
	{
		return;
	}

	UTexture2D* TitleTexture = LoadObject<UTexture2D>(nullptr, MainMenuTitleTexturePath);
	UTexture2D* NormalButtonTexture = LoadObject<UTexture2D>(nullptr, MainMenuButtonTexturePath);
	UTexture2D* FocusButtonTexture = LoadObject<UTexture2D>(nullptr, MainMenuFocusTexturePath);
	UTexture2D* InkPanelTexture = LoadObject<UTexture2D>(nullptr, MainMenuInkPanelTexturePath);
	UFont* MainMenuFont = LoadObject<UFont>(nullptr, MainMenuFontPath);

	if (!IsValid(TitleTexture))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 main menu HUD title texture could not be loaded: %s."), MainMenuTitleTexturePath);
	}
	if (!IsValid(NormalButtonTexture))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 main menu HUD normal button texture could not be loaded: %s."), MainMenuButtonTexturePath);
	}
	if (!IsValid(FocusButtonTexture))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Stage01 main menu HUD focus texture could not be loaded; reusing the normal button texture."));
		FocusButtonTexture = NormalButtonTexture;
	}
	if (!IsValid(MainMenuFont))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 main menu HUD font could not be loaded: %s."), MainMenuFontPath);
	}
	if (!IsValid(InkPanelTexture))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Stage01 main menu HUD ink panel texture could not be loaded: %s; continuing without the optional panel."),
			MainMenuInkPanelTexturePath);
	}

	TArray<UButton*> Buttons;
	MenuWidget->WidgetTree->ForEachWidget([&Buttons](UWidget* Widget)
	{
		if (UButton* Button = Cast<UButton>(Widget))
		{
			Buttons.Add(Button);
		}
	});

	if (Buttons.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage01 main menu HUD art found no buttons in widget '%s'."), *MenuWidget->GetName());
		return;
	}

	// Prefer the actual labels, then fall back to the stable names in the
	// authored WBP. This keeps the art pass independent from Blueprint layout
	// coordinates while preserving the existing button events.
	UButton* StartButton = FindButtonByLabel(Buttons, TEXT("开始游戏"));
	UButton* SettingsButton = FindButtonByLabel(Buttons, TEXT("设置"));
	UButton* QuitButton = FindButtonByLabel(Buttons, TEXT("退出游戏"));
	if (!IsValid(StartButton))
	{
		StartButton = FindButtonByName(Buttons, TEXT("Button_41"));
	}
	if (!IsValid(SettingsButton))
	{
		SettingsButton = FindButtonByName(Buttons, TEXT("Button_89"));
	}
	if (!IsValid(QuitButton))
	{
		QuitButton = FindButtonByName(Buttons, TEXT("Button_168"));
	}
	if (!IsValid(StartButton) && !BoundNewGameButtons.IsEmpty())
	{
		StartButton = BoundNewGameButtons[0].Get();
	}

	TArray<UButton*> OrderedButtons;
	OrderedButtons.Reserve(3);
	if (IsValid(StartButton))
	{
		OrderedButtons.AddUnique(StartButton);
	}
	if (IsValid(SettingsButton))
	{
		OrderedButtons.AddUnique(SettingsButton);
	}
	if (IsValid(QuitButton))
	{
		OrderedButtons.AddUnique(QuitButton);
	}
	for (UButton* Button : Buttons)
	{
		if (OrderedButtons.Num() >= 3)
		{
			break;
		}
		OrderedButtons.AddUnique(Button);
	}

	UCanvasPanel* RootCanvas = FindCanvasPanelRecursive(MenuWidget->WidgetTree->RootWidget);
	if (!IsValid(RootCanvas))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Stage01 main menu HUD art could not find a CanvasPanel in widget '%s'; keeping authored slots."),
			*MenuWidget->GetName());
	}

	if (IsValid(RootCanvas) && IsValid(InkPanelTexture))
	{
		UImage* InkPanelImage = Cast<UImage>(
			MenuWidget->WidgetTree->FindWidget(FName(TEXT("MainMenuInkPanel"))));
		if (!IsValid(InkPanelImage))
		{
			InkPanelImage = MenuWidget->WidgetTree->ConstructWidget<UImage>(
				UImage::StaticClass(),
				FName(TEXT("MainMenuInkPanel")));
		}

		if (IsValid(InkPanelImage))
		{
			InkPanelImage->RemoveFromParent();
			InkPanelImage->SetBrushFromTexture(InkPanelTexture, false);
			InkPanelImage->SetColorAndOpacity(FLinearColor::White);
			InkPanelImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			InkPanelImage->SetRenderScale(FVector2D(-1.0f, 1.0f));
			InkPanelImage->SetVisibility(ESlateVisibility::HitTestInvisible);

			if (UCanvasPanelSlot* InkPanelSlot = RootCanvas->AddChildToCanvas(InkPanelImage))
			{
				InkPanelSlot->SetAnchors(FAnchors(0.0f, 0.5f));
				InkPanelSlot->SetAlignment(FVector2D(0.0f, 0.5f));
				InkPanelSlot->SetPosition(FVector2D(0.0f, 0.0f));
				InkPanelSlot->SetSize(FVector2D(520.0f, 924.0f));
				InkPanelSlot->SetZOrder(5);
			}
		}
	}

	if (IsValid(RootCanvas) && IsValid(TitleTexture))
	{
		UImage* TitleImage = Cast<UImage>(
			MenuWidget->WidgetTree->FindWidget(FName(TEXT("MainMenuTitleArt"))));
		if (!IsValid(TitleImage))
		{
			TitleImage = MenuWidget->WidgetTree->ConstructWidget<UImage>(
				UImage::StaticClass(),
				FName(TEXT("MainMenuTitleArt")));
		}

		if (IsValid(TitleImage))
		{
			TitleImage->RemoveFromParent();
			TitleImage->SetBrushFromTexture(TitleTexture, false);
			TitleImage->SetColorAndOpacity(FLinearColor::White);
			TitleImage->SetVisibility(ESlateVisibility::HitTestInvisible);

			if (UCanvasPanelSlot* TitleSlot = RootCanvas->AddChildToCanvas(TitleImage))
			{
				TitleSlot->SetAnchors(FAnchors(0.0f, 0.5f));
				TitleSlot->SetAlignment(FVector2D(0.0f, 0.5f));
				TitleSlot->SetPosition(FVector2D(64.0f, -190.0f));
				TitleSlot->SetSize(FVector2D(500.0f, 200.0f));
				TitleSlot->SetZOrder(10);
			}
		}
	}

	const TCHAR* ButtonLabels[] = { TEXT("开始游戏"), TEXT("设置"), TEXT("退出游戏") };
	const FVector2D ButtonPositions[] =
	{
		FVector2D(84.0f, -24.0f),
		FVector2D(84.0f, 71.0f),
		FVector2D(84.0f, 166.0f)
	};
	// Keep the 32px HUD text unchanged while giving the brush image a slightly
	// larger frame and a little more breathing room around the label.
	const FVector2D ButtonLayoutSize(480.0f, 95.0f);
	const FLinearColor InkTextColor(0.10f, 0.08f, 0.06f, 1.0f);
	const FSlateBrush NormalBrush = MakeMainMenuBrush(
		NormalButtonTexture,
		FLinearColor::White);
	const FSlateBrush FocusBrush = MakeMainMenuBrush(
		FocusButtonTexture,
		FLinearColor::White);
	FSlateBrush HoverBrush = FocusBrush;
	HoverBrush.TintColor = FSlateColor(FLinearColor(1.0f, 0.88f, 0.78f, 1.0f));

	// The authored menu keeps the buttons in a non-Canvas panel. Move the
	// existing button instances into the HUD canvas so the acceptance layout
	// can be applied without replacing their click bindings or changing the
	// scene/sequence layout.
	if (IsValid(RootCanvas))
	{
		for (UButton* Button : OrderedButtons)
		{
			if (!IsValid(Button) || Button->GetParent() == RootCanvas)
			{
				continue;
			}

			Button->RemoveFromParent();
			RootCanvas->AddChildToCanvas(Button);
		}
	}

	for (int32 ButtonIndex = 0; ButtonIndex < OrderedButtons.Num() && ButtonIndex < 3; ++ButtonIndex)
	{
		UButton* Button = OrderedButtons[ButtonIndex];
		if (!IsValid(Button))
		{
			continue;
		}

		FButtonStyle Style = Button->GetStyle();
		// Normal must stay neutral even for Start Game; otherwise it looks
		// hovered while the pointer is elsewhere. Focus is reserved for actual
		// hover/press feedback.
		Style.Normal = NormalBrush;
		Style.Hovered = HoverBrush;
		Style.Pressed = FocusBrush;
		Style.Disabled = NormalBrush;
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);
		Button->SetStyle(Style);

		if (UTextBlock* Label = FindTextBlockRecursive(Button))
		{
			if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->GetContentSlot()))
			{
				ContentSlot->SetPadding(FMargin(0.0f));
				ContentSlot->SetHorizontalAlignment(HAlign_Center);
				ContentSlot->SetVerticalAlignment(VAlign_Center);
			}

			Label->SetText(FText::FromString(ButtonLabels[ButtonIndex]));
			if (IsValid(MainMenuFont))
			{
				FSlateFontInfo FontInfo = Label->GetFont();
				FontInfo.FontObject = MainMenuFont;
				FontInfo.Size = 32;
				Label->SetFont(FontInfo);
			}
			Label->SetColorAndOpacity(FSlateColor(InkTextColor));
			Label->SetJustification(ETextJustify::Center);
			Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("Stage01 main menu HUD button '%s' has no TextBlock label to style."),
				*Button->GetName());
		}

		if (IsValid(RootCanvas))
		{
			if (UCanvasPanelSlot* ButtonSlot = Cast<UCanvasPanelSlot>(Button->Slot))
			{
				ButtonSlot->SetAnchors(FAnchors(0.0f, 0.5f));
				ButtonSlot->SetAlignment(FVector2D(0.0f, 0.5f));
				ButtonSlot->SetPosition(ButtonPositions[ButtonIndex]);
				ButtonSlot->SetSize(ButtonLayoutSize);
				ButtonSlot->SetZOrder(20 + ButtonIndex);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("Stage01 main menu HUD button '%s' is not attached through a CanvasPanelSlot; keeping its authored layout."),
					*Button->GetName());
			}
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("Stage01 main menu HUD art applied to '%s': title=%s inkPanel=%s buttons=%d font=%s; scene layout unchanged."),
		*MenuWidget->GetName(),
		IsValid(TitleTexture) ? TEXT("loaded") : TEXT("missing"),
		IsValid(InkPanelTexture) ? TEXT("loaded") : TEXT("missing"),
		OrderedButtons.Num(),
		IsValid(MainMenuFont) ? TEXT("AaGuDianKeBenSongYouMoBan_2_Font") : TEXT("fallback"));
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
			ApplyMainMenuHudArt(Candidate);
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
	if (bIntroPlaying || IntroState == EStage01IntroState::HudFading
		|| IntroState == EStage01IntroState::Playing
		|| IntroState == EStage01IntroState::EffectPlaying
		|| IntroState == EStage01IntroState::Fading
		|| IntroState == EStage01IntroState::Traveling)
	{
		UE_LOG(LogTemp, Warning, TEXT("Stage01 intro rejected duplicate New Game click during transition."));
		return;
	}

	BeginMainMenuHudFade();
}

void AStage01IntroDirector::BeginMainMenuHudFade()
{
	if (!IsValid(BoundMainMenuWidget))
	{
		// The click normally arrives only after TryBindMainMenu has completed.
		// Keep the functional fallback safe if a custom caller invokes the flow
		// without a bound widget.
		PlayIntro();
		return;
	}

	bIntroPlaying = false;
	bTravelRequested = false;
	HudFadeElapsed = 0.0f;
	InkEffectElapsed = 0.0f;
	IntroState = EStage01IntroState::HudFading;

	// Lock input and camera ownership immediately, but keep the widget visible
	// during the fade. SetMenuCinematicState collapses the widget by design, so
	// restore visibility for this transition before rendering the first fade
	// frame.
	SetMenuCinematicState(true);
	BoundMainMenuWidget->SetVisibility(ESlateVisibility::Visible);
	BoundMainMenuWidget->SetRenderOpacity(1.0f);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Stage01 New Game clicked; fading MainMenu HUD for %.2fs before starting the intro."),
		FMath::Max(0.0f, MenuHudFadeDuration));

	if (MenuHudFadeDuration <= KINDA_SMALL_NUMBER)
	{
		UpdateMainMenuHudFade(0.0f);
	}
}

void AStage01IntroDirector::UpdateMainMenuHudFade(float DeltaTime)
{
	if (IntroState != EStage01IntroState::HudFading)
	{
		return;
	}

	if (!IsValid(BoundMainMenuWidget))
	{
		IntroState = EStage01IntroState::Idle;
		PlayIntro();
		return;
	}

	if (MenuHudFadeDuration <= KINDA_SMALL_NUMBER)
	{
		BoundMainMenuWidget->SetRenderOpacity(0.0f);
		UE_LOG(LogTemp, Log, TEXT("Stage01 MainMenu HUD fade skipped; starting intro at authored K0."));
		IntroState = EStage01IntroState::Idle;
		PlayIntro();
		return;
	}

	const float Duration = FMath::Max(KINDA_SMALL_NUMBER, MenuHudFadeDuration);
	HudFadeElapsed = FMath::Min(
		HudFadeElapsed + FMath::Max(0.0f, DeltaTime),
		Duration);
	const float Progress = FMath::Clamp(HudFadeElapsed / Duration, 0.0f, 1.0f);
	// Ease both ends of the fade so the final portion settles gently instead
	// of dropping sharply into zero opacity.
	const float EasedProgress = FMath::InterpEaseInOut(
		0.0f,
		1.0f,
		Progress,
		FMath::Max(1.0f, MenuHudFadeEaseExponent));
	BoundMainMenuWidget->SetRenderOpacity(1.0f - EasedProgress);

	if (HudFadeElapsed >= Duration)
	{
		BoundMainMenuWidget->SetRenderOpacity(0.0f);
		UE_LOG(LogTemp, Log, TEXT("Stage01 MainMenu HUD fade completed; starting intro at authored K0."));
		IntroState = EStage01IntroState::Idle;
		PlayIntro();
	}
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
		if (bCinematic)
		{
			BoundMainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
			BoundMainMenuWidget->SetIsEnabled(false);
		}
		else
		{
			BoundMainMenuWidget->SetRenderOpacity(1.0f);
			BoundMainMenuWidget->SetVisibility(ESlateVisibility::Visible);
			BoundMainMenuWidget->SetIsEnabled(true);
		}
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

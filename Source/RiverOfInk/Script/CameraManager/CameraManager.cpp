// Fill out your copyright notice in the Description page of Project Settings.

#include "CameraManager/CameraManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RoguelikeSystem/RoguelikeExitTrigger.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#if !UE_BUILD_SHIPPING
namespace ExitGuideDiagnostics
{
	static TAutoConsoleVariable<int32> CVarEnabled(
		TEXT("roi.ExitGuide.Debug"),
		0,
		TEXT("Show the exit-guide camera diagnostic overlay. 0: hidden, 1: shown."),
		ECVF_Default);
	static TAutoConsoleVariable<int32> CVarLogEnabled(
		TEXT("roi.ExitGuide.Log"),
		1,
		TEXT("Write exit-guide camera diagnostics to readmes/debug. 0: disabled, 1: enabled."),
		ECVF_Default);

	static constexpr uint64 ScreenMessageKey = 0xE817C0DE;
	static constexpr int32 RowsPerFlush = 120;
	static bool bHasSample = false;
	static bool bIsLogging = false;
	static int32 FrameNumber = 0;
	static int32 SessionSerial = 0;
	static FVector LastCameraLocation = FVector::ZeroVector;
	static FVector LastFinalPovLocation = FVector::ZeroVector;
	static float LastOrthoWidth = 0.0f;
	static FString LogFilePath;
	static TArray<FString> PendingLogRows;

	static bool FlushLog()
	{
		if (!bIsLogging || PendingLogRows.IsEmpty())
		{
			return true;
		}

		const FString Content = FString::Join(PendingLogRows, LINE_TERMINATOR) + LINE_TERMINATOR;
		const bool bSaved = FFileHelper::SaveStringToFile(
			Content,
			*LogFilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
			&IFileManager::Get(),
			FILEWRITE_Append);
		if (!bSaved)
		{
			UE_LOG(LogTemp, Warning, TEXT("Exit guide diagnostics: failed to append %s."), *LogFilePath);
			return false;
		}

		PendingLogRows.Reset();
		return true;
	}

	static void StartLog()
	{
		bHasSample = false;
		bIsLogging = false;
		FrameNumber = 0;
		PendingLogRows.Reset();
		LogFilePath.Empty();
		if (CVarLogEnabled.GetValueOnGameThread() == 0)
		{
			return;
		}

		const FString OutputDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("readmes"), TEXT("debug"));
		if (!IFileManager::Get().DirectoryExists(*OutputDirectory)
			&& !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
		{
			UE_LOG(LogTemp, Warning, TEXT("Exit guide diagnostics: unable to create %s."), *OutputDirectory);
			return;
		}

		LogFilePath = FPaths::Combine(
			OutputDirectory,
			FString::Printf(
				TEXT("ExitGuide_%s_%03d.csv"),
				*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")),
				++SessionSerial));
		const FString Header = FString(TEXT("Frame,DeltaTimeSeconds,FPS,GuideElapsed,ViewTarget,CameraManagerCount,ActorX,ActorY,ActorZ,ActorStep,DesiredCameraX,DesiredCameraY,DesiredCameraZ,FinalPovX,FinalPovY,FinalPovZ,FinalPovStep,PlayerFocusX,PlayerFocusY,PlayerFocusZ,ExitFocusX,ExitFocusY,ExitFocusZ,BlendedFocusX,BlendedFocusY,BlendedFocusZ,AppliedOrthoWidth,DesiredOrthoWidth,OrthoStep,CurrentShakeX,CurrentShakeY,CurrentShakeZ,AppliedShakeX,AppliedShakeY,AppliedShakeZ")) + LINE_TERMINATOR;
		bIsLogging = FFileHelper::SaveStringToFile(
			Header,
			*LogFilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		if (bIsLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("Exit guide diagnostics: writing %s."), *LogFilePath);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Exit guide diagnostics: failed to create %s."), *LogFilePath);
		}
	}

	static void QueueFrame(
		float DeltaTime,
		float GuideElapsed,
		const FString& ViewTargetName,
		int32 CameraManagerCount,
		const FVector& ActualCameraLocation,
		float ActorStep,
		const FVector& DesiredCameraLocation,
		const FVector& FinalPovLocation,
		float FinalPovStep,
		const FVector& PlayerFocus,
		const FVector& ExitFocus,
		const FVector& BlendedFocus,
		float AppliedOrthoWidth,
		float DesiredOrthoWidth,
		float OrthoStep,
		const FVector& CurrentShake,
		const FVector& AppliedShake)
	{
		if (!bIsLogging)
		{
			return;
		}

		const float FrameRate = DeltaTime > KINDA_SMALL_NUMBER ? 1.0f / DeltaTime : 0.0f;
		PendingLogRows.Add(FString::Printf(
			TEXT("%d,%.6f,%.3f,%.6f,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f"),
			++FrameNumber,
			DeltaTime,
			FrameRate,
			GuideElapsed,
			*ViewTargetName,
			CameraManagerCount,
			ActualCameraLocation.X, ActualCameraLocation.Y, ActualCameraLocation.Z, ActorStep,
			DesiredCameraLocation.X, DesiredCameraLocation.Y, DesiredCameraLocation.Z,
			FinalPovLocation.X, FinalPovLocation.Y, FinalPovLocation.Z, FinalPovStep,
			PlayerFocus.X, PlayerFocus.Y, PlayerFocus.Z,
			ExitFocus.X, ExitFocus.Y, ExitFocus.Z,
			BlendedFocus.X, BlendedFocus.Y, BlendedFocus.Z,
			AppliedOrthoWidth, DesiredOrthoWidth, OrthoStep,
			CurrentShake.X, CurrentShake.Y, CurrentShake.Z,
			AppliedShake.X, AppliedShake.Y, AppliedShake.Z));

		if (PendingLogRows.Num() >= RowsPerFlush && !FlushLog())
		{
			bIsLogging = false;
			PendingLogRows.Reset();
		}
	}

	static void StopLog()
	{
		if (bIsLogging)
		{
			FlushLog();
			UE_LOG(LogTemp, Log, TEXT("Exit guide diagnostics: finished %s."), *LogFilePath);
		}
		bIsLogging = false;
		PendingLogRows.Reset();
	}
}
#endif

ACameraManager::ACameraManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// ── 显式根组件，避免引擎自动挑选场景组件作为根 ──
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// ── 创建弹簧弓（Camera Boom），效果同旧项目 ──
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);   // 弹簧臂自身保持固定旋转（俯视角）
	CameraBoom->TargetArmLength = 1600.f;         // 相机距离目标 1600 单位（高度差约 800，原为 400）
	CameraBoom->SetRelativeRotation(FRotator(-40.f, 45.f, 0.f)); // 俯视 45 度 + Yaw 45 度对齐 WASD 移动方向（接近等距视角）
	CameraBoom->bDoCollisionTest = false;

	// ── 创建摄像机 ──
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;  // 相机不随控制器转

	// ── 固定为正交投影（接近等距视角），并缩小过大的取景范围 ──
	// 使用 UCameraComponent 正式接口；在构造函数中直接落在 CDO 默认值上，PIE 重启后仍生效。
	TopDownCameraComponent->SetProjectionMode(ECameraProjectionMode::Orthographic);
	TopDownCameraComponent->SetOrthoWidth(1550.f);

}

void ACameraManager::BeginPlay()
{
	Super::BeginPlay();
	// 玩家可能比本管理器晚生成，由 Tick 每帧检测并接管
	CurrentFollowStrength = IntroFollowStrength;

	if (TopDownCameraComponent)
	{
		NormalOrthoWidth = TopDownCameraComponent->OrthoWidth;
	}
}

void ACameraManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ── 玩家生成时自动接管 ──
	if (!IsValid(TargetActor))
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			TargetActor = Pawn;

			// 进场：回到 Intro 强度，镜头从当前位置缓慢飘向玩家
			CurrentFollowStrength = IntroFollowStrength;

			// 只对齐 Z（仅一次），避免 GameMode 生成在原点导致镜头贴地；XY 由 Intro 强度自然过渡
			FVector StartLocation = GetActorLocation() - AppliedShakeOffset;
			StartLocation.Z = Pawn->GetActorLocation().Z;
			StartLocation += CurrentShakeOffset;
			AppliedShakeOffset = CurrentShakeOffset;
			SetActorLocation(StartLocation);

			// 玩家身上没有相机组件，把渲染视角切换到本管理器
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				PC->SetViewTarget(this);
			}
		}
		return;
	}


	if (bExitGuideActive)
	{
		TickExitGuide(DeltaTime);
		return;
	}
	// 跟随强度从 Intro 平滑过渡到 Normal
	CurrentFollowStrength = FMath::FInterpTo(CurrentFollowStrength, NormalFollowStrength, DeltaTime, StrengthBlendSpeed);

	// 帧率无关平滑跟随（同旧项目思路：VLerp + Strength，越大越紧）
	const FVector CurrentBaseLocation = GetActorLocation() - AppliedShakeOffset;
	const float Alpha = FMath::Clamp(CurrentFollowStrength * DeltaTime, 0.0f, 1.0f);
	FVector NewLocation = CurrentBaseLocation + (TargetActor->GetActorLocation() - CurrentBaseLocation) * Alpha;

	// 仅跟随 XY：保持本管理器当前的 Z
	if (bFollowOnlyXY)
	{
		NewLocation.Z = CurrentBaseLocation.Z;
	}

	// 叠加相机震动偏移（非震动时为零向量，不影响正常跟随）
	NewLocation += CurrentShakeOffset;

	AppliedShakeOffset = CurrentShakeOffset;
	SetActorLocation(NewLocation);
}
void ACameraManager::StartExitGuide(ARoguelikeExitTrigger* InExitTrigger)
{
	if (!IsValid(InExitTrigger))
	{
		UE_LOG(LogTemp, Warning, TEXT("Exit guide camera start skipped: exit trigger is invalid."));
		return;
	}

	if (bExitGuideActive && GuideExitTrigger.Get() == InExitTrigger)
	{
		return;
	}

	if (!IsValid(TargetActor))
	{
		TargetActor = UGameplayStatics::GetPlayerPawn(this, 0);
	}
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("Exit guide camera start skipped: player pawn is unavailable."));
		return;
	}

	GuideExitTrigger = InExitTrigger;
	bExitGuideActive = true;
	ExitGuideElapsed = 0.0f;
	CurrentShakeOffset = FVector::ZeroVector;
	ExitGuideStartLocation = GetActorLocation() - AppliedShakeOffset;
	ExitGuideStartOrthoWidth = TopDownCameraComponent
		? TopDownCameraComponent->OrthoWidth
		: NormalOrthoWidth;

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PlayerController->GetViewTarget() != this)
		{
			PlayerController->SetViewTarget(this);
		}
	}

#if !UE_BUILD_SHIPPING
	ExitGuideDiagnostics::StartLog();
#endif

	UE_LOG(LogTemp, Log,
		TEXT("Exit guide camera started: Exit=%s Intro=%.2fs ExitWeight=%.2f."),
		*GetNameSafe(InExitTrigger),
		ExitGuideIntroDuration,
		ExitGuideExitWeight);
}

void ACameraManager::StopExitGuide()
{
	if (!bExitGuideActive)
	{
		return;
	}

	bExitGuideActive = false;
	GuideExitTrigger = nullptr;
	ExitGuideElapsed = 0.0f;
	SetActorLocation(ExitGuideStartLocation + CurrentShakeOffset);
	AppliedShakeOffset = CurrentShakeOffset;
	if (TopDownCameraComponent)
	{
		TopDownCameraComponent->SetOrthoWidth(ExitGuideStartOrthoWidth);
	}
#if !UE_BUILD_SHIPPING
	ExitGuideDiagnostics::bHasSample = false;
	ExitGuideDiagnostics::StopLog();
	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(ExitGuideDiagnostics::ScreenMessageKey);
	}
#endif

	UE_LOG(LogTemp, Log, TEXT("Exit guide camera stopped; previous camera state restored."));
}

bool ACameraManager::ResolveExitGuideTargets(FVector& OutPlayerFocus, FVector& OutExitFocus) const
{
	if (!IsValid(TargetActor) || !IsValid(GuideExitTrigger))
	{
		return false;
	}

	OutPlayerFocus = TargetActor->GetActorLocation();
	OutPlayerFocus.Z += ExitGuidePlayerFocusHeight;
	OutExitFocus = GuideExitTrigger->GetGuideFocusLocation();
	return true;
}

float ACameraManager::GetCurrentViewAspectRatio() const
{
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		if (ViewportSize.X > 0 && ViewportSize.Y > 0)
		{
			return static_cast<float>(ViewportSize.X) / static_cast<float>(ViewportSize.Y);
		}
	}

	if (TopDownCameraComponent && TopDownCameraComponent->AspectRatio > KINDA_SMALL_NUMBER)
	{
		return TopDownCameraComponent->AspectRatio;
	}

	return 16.0f / 9.0f;
}

float ACameraManager::CalculateExitGuideOrthoWidth(
	const FVector& PlayerFocus,
	const FVector& ExitFocus,
	const FVector& CameraCenter) const
{
	if (!TopDownCameraComponent)
	{
		return NormalOrthoWidth;
	}

	// The camera is orthographic and keeps the existing fixed top-down rotation.
	// In camera-local space, Y is screen horizontal and Z is screen vertical.
	const FTransform ViewTransform = TopDownCameraComponent->GetComponentTransform();
	const FVector PlayerLocal = ViewTransform.InverseTransformVector(PlayerFocus - CameraCenter);
	const FVector ExitLocal = ViewTransform.InverseTransformVector(ExitFocus - CameraCenter);

	const float MaxHorizontalExtent = FMath::Max(FMath::Abs(PlayerLocal.Y), FMath::Abs(ExitLocal.Y));
	const float MaxVerticalExtent = FMath::Max(FMath::Abs(PlayerLocal.Z), FMath::Abs(ExitLocal.Z));
	const float HorizontalHalfFraction = FMath::Max(0.05f, 0.5f * ExitGuideHorizontalSafeFraction);
	const float VerticalHalfFraction = FMath::Max(0.05f, 0.5f * ExitGuideVerticalSafeFraction);
	const float WidthFromHorizontal = (MaxHorizontalExtent + ExitGuideOrthoPadding) / HorizontalHalfFraction;
	const float WidthFromVertical = (MaxVerticalExtent + ExitGuideOrthoPadding) * GetCurrentViewAspectRatio() / VerticalHalfFraction;

	const float MinimumWidth = FMath::Max(NormalOrthoWidth, ExitGuideStartOrthoWidth);
	const float RequiredWidth = FMath::Max(MinimumWidth, FMath::Max(WidthFromHorizontal, WidthFromVertical));
	return FMath::Clamp(
		RequiredWidth,
		MinimumWidth,
		FMath::Max(MinimumWidth, ExitGuideMaxOrthoWidth));
}

void ACameraManager::TickExitGuide(float DeltaTime)
{
	FVector PlayerFocus;
	FVector ExitFocus;
	if (!ResolveExitGuideTargets(PlayerFocus, ExitFocus) || !TopDownCameraComponent)
	{
		StopExitGuide();
		return;
	}

	ExitGuideElapsed += FMath::Max(0.0f, DeltaTime);
	const float RawIntroAlpha = ExitGuideIntroDuration <= KINDA_SMALL_NUMBER
		? 1.0f
		: FMath::Clamp(ExitGuideElapsed / ExitGuideIntroDuration, 0.0f, 1.0f);
	const float IntroAlpha = RawIntroAlpha * RawIntroAlpha * (3.0f - (2.0f * RawIntroAlpha));

	const FVector DesiredFocus = FMath::Lerp(
		PlayerFocus,
		ExitFocus,
		FMath::Clamp(ExitGuideExitWeight, 0.0f, 1.0f));

	const FVector CurrentBaseLocation = GetActorLocation() - AppliedShakeOffset;
	FVector NewBaseLocation = DesiredFocus;
	if (RawIntroAlpha < 1.0f)
	{
		NewBaseLocation = FMath::Lerp(ExitGuideStartLocation, DesiredFocus, IntroAlpha);
	}
	else
	{
		NewBaseLocation = FMath::VInterpTo(
			CurrentBaseLocation,
			DesiredFocus,
			DeltaTime,
			FMath::Max(0.0f, ExitGuideFollowStrength));
	}

	const float DesiredOrthoWidth = CalculateExitGuideOrthoWidth(
		PlayerFocus,
		ExitFocus,
		NewBaseLocation);
	const float CurrentOrthoWidth = TopDownCameraComponent->OrthoWidth;
	const float ZoomSpeed = DesiredOrthoWidth > CurrentOrthoWidth
		? ExitGuideZoomOutStrength
		: ExitGuideZoomInStrength;
	const float SmoothedOrthoWidth = RawIntroAlpha < 1.0f
		? FMath::Lerp(ExitGuideStartOrthoWidth, DesiredOrthoWidth, IntroAlpha)
		: FMath::FInterpTo(CurrentOrthoWidth, DesiredOrthoWidth, DeltaTime, FMath::Max(0.0f, ZoomSpeed));
	// During the intro, use the monotonic blend from the pre-guide width. Applying the
	// immediate zoom-out guard here alternates between the blended width and the target
	// width whenever the target is already reached on the previous frame.
	const float NewOrthoWidth = RawIntroAlpha < 1.0f
		? SmoothedOrthoWidth
		// After the intro, zoom out immediately when the safe frame grows; zooming in remains eased.
		: (DesiredOrthoWidth > CurrentOrthoWidth
			? FMath::Max(SmoothedOrthoWidth, DesiredOrthoWidth)
			: SmoothedOrthoWidth);

	SetActorLocation(NewBaseLocation + CurrentShakeOffset);
	AppliedShakeOffset = CurrentShakeOffset;
	TopDownCameraComponent->SetOrthoWidth(NewOrthoWidth);

#if !UE_BUILD_SHIPPING
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	APlayerCameraManager* PlayerCameraManager = PlayerController
		? PlayerController->PlayerCameraManager
		: nullptr;
	const AActor* ViewTarget = PlayerController ? PlayerController->GetViewTarget() : nullptr;
	const FVector FinalPovLocation = PlayerCameraManager
		? PlayerCameraManager->GetCameraLocation()
		: FVector::ZeroVector;
	const FVector ActualCameraLocation = GetActorLocation();

	int32 CameraManagerCount = 0;
	for (TActorIterator<ACameraManager> It(GetWorld()); It; ++It)
	{
		++CameraManagerCount;
	}

	const float ActorStep = ExitGuideDiagnostics::bHasSample
		? FVector::Dist(ActualCameraLocation, ExitGuideDiagnostics::LastCameraLocation)
		: 0.0f;
	const float FinalPovStep = ExitGuideDiagnostics::bHasSample
		? FVector::Dist(FinalPovLocation, ExitGuideDiagnostics::LastFinalPovLocation)
		: 0.0f;
	const float OrthoStep = ExitGuideDiagnostics::bHasSample
		? FMath::Abs(NewOrthoWidth - ExitGuideDiagnostics::LastOrthoWidth)
		: 0.0f;
	const FString ViewTargetName = GetNameSafe(ViewTarget);

	ExitGuideDiagnostics::QueueFrame(
		DeltaTime,
		ExitGuideElapsed,
		ViewTargetName,
		CameraManagerCount,
		ActualCameraLocation,
		ActorStep,
		NewBaseLocation,
		FinalPovLocation,
		FinalPovStep,
		PlayerFocus,
		ExitFocus,
		DesiredFocus,
		NewOrthoWidth,
		DesiredOrthoWidth,
		OrthoStep,
		CurrentShakeOffset,
		AppliedShakeOffset);

	if (GEngine && ExitGuideDiagnostics::CVarEnabled.GetValueOnGameThread() != 0)
	{
		const bool bViewTargetMismatch = ViewTarget != this;
		const bool bMultipleCameraManagers = CameraManagerCount > 1;
		const FColor PanelColor = bViewTargetMismatch || bMultipleCameraManagers ? FColor::Red : FColor::Cyan;
		const float FrameRate = DeltaTime > KINDA_SMALL_NUMBER ? 1.0f / DeltaTime : 0.0f;
		const FString DiagnosticText = FString::Printf(
			TEXT("EXIT GUIDE DIAGNOSTICS  [roi.ExitGuide.Debug 0/1]\\n")
			TEXT("dt %.2f ms | %.1f fps | guide %.2f s | ViewTarget %s | CameraManagers %d%s\\n")
			TEXT("Actor %s | step %.2f | desired %s\\n")
			TEXT("Final POV %s | step %.2f | %s\\n")
			TEXT("Focus player %s | exit %s | blended %s\\n")
			TEXT("Ortho applied %.2f | desired %.2f | step %.2f\\n")
			TEXT("Shake current %s | applied %s"),
			DeltaTime * 1000.0f,
			FrameRate,
			ExitGuideElapsed,
			*ViewTargetName,
			CameraManagerCount,
			bViewTargetMismatch ? TEXT("  < VIEW TARGET MISMATCH") : (bMultipleCameraManagers ? TEXT("  < MULTIPLE CAMERA MANAGERS") : TEXT("")),
			*ActualCameraLocation.ToCompactString(), ActorStep, *NewBaseLocation.ToCompactString(),
			*FinalPovLocation.ToCompactString(), FinalPovStep, PlayerCameraManager ? TEXT("") : TEXT("< NO PLAYER CAMERA MANAGER"),
			*PlayerFocus.ToCompactString(), *ExitFocus.ToCompactString(), *DesiredFocus.ToCompactString(),
			NewOrthoWidth, DesiredOrthoWidth, OrthoStep,
			*CurrentShakeOffset.ToCompactString(), *AppliedShakeOffset.ToCompactString());

		GEngine->AddOnScreenDebugMessage(
			ExitGuideDiagnostics::ScreenMessageKey, 0.25f, PanelColor, DiagnosticText, false, FVector2D(1.0f, 1.0f));
	}
	else if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(ExitGuideDiagnostics::ScreenMessageKey);
	}

	ExitGuideDiagnostics::bHasSample = true;
	ExitGuideDiagnostics::LastCameraLocation = ActualCameraLocation;
	ExitGuideDiagnostics::LastFinalPovLocation = FinalPovLocation;
	ExitGuideDiagnostics::LastOrthoWidth = NewOrthoWidth;
#endif
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/PlayerInputComponent.h"
#include "RiverOfInk.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"

UPlayerInputComponent::UPlayerInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Input actions and key mappings are content assets. C++ only binds them.
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveXAsset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_MoveX.IA_Player_MoveX"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveYAsset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_MoveY.IA_Player_MoveY"));
	static ConstructorHelpers::FObjectFinder<UInputAction> SprintAsset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Sprint.IA_Player_Sprint"));
	static ConstructorHelpers::FObjectFinder<UInputAction> AttackAsset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Attack.IA_Player_Attack"));
	static ConstructorHelpers::FObjectFinder<UInputAction> SecondaryAsset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Secondary.IA_Player_Secondary"));
	static ConstructorHelpers::FObjectFinder<UInputAction> DashAsset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Dash.IA_Player_Dash"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Skill1Asset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Skill1.IA_Player_Skill1"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Skill2Asset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Skill2.IA_Player_Skill2"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Skill3Asset(
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Skill3.IA_Player_Skill3"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingContextAsset(
		TEXT("/Game/Blueprint/GameSystem/Input/IMC_Player.IMC_Player"));

	MoveXAction = MoveXAsset.Object;
	MoveYAction = MoveYAsset.Object;
	ShiftAction = SprintAsset.Object;
	LmbAction = AttackAsset.Object;
	RmbAction = SecondaryAsset.Object;
	SpaceAction = DashAsset.Object;
	QAction = Skill1Asset.Object;
	EAction = Skill2Asset.Object;
	FAction = Skill3Asset.Object;
	DefaultMappingContext = MappingContextAsset.Object;
}

void UPlayerInputComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadInputAssets();
	ValidateInputAssets();
}

void UPlayerInputComponent::LoadInputAssets()
{
	// Re-apply the canonical assets at runtime so old BP_Hikari overrides cannot
	// reintroduce the legacy IMC_Hikari context.
	MoveXAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_MoveX.IA_Player_MoveX"));
	MoveYAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_MoveY.IA_Player_MoveY"));
	ShiftAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Sprint.IA_Player_Sprint"));
	LmbAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Attack.IA_Player_Attack"));
	RmbAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Secondary.IA_Player_Secondary"));
	SpaceAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Dash.IA_Player_Dash"));
	QAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Skill1.IA_Player_Skill1"));
	EAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Skill2.IA_Player_Skill2"));
	FAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IA_Player_Skill3.IA_Player_Skill3"));
	DefaultMappingContext = LoadObject<UInputMappingContext>(nullptr,
		TEXT("/Game/Blueprint/GameSystem/Input/IMC_Player.IMC_Player"));
	UE_LOG(LogRiverOfInk, Log, TEXT("Player input assets loaded: Context=%s MoveX=%s MoveY=%s."),
		*GetNameSafe(DefaultMappingContext), *GetNameSafe(MoveXAction), *GetNameSafe(MoveYAction));
}

void UPlayerInputComponent::ValidateInputAssets() const
{
	ensureMsgf(DefaultMappingContext, TEXT("Player input mapping context asset is missing."));
	ensureMsgf(MoveXAction && MoveYAction && ShiftAction, TEXT("Player movement input assets are missing."));
	ensureMsgf(LmbAction && RmbAction && SpaceAction && QAction && EAction && FAction,
		TEXT("Player action input assets are missing."));
}

void UPlayerInputComponent::MapGamepadKey(
	UInputMappingContext* Context,
	UInputAction* Action,
	const FKey& Key)
{
	if (!Context || !Action || !Key.IsValid())
	{
		return;
	}

	// 同一条动作 + 同一个键重复映射会让轴值翻倍，所以先查重再挂。
	for (const FEnhancedActionKeyMapping& Existing : Context->GetMappings())
	{
		if (Existing.Action == Action && Existing.Key == Key)
		{
			return;
		}
	}

	Context->MapKey(Action, Key);
}

void UPlayerInputComponent::BuildGamepadMappingContext()
{
	if (!bEnableGamepadInput)
	{
		GamepadMappingContext = nullptr;
		return;
	}

	// 临时对象：不写进 IMC_Player 资产（避免把编辑器资产改脏），也不需要额外 cook，
	// 打包后一定存在——键位在编辑器里通过上面的 UPROPERTY 覆盖。
	if (!GamepadMappingContext)
	{
		GamepadMappingContext = NewObject<UInputMappingContext>(
			this, TEXT("IMC_Player_Gamepad"), RF_Transient);
	}

	if (!AimXAction)
	{
		AimXAction = NewObject<UInputAction>(this, TEXT("IA_Player_GamepadAimX"), RF_Transient);
		AimXAction->ValueType = EInputActionValueType::Axis1D;
	}
	if (!AimYAction)
	{
		AimYAction = NewObject<UInputAction>(this, TEXT("IA_Player_GamepadAimY"), RF_Transient);
		AimYAction->ValueType = EInputActionValueType::Axis1D;
	}

	// 左摇杆 → 移动（复用 WSAD 已有的动作，整条移动管线不用改）
	MapGamepadKey(GamepadMappingContext, MoveXAction, GamepadMoveXKey);
	MapGamepadKey(GamepadMappingContext, MoveYAction, GamepadMoveYKey);

	// 右摇杆 → 攻击朝向（独立动作，不参与移动）
	MapGamepadKey(GamepadMappingContext, AimXAction, GamepadAimXKey);
	MapGamepadKey(GamepadMappingContext, AimYAction, GamepadAimYKey);

	// 扳机 / 肩键 / 面键 → 已有的战斗动作
	MapGamepadKey(GamepadMappingContext, LmbAction, GamepadAttackKey);      // 右扳机 = 左键普攻
	MapGamepadKey(GamepadMappingContext, RmbAction, GamepadSecondaryKey);   // 西键 = 右键特攻
	MapGamepadKey(GamepadMappingContext, QAction, GamepadSkill1Key);        // 左肩键 = Q 法术
	MapGamepadKey(GamepadMappingContext, EAction, GamepadSkill2Key);        // 右肩键 = E 斩击
	MapGamepadKey(GamepadMappingContext, SpaceAction, GamepadDashKey);      // 左扳机 = 冲刺

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Gamepad mapping ready: Context=%s Move=(%s,%s) Aim=(%s,%s) Attack=%s Secondary=%s Skill1=%s Skill2=%s Dash=%s."),
		*GetNameSafe(GamepadMappingContext),
		*GamepadMoveXKey.ToString(), *GamepadMoveYKey.ToString(),
		*GamepadAimXKey.ToString(), *GamepadAimYKey.ToString(),
		*GamepadAttackKey.ToString(), *GamepadSecondaryKey.ToString(),
		*GamepadSkill1Key.ToString(), *GamepadSkill2Key.ToString(),
		*GamepadDashKey.ToString());
}

void UPlayerInputComponent::SetupEnhancedInput(UEnhancedInputComponent* EnhancedInput, APlayerController* PC)
{
	if (!EnhancedInput || !PC || bInputSetup) return;

	CachedPlayerController = PC;

	LoadInputAssets();
	ValidateInputAssets();
	if (!DefaultMappingContext || !MoveXAction || !MoveYAction || !ShiftAction ||
		!LmbAction || !RmbAction || !SpaceAction || !QAction || !EAction || !FAction)
	{
		return;
	}

	// 先把手柄上下文和右摇杆动作准备好（没有本地玩家时也不会拿到空动作）。
	BuildGamepadMappingContext();

	// ── 注册 Mapping Context 到子系统 ──
	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (UInputMappingContext* LegacyContext = LoadObject<UInputMappingContext>(nullptr,
				TEXT("/Game/Blueprint/GameSystem/Input/IMC_Hikari.IMC_Hikari")))
			{
				Subsystem->RemoveMappingContext(LegacyContext);
			}
			Subsystem->AddMappingContext(DefaultMappingContext, 0);

			// 手柄映射单独一个上下文：手柄键和键鼠键不同，所以两边互不覆盖，
			// 动的只是同一条 InputAction（左摇杆并入移动、扳机并入普攻/特攻）。
			if (GamepadMappingContext)
			{
				Subsystem->AddMappingContext(GamepadMappingContext, 0);
			}
		}
	}

	// ── 绑定回调 ──
	// 轴（Triggered = 按住持续触发；Completed = 松开广播 0，让订阅方清除输入方向）
	EnhancedInput->BindAction(MoveXAction, ETriggerEvent::Triggered, this, &UPlayerInputComponent::OnMoveX);
	EnhancedInput->BindAction(MoveXAction, ETriggerEvent::Completed, this, &UPlayerInputComponent::OnMoveX);
	EnhancedInput->BindAction(MoveYAction, ETriggerEvent::Triggered, this, &UPlayerInputComponent::OnMoveY);
	EnhancedInput->BindAction(MoveYAction, ETriggerEvent::Completed, this, &UPlayerInputComponent::OnMoveY);

	// 右摇杆瞄准（手柄专用动作；Completed 时归零，避免松杆后朝向还在飘）
	if (AimXAction && AimYAction)
	{
		EnhancedInput->BindAction(AimXAction, ETriggerEvent::Triggered, this, &UPlayerInputComponent::OnAimX);
		EnhancedInput->BindAction(AimXAction, ETriggerEvent::Completed, this, &UPlayerInputComponent::OnAimX);
		EnhancedInput->BindAction(AimYAction, ETriggerEvent::Triggered, this, &UPlayerInputComponent::OnAimY);
		EnhancedInput->BindAction(AimYAction, ETriggerEvent::Completed, this, &UPlayerInputComponent::OnAimY);
	}

	// 疾跑（Shift 按住）：Triggered 持续触发、Completed 松开时广播 0。
	EnhancedInput->BindAction(ShiftAction, ETriggerEvent::Triggered, this, &UPlayerInputComponent::OnShift);
	EnhancedInput->BindAction(ShiftAction, ETriggerEvent::Completed, this, &UPlayerInputComponent::OnShift);

	// 动作（Started = 按下的瞬间触发一次）
	EnhancedInput->BindAction(LmbAction, ETriggerEvent::Started, this, &UPlayerInputComponent::OnLmb);
	EnhancedInput->BindAction(RmbAction, ETriggerEvent::Started, this, &UPlayerInputComponent::OnRmb);
	EnhancedInput->BindAction(SpaceAction, ETriggerEvent::Started, this, &UPlayerInputComponent::OnSpace);
	EnhancedInput->BindAction(QAction, ETriggerEvent::Started, this, &UPlayerInputComponent::OnQ);
	EnhancedInput->BindAction(EAction, ETriggerEvent::Started, this, &UPlayerInputComponent::OnE);
	EnhancedInput->BindAction(FAction, ETriggerEvent::Started, this, &UPlayerInputComponent::OnF);
	bInputSetup = true;
	UE_LOG(LogRiverOfInk, Log, TEXT("Player input binding ready: Context=%s."),
		*GetNameSafe(DefaultMappingContext));
}

// ──────────────────────────────
// 轴回调（按住持续触发）
// ──────────────────────────────

void UPlayerInputComponent::OnMoveX(const FInputActionValue& Value)
{
	CurrentMoveX = SanitizeMoveAxis(Value.Get<float>());
	OnMoveXDelegate.Broadcast(CurrentMoveX);
}

void UPlayerInputComponent::OnMoveY(const FInputActionValue& Value)
{
	CurrentMoveY = SanitizeMoveAxis(Value.Get<float>());
	OnMoveYDelegate.Broadcast(CurrentMoveY);
}

float UPlayerInputComponent::SanitizeMoveAxis(float RawValue) const
{
	// 同轴正反键同时按下（SOCD neutral）：增强输入会把同一条轴上的多个按键映射相加，
	// A=-1 + D=+1 本身就是 0；如果映射被配成同号则会得到 |值|>1，
	// 这里统一按“反向抵消”处理成 0，保证“AD / WS 同时按 → 该轴不产生移动”。
	if (FMath::Abs(RawValue) > 1.0f)
	{
		return 0.0f;
	}

	// 死区：手柄摇杆的轻微漂移不产生移动；键盘 ±1 不受影响。
	return FMath::Abs(RawValue) < MoveAxisDeadZone ? 0.0f : RawValue;
}

FVector UPlayerInputComponent::GetMoveWorldDirection() const
{
	// 与 UPlayerState_Move / Attack1 / Attack2 内的合成算法保持一致：
	// A/D 沿世界 (-1,1,0)，W/S 沿世界 (1,1,0)，即固定的等距方向映射。
	const FVector XDir = (FVector::RightVector - FVector::ForwardVector).GetSafeNormal();
	const FVector YDir = (FVector::ForwardVector + FVector::RightVector).GetSafeNormal();

	FVector Direction = XDir * CurrentMoveX + YDir * CurrentMoveY;
	if (Direction.SizeSquared() > 1.0f)
	{
		Direction = Direction.GetSafeNormal();
	}

	return Direction;
}

// ──────────────────────────────
// 输入缓冲（预输入）
// ──────────────────────────────

void UPlayerInputComponent::BufferInput(EPlayerBufferedInput Input)
{
	if (Input == EPlayerBufferedInput::None || InputBufferWindow <= 0.0f)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	BufferedInputTimes.Add(Input, Now);
}

bool UPlayerInputComponent::HasBufferedInput(EPlayerBufferedInput Input) const
{
	if (Input == EPlayerBufferedInput::None || InputBufferWindow <= 0.0f)
	{
		return false;
	}

	const double* FoundTime = BufferedInputTimes.Find(Input);
	if (!FoundTime)
	{
		return false;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return (Now - *FoundTime) <= static_cast<double>(InputBufferWindow);
}

bool UPlayerInputComponent::ConsumeBufferedInput(EPlayerBufferedInput Input)
{
	if (Input == EPlayerBufferedInput::None || InputBufferWindow <= 0.0f)
	{
		return false;
	}

	double PressedTime = 0.0;
	if (!BufferedInputTimes.RemoveAndCopyValue(Input, PressedTime))
	{
		return false;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return (Now - PressedTime) <= static_cast<double>(InputBufferWindow);
}

void UPlayerInputComponent::ClearBufferedInput(EPlayerBufferedInput Input)
{
	if (Input == EPlayerBufferedInput::None)
	{
		return;
	}

	BufferedInputTimes.Remove(Input);
}

void UPlayerInputComponent::ClearAllBufferedInputs()
{
	BufferedInputTimes.Reset();
}

void UPlayerInputComponent::OnShift(const FInputActionValue& Value)
{
	CurrentShiftValue = Value.Get<float>();
	OnShiftDelegate.Broadcast(CurrentShiftValue);
}

// ──────────────────────────────
// 手柄：右摇杆瞄准
// ──────────────────────────────

void UPlayerInputComponent::OnAimX(const FInputActionValue& Value)
{
	const float Raw = Value.Get<float>();
	CurrentAimX = FMath::Abs(Raw) < AimAxisDeadZone ? 0.0f : Raw;
}

void UPlayerInputComponent::OnAimY(const FInputActionValue& Value)
{
	const float Raw = Value.Get<float>();
	CurrentAimY = FMath::Abs(Raw) < AimAxisDeadZone ? 0.0f : Raw;
}

bool UPlayerInputComponent::HasGamepadAimInput() const
{
	return bEnableGamepadInput
		&& (!FMath::IsNearlyZero(CurrentAimX) || !FMath::IsNearlyZero(CurrentAimY));
}

bool UPlayerInputComponent::IsGamepadInputActive() const
{
	if (!bEnableGamepadInput)
	{
		return false;
	}

	const APlayerController* PC = CachedPlayerController.Get();
	if (!PC)
	{
		return false;
	}

	// 按钮类（扳机 / 肩键 / 面键 / 十字键）：直接查按下状态
	const FKey ButtonKeys[] =
	{
		GamepadAttackKey,
		GamepadSecondaryKey,
		GamepadSkill1Key,
		GamepadSkill2Key,
		GamepadDashKey,
		EKeys::Gamepad_DPad_Up,
		EKeys::Gamepad_DPad_Down,
		EKeys::Gamepad_DPad_Left,
		EKeys::Gamepad_DPad_Right,
		EKeys::Gamepad_FaceButton_Bottom,
		EKeys::Gamepad_FaceButton_Right,
		EKeys::Gamepad_FaceButton_Left,
		EKeys::Gamepad_FaceButton_Top,
		EKeys::Gamepad_LeftShoulder,
		EKeys::Gamepad_RightShoulder,
	};
	for (const FKey& Key : ButtonKeys)
	{
		if (PC->IsInputKeyDown(Key))
		{
			return true;
		}
	}

	// 摇杆 / 扳机是轴键，用模拟量再确认一次（阈值和移动死区一致）。
	const float AnalogThreshold = FMath::Max(MoveAxisDeadZone, 0.2f);
	const FKey AnalogKeys[] =
	{
		GamepadMoveXKey,
		GamepadMoveYKey,
		GamepadAimXKey,
		GamepadAimYKey,
		GamepadAttackKey,
		GamepadSecondaryKey,
		GamepadDashKey,
	};
	for (const FKey& Key : AnalogKeys)
	{
		if (FMath::Abs(PC->GetInputAnalogKeyState(Key)) > AnalogThreshold)
		{
			return true;
		}
	}

	return false;
}

bool UPlayerInputComponent::GetGamepadAimWorldDirection(FVector& OutDirection) const
{
	if (!HasGamepadAimInput())
	{
		return false;
	}

	// 与 GetMoveWorldDirection 用同一套固定等距映射：摇杆向右 = 屏幕右，摇杆向上 = 屏幕上。
	const FVector XDir = (FVector::RightVector - FVector::ForwardVector).GetSafeNormal();
	const FVector YDir = (FVector::ForwardVector + FVector::RightVector).GetSafeNormal();

	FVector Direction = XDir * CurrentAimX + YDir * CurrentAimY;
	if (Direction.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutDirection = Direction.GetSafeNormal();
	return true;
}

// ──────────────────────────────
// 动作回调（按下一次触发）
// ──────────────────────────────

void UPlayerInputComponent::OnLmb()
{
	DispatchLmb();
}

void UPlayerInputComponent::DispatchPrimaryAttackInput()
{
	DispatchLmb();
}

void UPlayerInputComponent::DispatchLmb()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (LastLmbDispatchTime >= 0.0 && (Now - LastLmbDispatchTime) < 0.02)
	{
		return;
	}
	LastLmbDispatchTime = Now;
	BufferInput(EPlayerBufferedInput::Attack1);
	OnLmbDelegate.Broadcast();
}

void UPlayerInputComponent::OnRmb()
{
	BufferInput(EPlayerBufferedInput::Attack2);
	OnRmbDelegate.Broadcast();
}

void UPlayerInputComponent::OnSpace()
{
	BufferInput(EPlayerBufferedInput::Dash);
	OnSpaceDelegate.Broadcast();
}

void UPlayerInputComponent::OnQ()
{
	BufferInput(EPlayerBufferedInput::Skill1);
	OnQDelegate.Broadcast();
}

void UPlayerInputComponent::OnE()
{
	BufferInput(EPlayerBufferedInput::Skill2);
	OnEDelegate.Broadcast();
}

void UPlayerInputComponent::OnF()
{
	OnFDelegate.Broadcast();
}


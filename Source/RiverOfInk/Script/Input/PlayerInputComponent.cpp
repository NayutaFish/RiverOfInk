// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/PlayerInputComponent.h"
#include "RiverOfInk.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"

UPlayerInputComponent::UPlayerInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Input actions and key mappings are content assets. C++ only binds them.
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveXAsset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_MoveX.IA_Player_MoveX"));
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveYAsset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_MoveY.IA_Player_MoveY"));
	static ConstructorHelpers::FObjectFinder<UInputAction> SprintAsset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Sprint.IA_Player_Sprint"));
	static ConstructorHelpers::FObjectFinder<UInputAction> AttackAsset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Attack.IA_Player_Attack"));
	static ConstructorHelpers::FObjectFinder<UInputAction> SecondaryAsset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Secondary.IA_Player_Secondary"));
	static ConstructorHelpers::FObjectFinder<UInputAction> DashAsset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Dash.IA_Player_Dash"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Skill1Asset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Skill1.IA_Player_Skill1"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Skill2Asset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Skill2.IA_Player_Skill2"));
	static ConstructorHelpers::FObjectFinder<UInputAction> Skill3Asset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Skill3.IA_Player_Skill3"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingContextAsset(
		TEXT("/Game/A_StudyContent/BluePrint/Input/IMC_Player.IMC_Player"));

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
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_MoveX.IA_Player_MoveX"));
	MoveYAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_MoveY.IA_Player_MoveY"));
	ShiftAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Sprint.IA_Player_Sprint"));
	LmbAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Attack.IA_Player_Attack"));
	RmbAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Secondary.IA_Player_Secondary"));
	SpaceAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Dash.IA_Player_Dash"));
	QAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Skill1.IA_Player_Skill1"));
	EAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Skill2.IA_Player_Skill2"));
	FAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IA_Player_Skill3.IA_Player_Skill3"));
	DefaultMappingContext = LoadObject<UInputMappingContext>(nullptr,
		TEXT("/Game/A_StudyContent/BluePrint/Input/IMC_Player.IMC_Player"));
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

void UPlayerInputComponent::SetupEnhancedInput(UEnhancedInputComponent* EnhancedInput, APlayerController* PC)
{
	if (!EnhancedInput || !PC || bInputSetup) return;

	LoadInputAssets();
	ValidateInputAssets();
	if (!DefaultMappingContext || !MoveXAction || !MoveYAction || !ShiftAction ||
		!LmbAction || !RmbAction || !SpaceAction || !QAction || !EAction || !FAction)
	{
		return;
	}

	// ── 注册 Mapping Context 到子系统 ──
	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (UInputMappingContext* LegacyContext = LoadObject<UInputMappingContext>(nullptr,
				TEXT("/Game/A_StudyContent/BluePrint/Input/IMC_Hikari.IMC_Hikari")))
			{
				Subsystem->RemoveMappingContext(LegacyContext);
			}
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// ── 绑定回调 ──
	// 轴（Triggered = 按住持续触发；Completed = 松开广播 0，让订阅方清除输入方向）
	EnhancedInput->BindAction(MoveXAction, ETriggerEvent::Triggered, this, &UPlayerInputComponent::OnMoveX);
	EnhancedInput->BindAction(MoveXAction, ETriggerEvent::Completed, this, &UPlayerInputComponent::OnMoveX);
	EnhancedInput->BindAction(MoveYAction, ETriggerEvent::Triggered, this, &UPlayerInputComponent::OnMoveY);
	EnhancedInput->BindAction(MoveYAction, ETriggerEvent::Completed, this, &UPlayerInputComponent::OnMoveY);
	EnhancedInput->BindAction(ShiftAction, ETriggerEvent::Triggered, this, &UPlayerInputComponent::OnShift);

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
	OnMoveXDelegate.Broadcast(Value.Get<float>());
}

void UPlayerInputComponent::OnMoveY(const FInputActionValue& Value)
{
	OnMoveYDelegate.Broadcast(Value.Get<float>());
}

void UPlayerInputComponent::OnShift(const FInputActionValue& Value)
{
	OnShiftDelegate.Broadcast(Value.Get<float>());
}

// ──────────────────────────────
// 动作回调（按下一次触发）
// ──────────────────────────────

void UPlayerInputComponent::OnLmb()
{
	OnLmbDelegate.Broadcast();
}

void UPlayerInputComponent::OnRmb()
{
	OnRmbDelegate.Broadcast();
}

void UPlayerInputComponent::OnSpace()
{
	OnSpaceDelegate.Broadcast();
}

void UPlayerInputComponent::OnQ()
{
	OnQDelegate.Broadcast();
}

void UPlayerInputComponent::OnE()
{
	OnEDelegate.Broadcast();
}

void UPlayerInputComponent::OnF()
{
	OnFDelegate.Broadcast();
}


// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Common/AttackAreaBase.h"
#include "Common/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Player/Attack/AttackArea_PlayerAttack1.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Player/Skill/SkillComponent.h"
#include "RoguelikeSystem/RoguelikeRuntimeDataSubsystem.h"
#include "RoguelikeSystem/RoguelikeShopManager.h"
#include "Input/PlayerInputComponent.h"
#include "InputCoreTypes.h"
#include "EnhancedInputComponent.h"
#include "Common/StateBase.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Player/PlayerState/PlayerState_Attack1.h"
#include "Player/PlayerState/PlayerState_Attack2.h"
#include "Player/PlayerState/PlayerState_Dash.h"
#include "Player/PlayerState/PlayerState_HitBack.h"
#include "Player/PlayerState/PlayerState_Skill1.h"
#include "Player/PlayerState/PlayerState_Skill2.h"
#include "UI/PlayerHealthWidget.h"
#include "UI/PlayerSkillWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel3);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);

	SkillComponent = CreateDefaultSubobject<USkillComponent>(TEXT("SkillComponent"));
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthWidgetClass = UPlayerHealthWidget::StaticClass();
	SkillWidgetClass = UPlayerSkillWidget::StaticClass();
	static ConstructorHelpers::FClassFinder<UPlayerSkillWidget> SkillWidgetBlueprint(
		TEXT("/Game/Blueprint/GameSystem/UI/Skill/WBP_SkillHUD"));
	if (SkillWidgetBlueprint.Succeeded())
	{
		SkillWidgetClass = SkillWidgetBlueprint.Class;
	}
	ShopInteractionKey = EKeys::J;

	// ── 状态机与输入组件：纯 C++ 自建，无需蓝图挂载 ──
	CreateDefaultSubobject<UPlayerInputComponent>(TEXT("PlayerInputComponent"));
	CreateDefaultSubobject<UPlayerState_Idle>(TEXT("PlayerState_Idle"));
	CreateDefaultSubobject<UPlayerState_Move>(TEXT("PlayerState_Move"));
	PlayerState_Attack1 = CreateDefaultSubobject<UPlayerState_Attack1>(TEXT("PlayerState_Attack1"));
	CreateDefaultSubobject<UPlayerState_Attack2>(TEXT("PlayerState_Attack2"));
	CreateDefaultSubobject<UPlayerState_Dash>(TEXT("PlayerState_Dash"));
	CreateDefaultSubobject<UPlayerState_HitBack>(TEXT("PlayerState_HitBack"));
	CreateDefaultSubobject<UPlayerState_Skill1>(TEXT("PlayerState_Skill1"));
	CreateDefaultSubobject<UPlayerState_Skill2>(TEXT("PlayerState_Skill2"));

	// ── 模型与动画（纯 C++ 直接引用资产，不依赖蓝图）──
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("/Game/RawContent/Character/Player/Mesh/SK_Hikari.SK_Hikari"));
		if (MeshAsset.Succeeded())
		{
			MeshComp->SetSkeletalMeshAsset(MeshAsset.Object);
		}

		static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBPAsset(TEXT("/Game/RawContent/Character/Player/AnimBP/ABP_Hikari.ABP_Hikari_C"));
		if (AnimBPAsset.Succeeded())
		{
			MeshComp->SetAnimInstanceClass(AnimBPAsset.Class);
		}

		// 模型正面默认朝 +Y，旋转 -90 度使其对齐角色 +X（移动方向），随 bOrientRotationToMovement 转向
		MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

		// 动画网格不参与碰撞（由胶囊体负责）
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool APlayerCharacter::CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const
{
	OutRuntimeData = FPlayerRuntimeData();
	bool bCapturedAllComponents = true;

	if (HealthComponent)
	{
		HealthComponent->CaptureRuntimeData(OutRuntimeData);
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Player runtime capture skipped HealthComponent: Player=%s."),
			*GetName());
		bCapturedAllComponents = false;
	}

	if (SkillComponent)
	{
		SkillComponent->CaptureRuntimeData(OutRuntimeData);
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Player runtime capture skipped SkillComponent: Player=%s."),
			*GetName());
		bCapturedAllComponents = false;
	}

	OutRuntimeData.Stats.WalkSpeed = FMath::Max(0.0f, WalkSpeed);
	OutRuntimeData.Stats.SprintSpeed = FMath::Max(
		OutRuntimeData.Stats.WalkSpeed,
		SprintSpeed);

	return bCapturedAllComponents;
}

bool APlayerCharacter::ApplyRuntimeData(const FPlayerRuntimeData& InRuntimeData)
{
	bool bAppliedAllComponents = true;

	if (HealthComponent)
	{
		HealthComponent->ApplyRuntimeData(InRuntimeData);
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Player runtime apply skipped HealthComponent: Player=%s."),
			*GetName());
		bAppliedAllComponents = false;
	}

	if (SkillComponent)
	{
		SkillComponent->ApplyRuntimeData(InRuntimeData);
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Player runtime apply skipped SkillComponent: Player=%s."),
			*GetName());
		bAppliedAllComponents = false;
	}

	WalkSpeed = FMath::Max(0.0f, InRuntimeData.Stats.WalkSpeed);
	SprintSpeed = FMath::Max(WalkSpeed, InRuntimeData.Stats.SprintSpeed);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}

	return bAppliedAllComponents;
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (!HealthComponent)
	{
		UE_LOG(LogRiverOfInk, Error, TEXT("Player health initialization failed: HealthComponent is missing."));
	}
	else
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &APlayerCharacter::HandleHealthChanged);
		HealthComponent->OnDeath.AddDynamic(this, &APlayerCharacter::HandleHealthDeath);
		HealthComponent->OnTakeDirectDamage.AddDynamic(this, &APlayerCharacter::HandleHealthDirectDamage);
		HealthComponent->InitializeHealth();
	}
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// Defaults are initialized first. A later level-spawned Pawn restores the
	// snapshot held by the GameInstance subsystem instead of replacing it with
	// Blueprint defaults again.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URoguelikeRuntimeDataSubsystem* RuntimeData = GameInstance->GetSubsystem<URoguelikeRuntimeDataSubsystem>())
		{
			if (RuntimeData->HasPlayerRuntimeData())
			{
				RuntimeData->ApplyRegisteredPlayerRuntimeData(this);
			}
			else
			{
				RuntimeData->CapturePlayerRuntimeData(this);
			}
		}
	}

	// ── 默认进入 Idle 状态 ──
	// 自动从角色身上挂载的组件中查找 Idle 状态
	if (UPlayerState_Idle* Idle = FindComponentByClass<UPlayerState_Idle>())
	{
		SwitchState(UPlayerState_Idle::StaticClass());
	}

	CreateHealthWidget();
	CreateSkillWidget();

	// ── 玩家生成完毕 ──
	FEventBus::Publish<FPlayerSpawnedEvent>(FPlayerSpawnedEvent(this));
	if (HealthComponent)
	{
		FEventBus::Publish<FPlayerHealthChangedEvent>(
			FPlayerHealthChangedEvent(
				FMath::RoundToInt(HealthComponent->GetMaxHealth()),
				FMath::RoundToInt(HealthComponent->GetCurrentHealth())));
	}
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HealthWidget)
    {
        HealthWidget->RemoveFromParent();
        HealthWidget = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}
void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasActorBegunPlay())
	{
		CreateHealthWidget();
		CreateSkillWidget();
	}
}

void APlayerCharacter::CreateHealthWidget()
{
	if (HealthWidget)
	{
		return;
	}

	if (!IsLocallyControlled())
	{
		UE_LOG(LogRiverOfInk, Verbose, TEXT("Health HUD skipped: %s is not locally controlled yet."), *GetName());
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	}

	if (!PlayerController)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("Health HUD skipped: no local PlayerController for %s."), *GetName());
		return;
	}

	TSubclassOf<UPlayerHealthWidget> WidgetClass = HealthWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UPlayerHealthWidget::StaticClass();
	}
	HealthWidget = CreateWidget<UPlayerHealthWidget>(PlayerController, WidgetClass);
	if (!HealthWidget)
	{
		UE_LOG(LogRiverOfInk, Error, TEXT("Health HUD creation failed for %s."), *GetName());
		return;
	}

	HealthWidget->AddToViewport(10);
	HealthWidget->InitializeForPlayer(this);
	UE_LOG(LogRiverOfInk, Log, TEXT("Health HUD created for %s."), *GetName());
}

void APlayerCharacter::CreateSkillWidget()
{
	if (SkillWidget)
	{
		return;
	}

	if (!IsLocallyControlled())
	{
		UE_LOG(LogSkill, Verbose, TEXT("Skill HUD skipped: %s is not locally controlled yet."), *GetName());
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	}

	if (!PlayerController)
	{
		UE_LOG(LogSkill, Warning, TEXT("Skill HUD skipped: no local PlayerController for %s."), *GetName());
		return;
	}

	TSubclassOf<UPlayerSkillWidget> WidgetClass = SkillWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UPlayerSkillWidget::StaticClass();
	}

	SkillWidget = CreateWidget<UPlayerSkillWidget>(PlayerController, WidgetClass);
	if (!SkillWidget)
	{
		UE_LOG(LogSkill, Error, TEXT("Skill HUD creation failed for %s."), *GetName());
		return;
	}

	// The native widget owns its presentation, but the character explicitly
	// resets visibility in case a Blueprint subclass kept a designer preview state.
	SkillWidget->SetVisibility(ESlateVisibility::Visible);
	SkillWidget->SetRenderOpacity(1.0f);
	SkillWidget->AddToViewport(20);
	SkillWidget->InitializeForPlayer(this);
	UE_LOG(LogSkill, Log, TEXT("Skill HUD created for %s. InViewport=%s Visibility=%d."),
		*GetName(),
		SkillWidget->IsInViewport() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(SkillWidget->GetVisibility()));
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState)
	{
		CurrentState->Update(DeltaTime);
	}
}

bool APlayerCharacter::IsDead() const
{
	return HealthComponent ? HealthComponent->IsDead() : bIsDead;
}

void APlayerCharacter::HandleHealthChanged(float InCurrentHealth, float InMaxHealth)
{
	FEventBus::Publish<FPlayerHealthChangedEvent>(
		FPlayerHealthChangedEvent(FMath::RoundToInt(InMaxHealth), FMath::RoundToInt(InCurrentHealth)));
}

void APlayerCharacter::HandleHealthDeath(AActor* DeadActor)
{
	(void)DeadActor;
	Die();
}

void APlayerCharacter::HandleHealthDirectDamage(const FTakeDamageInfo& InInfo)
{
	LastAttacker = InInfo.Attacker;
	OnTakeDirectDamage.Broadcast(InInfo);

	// 通告玩家受到直接性攻击（供相机震动等订阅）
	FEventBus::Publish<FPlayerTookDirectDamageEvent>(FPlayerTookDirectDamageEvent(InInfo));

	// Direct damage grants the player a short invincibility window. This is
	// player-specific state, so it remains outside the reusable health pool.
	bIsInDirectDamageInvincible = true;
	GetWorldTimerManager().ClearTimer(InvincibleTimerHandle);
	GetWorldTimerManager().SetTimer(InvincibleTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bIsInDirectDamageInvincible = false;
	}), 0.5f, false);
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateHealthWidget();
	CreateSkillWidget();

	// 通知 PlayerInputComponent 注册子系统和绑定回调（此时 Controller 和 InputComponent 均已就绪）
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (UPlayerInputComponent* InputComp = FindComponentByClass<UPlayerInputComponent>())
		{
			InputComp->SetupEnhancedInput(EnhancedInput, Cast<APlayerController>(GetController()));
			// Keep a raw UInputComponent fallback for the first click after a PIE
			// viewport captures the mouse. DispatchPrimaryAttackInput de-duplicates
			// this path against Enhanced Input when both receive the same press.
			PlayerInputComponent->BindKey(
				EKeys::LeftMouseButton,
				IE_Pressed,
				InputComp,
				&UPlayerInputComponent::DispatchPrimaryAttackInput);
		}
	}

	if (PlayerInputComponent && ShopInteractionKey.IsValid())
	{
		PlayerInputComponent->BindKey(
			ShopInteractionKey,
			IE_Pressed,
			this,
			&APlayerCharacter::TryInteractWithShop);
	}
}

void APlayerCharacter::SetNearbyShopManager(ARoguelikeShopManager* InShopManager)
{
	NearbyShopManager = InShopManager;
}

void APlayerCharacter::ClearNearbyShopManager(ARoguelikeShopManager* InShopManager)
{
	if (!InShopManager || NearbyShopManager.Get() == InShopManager)
	{
		NearbyShopManager.Reset();
	}
}

void APlayerCharacter::TryInteractWithShop()
{
	if (NearbyShopManager.IsValid())
	{
		NearbyShopManager->TryOpenShop(this);
	}
}

FText APlayerCharacter::GetShopInteractionKeyLabel() const
{
	return ShopInteractionKey.IsValid()
		? ShopInteractionKey.GetDisplayName(false)
		: FText::FromString(TEXT("J"));
}

// ── 主动技能 ──

void APlayerCharacter::TryCastSkill1()
{
	TryCastSkillSlot1();
}

void APlayerCharacter::TryCastSkill2()
{
	TryCastSkillSlot2();
}

void APlayerCharacter::TryCastSkillSlot1()
{
	if (SkillComponent)
	{
		SkillComponent->TryCastSkillSlot(0);
	}
}

void APlayerCharacter::TryCastSkillSlot2()
{
	if (SkillComponent)
	{
		SkillComponent->TryCastSkillSlot(1);
	}
}

void APlayerCharacter::TryCastSkillSlot3()
{
	if (SkillComponent)
	{
		SkillComponent->TryCastSkillSlot(2);
	}
}

// ── 状态机 ──

void APlayerCharacter::SwitchState(TSubclassOf<UStateBase> StateClass)
{
	if (!StateClass) return;

	// 查找指定子类的状态组件
	TArray<UStateBase*> Found;
	GetComponents(StateClass, Found);
	if (Found.Num() == 0) return;

	UStateBase* NewState = Found[0];
	if (NewState == CurrentState) return;

	// 旧状态退出
	if (CurrentState)
	{
		CurrentState->OnExit();
	}

	// 新状态进入
	CurrentState = NewState;
	CurrentState->OnEnter();
}

// ── 状态 ──

bool APlayerCharacter::CanMove() const
{
	return CurrentActionState == EHikariActionState::Normal;
}

bool APlayerCharacter::CanStartAction() const
{
	return CurrentActionState == EHikariActionState::Normal;
}

void APlayerCharacter::SetActionState(EHikariActionState NewState)
{
	if (CurrentActionState == NewState) return;
	CurrentActionState = NewState;
}

// ── 攻击动画（左键） ──

void APlayerCharacter::BeginAttack(bool bRestartMontage)
{
	if (!bRestartMontage && !CanStartAction()) return;
	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttackMontage is not set."));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;

	// A buffered combo step reuses the current montage when no second montage
	// asset exists. Stop the old section first so the second step restarts at
	// the beginning instead of continuing from the previous frame.
	if (bRestartMontage && AnimInstance->Montage_IsPlaying(AttackMontage))
	{
		AnimInstance->Montage_Stop(0.0f, AttackMontage);
	}

	SetActionState(EHikariActionState::Attacking);

	float MontageLength = PlayAnimMontage(AttackMontage);
	if (MontageLength <= 0.0f)
	{
		SetActionState(EHikariActionState::Normal);
		return;
	}

	FOnMontageEnded Delegate;
	Delegate.BindUObject(this, &APlayerCharacter::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(Delegate, AttackMontage);
}

void APlayerCharacter::EndAttack()
{
	if (CurrentActionState != EHikariActionState::Attacking) return;
	SetActionState(EHikariActionState::Normal);
}

void APlayerCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage) return;
	EndAttack();
}

void APlayerCharacter::CancelAttack()
{
	if (CurrentActionState != EHikariActionState::Attacking) return;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		AnimInstance->Montage_Stop(AttackCancelBlendOutTime, AttackMontage);
	SetActionState(EHikariActionState::Normal);
	UE_LOG(LogTemp, Log, TEXT("Hikari Attack Canceled"));
}

// ── 左键攻击（动画 + 伤害范围） ──

void APlayerCharacter::OnAttack()
{
	if (IsDead()) return;

	// 左键同时播放攻击动画并生成伤害范围
	BeginAttack();

	if (!AttackAreaClass) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AAttackArea_PlayerAttack1* AttackArea = GetWorld()->SpawnActor<AAttackArea_PlayerAttack1>(
			AttackAreaClass,
			GetActorLocation(),
			GetActorRotation(),
			Params))
	{
		AttackArea->Initialize(0.5f, 0.0f, true, this);
	}
}

// ── 受伤与死亡 ──

void APlayerCharacter::TakeDamage(const FTakeDamageInfo& InInfo)
{
	if (IsDead() || !HealthComponent || InInfo.DamageValue <= 0.0f)
	{
		return;
	}

	// 直接性伤害无敌：无敌期间直接跳过
	if (InInfo.bIsDirectDamage && IsInvincible()) return;

	HealthComponent->TakeDamage(InInfo);
}

void APlayerCharacter::TestDie()
{
	if (HealthComponent)
	{
		HealthComponent->Die();
	}
	else
	{
		Die();
	}
}

void APlayerCharacter::Die()
{
	if (bIsDead) return;
	bIsDead = true;
	bIsSprinting = false;
	OnPlayerDeath.Broadcast(this);

	// 通告玩家死亡（击杀者暂未知，传 nullptr）
	FEventBus::Publish<FPlayerDiedEvent>(FPlayerDiedEvent(nullptr, this));

	Destroy();
}

void APlayerCharacter::StartDashCooldown()
{
	bCanDash = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanDash = true;
	}), 0.6f, false);
}

void APlayerCharacter::StartAttack1Cooldown()
{
	bCanAttack1 = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanAttack1 = true;
	}), 0.3f, false);
}

void APlayerCharacter::StartAttack2Cooldown()
{
	bCanAttack2 = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanAttack2 = true;
	}), 0.3f, false);
}

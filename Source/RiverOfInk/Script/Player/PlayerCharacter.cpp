// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Common/AttackAreaBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Player/Skill/SkillComponent.h"
#include "Input/PlayerInputComponent.h"
#include "EnhancedInputComponent.h"
#include "Common/StateBase.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Player/PlayerState/PlayerState_Attack1.h"
#include "Player/PlayerState/PlayerState_Attack2.h"
#include "Player/PlayerState/PlayerState_Dash.h"
#include "Player/PlayerState/PlayerState_Skill1.h"
#include "Player/PlayerState/PlayerState_Skill2.h"
#include "UI/PlayerHealthWidget.h"
#include "Kismet/GameplayStatics.h"
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
	HealthWidgetClass = UPlayerHealthWidget::StaticClass();

	// ── 状态机与输入组件：纯 C++ 自建，无需蓝图挂载 ──
	CreateDefaultSubobject<UPlayerInputComponent>(TEXT("PlayerInputComponent"));
	CreateDefaultSubobject<UPlayerState_Idle>(TEXT("PlayerState_Idle"));
	CreateDefaultSubobject<UPlayerState_Move>(TEXT("PlayerState_Move"));
	CreateDefaultSubobject<UPlayerState_Attack1>(TEXT("PlayerState_Attack1"));
	CreateDefaultSubobject<UPlayerState_Attack2>(TEXT("PlayerState_Attack2"));
	CreateDefaultSubobject<UPlayerState_Dash>(TEXT("PlayerState_Dash"));
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

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// ── 默认进入 Idle 状态 ──
	// 自动从角色身上挂载的组件中查找 Idle 状态
	if (UPlayerState_Idle* Idle = FindComponentByClass<UPlayerState_Idle>())
	{
		SwitchState(UPlayerState_Idle::StaticClass());
	}

	CreateHealthWidget();

	// ── 玩家生成完毕 ──
	FEventBus::Publish<FPlayerSpawnedEvent>(FPlayerSpawnedEvent(this));
	FEventBus::Publish<FPlayerHealthChangedEvent>(
		FPlayerHealthChangedEvent(FMath::RoundToInt(MaxHealth), FMath::RoundToInt(CurrentHealth)));
}

void APlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasActorBegunPlay())
	{
		CreateHealthWidget();
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

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState)
	{
		CurrentState->Update(DeltaTime);
	}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateHealthWidget();

	// 通知 PlayerInputComponent 注册子系统和绑定回调（此时 Controller 和 InputComponent 均已就绪）
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (UPlayerInputComponent* InputComp = FindComponentByClass<UPlayerInputComponent>())
		{
			InputComp->SetupEnhancedInput(EnhancedInput, Cast<APlayerController>(GetController()));
		}
	}
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

void APlayerCharacter::BeginAttack()
{
	if (!CanStartAction()) return;
	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttackMontage is not set."));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance) return;

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
	if (bIsDead) return;

	// 左键同时播放攻击动画并生成伤害范围
	BeginAttack();

	if (!AttackAreaClass) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AAttackAreaBase* AttackArea = GetWorld()->SpawnActor<AAttackAreaBase>(
			AttackAreaClass,
			GetActorLocation() + GetActorForwardVector() * 100.0f,
			GetActorRotation(),
			Params))
	{
		AttackArea->Initialize(0.5f, 0.0f, true);
	}
}

// ── 受伤与死亡 ──

void APlayerCharacter::TakeDamage(const FTakeDamageInfo& InInfo)
{
	if (bIsDead || InInfo.DamageValue <= 0.0f) return;

	// 按伤害类型计算最终伤害（真实/必中伤害不减免）
	float FinalDamage = InInfo.DamageValue;
	switch (InInfo.DamageType)
	{
	case EDamageType::Physical:
		FinalDamage = FMath::Max(InInfo.DamageValue * 0.05f, InInfo.DamageValue - PhysicalResistance);
		break;
	case EDamageType::Magic:
		FinalDamage = FMath::Max(InInfo.DamageValue * 0.05f, (float)FMath::FloorToInt(InInfo.DamageValue * (1.0f - MagicResistance / 100.0f)));
		break;
	default:
		break;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - FinalDamage);
	UE_LOG(LogRiverOfInk, Warning, TEXT("Player took damage: -%.0f (Health left: %.0f)"), FinalDamage, CurrentHealth);

	if (CurrentHealth <= 0.0f)
	{
		if (InInfo.bCanCauseDeath)
		{
			Die();
		}
		else
		{
			CurrentHealth = 1.0f;
		}
	}
	// 通告血量变化
	FEventBus::Publish<FPlayerHealthChangedEvent>(
		FPlayerHealthChangedEvent(FMath::RoundToInt(MaxHealth), FMath::RoundToInt(CurrentHealth)));
}

void APlayerCharacter::TestDie()
{
	Die();
}

void APlayerCharacter::Die()
{
	if (bIsDead) return;
	bIsDead = true;
	bIsSprinting = false;
	CurrentHealth = 0.0f;
	FEventBus::Publish<FPlayerHealthChangedEvent>(FPlayerHealthChangedEvent(
		FMath::RoundToInt(MaxHealth), 0));
	OnPlayerDeath.Broadcast(this);

	// 通告玩家死亡（击杀者暂未知，传 nullptr）
	FEventBus::Publish<FPlayerDiedEvent>(FPlayerDiedEvent(nullptr, this));

	Destroy();
}

void APlayerCharacter::StartDashCooldown(float Time)
{
	bCanDash = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanDash = true;
	}), Time, false);
}

void APlayerCharacter::StartAttack1Cooldown(float Time)
{
	bCanAttack1 = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanAttack1 = true;
	}), Time, false);
}

void APlayerCharacter::StartAttack2Cooldown(float Time)
{
	bCanAttack2 = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanAttack2 = true;
	}), Time, false);
}

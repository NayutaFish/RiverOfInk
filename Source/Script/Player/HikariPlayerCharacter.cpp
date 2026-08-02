// Fill out your copyright notice in the Description page of Project Settings.

#include "HikariPlayerCharacter.h"
#include "Test_GamePlay.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Common/AttackAreaBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Skill/HikariSkillComponent.h"
#include "Input/PlayerInputComponent.h"
#include "EnhancedInputComponent.h"
#include "Common/StateBase.h"
#include "PlayerState/PlayerState_Idle.h"
#include "UI/PlayerHealthWidget.h"
#include "Kismet/GameplayStatics.h"

AHikariPlayerCharacter::AHikariPlayerCharacter()
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

	SkillComponent = CreateDefaultSubobject<UHikariSkillComponent>(TEXT("SkillComponent"));
	HealthWidgetClass = UPlayerHealthWidget::StaticClass();
}

void AHikariPlayerCharacter::BeginPlay()
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

void AHikariPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasActorBegunPlay())
	{
		CreateHealthWidget();
	}
}

void AHikariPlayerCharacter::CreateHealthWidget()
{
	if (HealthWidget)
	{
		return;
	}

	if (!IsLocallyControlled())
	{
		UE_LOG(LogTest_GamePlay, Verbose, TEXT("Health HUD skipped: %s is not locally controlled yet."), *GetName());
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	}

	if (!PlayerController)
	{
		UE_LOG(LogTest_GamePlay, Warning, TEXT("Health HUD skipped: no local PlayerController for %s."), *GetName());
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
		UE_LOG(LogTest_GamePlay, Error, TEXT("Health HUD creation failed for %s."), *GetName());
		return;
	}

	HealthWidget->AddToViewport(10);
	HealthWidget->InitializeForPlayer(this);
	UE_LOG(LogTest_GamePlay, Log, TEXT("Health HUD created for %s."), *GetName());
}

void AHikariPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState)
	{
		CurrentState->Update(DeltaTime);
	}
}

void AHikariPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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

void AHikariPlayerCharacter::TryCastSkill1()
{
	TryCastSkillSlot1();
}

void AHikariPlayerCharacter::TryCastSkill2()
{
	TryCastSkillSlot2();
}

void AHikariPlayerCharacter::TryCastSkillSlot1()
{
	if (SkillComponent)
	{
		SkillComponent->TryCastSkillSlot(0);
	}
}

void AHikariPlayerCharacter::TryCastSkillSlot2()
{
	if (SkillComponent)
	{
		SkillComponent->TryCastSkillSlot(1);
	}
}

void AHikariPlayerCharacter::TryCastSkillSlot3()
{
	if (SkillComponent)
	{
		SkillComponent->TryCastSkillSlot(2);
	}
}

// ── 状态机 ──

void AHikariPlayerCharacter::SwitchState(TSubclassOf<UStateBase> StateClass)
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

bool AHikariPlayerCharacter::CanMove() const
{
	return CurrentActionState == EHikariActionState::Normal;
}

bool AHikariPlayerCharacter::CanStartAction() const
{
	return CurrentActionState == EHikariActionState::Normal;
}

void AHikariPlayerCharacter::SetActionState(EHikariActionState NewState)
{
	if (CurrentActionState == NewState) return;
	CurrentActionState = NewState;
}

// ── 攻击动画（左键） ──

void AHikariPlayerCharacter::BeginAttack()
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
	Delegate.BindUObject(this, &AHikariPlayerCharacter::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(Delegate, AttackMontage);
}

void AHikariPlayerCharacter::EndAttack()
{
	if (CurrentActionState != EHikariActionState::Attacking) return;
	SetActionState(EHikariActionState::Normal);
}

void AHikariPlayerCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage) return;
	EndAttack();
}

void AHikariPlayerCharacter::CancelAttack()
{
	if (CurrentActionState != EHikariActionState::Attacking) return;
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		AnimInstance->Montage_Stop(AttackCancelBlendOutTime, AttackMontage);
	SetActionState(EHikariActionState::Normal);
	UE_LOG(LogTemp, Log, TEXT("Hikari Attack Canceled"));
}

// ── 左键攻击（动画 + 伤害范围） ──

void AHikariPlayerCharacter::OnAttack()
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

void AHikariPlayerCharacter::TakeDamage(const FTakeDamageInfo& InInfo)
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
	UE_LOG(LogTest_GamePlay, Warning, TEXT("Player took damage: -%.0f (Health left: %.0f)"), FinalDamage, CurrentHealth);

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

void AHikariPlayerCharacter::TestDie()
{
	Die();
}

void AHikariPlayerCharacter::Die()
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

void AHikariPlayerCharacter::StartDashCooldown(float Time)
{
	bCanDash = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanDash = true;
	}), Time, false);
}

void AHikariPlayerCharacter::StartAttack1Cooldown(float Time)
{
	bCanAttack1 = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanAttack1 = true;
	}), Time, false);
}

void AHikariPlayerCharacter::StartAttack2Cooldown(float Time)
{
	bCanAttack2 = false;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bCanAttack2 = true;
	}), Time, false);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/EnemyBase.h"

#include "Common/AttackAreaBase.h"
#include "Common/CombatEffectComponent.h"
#include "Common/CombatEffectTags.h"
#include "Common/StateBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/CombatDamageCalculator.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"
#include "UI/EnemyHealthWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Charge.h"
#include "TimerManager.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Dead.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Idle.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// 胶囊体同时作为根组件
	CapsuleCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleCollision"));
	RootComponent = CapsuleCollision;
	CapsuleCollision->SetCapsuleHalfHeight(50.0f);
	CapsuleCollision->SetCapsuleRadius(40.0f);
	CapsuleCollision->SetCollisionObjectType(ECC_GameTraceChannel2);    // EnemyHitbox
	CapsuleCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); // 响应伤害
	CapsuleCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CapsuleCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	CombatEffectComponent = CreateDefaultSubobject<UCombatEffectComponent>(TEXT("CombatEffectComponent"));

	// 网格（仅显示用，碰撞走胶囊体）
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(CapsuleCollision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HealthWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidgetComponent"));
	HealthWidgetComponent->SetupAttachment(CapsuleCollision);
	HealthWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidgetComponent->SetDrawSize(HealthWidgetDrawSize);
	HealthWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
	HealthWidgetComponent->SetRelativeScale3D(FVector(HealthWidgetWorldScale));
	HealthWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidgetComponent->SetTwoSided(true);
	HealthWidgetComponent->SetVisibility(false);
	static ConstructorHelpers::FClassFinder<UEnemyHealthWidget> EnemyHealthWidgetClassFinder(
		TEXT("/Game/Blueprint/GamePlay/Enemy/WBP_EnemyHealth"));
	if (EnemyHealthWidgetClassFinder.Succeeded())
	{
		HealthWidgetClass = EnemyHealthWidgetClassFinder.Class;
	}
	else
	{
		HealthWidgetClass = UEnemyHealthWidget::StaticClass();
	}
	HealthWidgetComponent->SetWidgetClass(HealthWidgetClass);
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	NormalizeDefenseFromLegacy();
	CurrentHealth = MaxHealth;
	CurrentHardValue = FMath::Max(0.0f, MaxHardValue);
	HardValueRecoveryDelayRemaining = 0.0f;
	HardBreakCooldownRemaining = 0.0f;
	bIsDead = false;
	bDeadHandled = false;
	if (HealthWidgetComponent)
	{
		HealthWidgetComponent->SetRelativeLocation(FVector(
			0.0f,
			HealthWidgetHorizontalOffset,
			CapsuleCollision->GetScaledCapsuleHalfHeight() + HealthWidgetHeightOffset));
		HealthWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
		TSubclassOf<UEnemyHealthWidget> WidgetClass = HealthWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UEnemyHealthWidget::StaticClass();
		}
		const FVector2D WidgetDrawSize = EnemyRank == EEnemyRank::Elite
			? EliteHealthWidgetDrawSize
			: HealthWidgetDrawSize;
		HealthWidgetComponent->SetDrawSize(WidgetDrawSize);
		HealthWidgetComponent->SetRelativeScale3D(FVector(HealthWidgetWorldScale));
		HealthWidgetComponent->SetWidgetClass(WidgetClass);
		HealthWidgetComponent->InitWidget();

		if (UEnemyHealthWidget* HealthWidget = Cast<UEnemyHealthWidget>(
			HealthWidgetComponent->GetUserWidgetObject()))
		{
			HealthWidget->InitializeForEnemy(this);
		}
		HealthWidgetComponent->SetVisibility(true);

		UE_LOG(LogRiverOfInk, Log,
			TEXT("Enemy %s health widget initialized: Widget=%s Space=Screen Rank=%d Size=(%.0f,%.0f) ComponentVisible=%s."),
			*GetName(),
			*GetNameSafe(HealthWidgetComponent->GetUserWidgetObject()),
			static_cast<int32>(EnemyRank),
			WidgetDrawSize.X,
			WidgetDrawSize.Y,
			HealthWidgetComponent->IsVisible() ? TEXT("true") : TEXT("false"));
	}
	BroadcastHealthChanged(CurrentHealth, EEnemyHealthChangeReason::Initialize);
	RefreshCombatTarget();
	EnsureStateComponent(UEnemyState_Charge::StaticClass());
	EnsureStateComponent(UEnemyState_TargetLost::StaticClass());
	EnsureStateComponent(UEnemyState_Dead::StaticClass());
	DisableStateComponentTicks();

	// 显式进入初始 Idle 状态（敌人蓝图需挂载 EnemyState_Idle 组件）
	SwitchState(UEnemyState_Idle::StaticClass());
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDead)
	{
		return;
	}

	UpdateHardValue(DeltaTime);
	RefreshCombatTarget();
	if (CurrentState)
	{
		CurrentState->Update(DeltaTime);
	}
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CurrentState.Get()))
	{
		CurrentState->OnExit();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
		World->GetTimerManager().ClearTimer(DeathDestroyTimerHandle);
	}

	CurrentState = nullptr;
	CachedPlayer = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AEnemyBase::SwitchState(TSubclassOf<UStateBase> StateClass)
{
	if (!StateClass)
	{
		return;
	}

	const bool bEnteringDeadState = StateClass->IsChildOf(UEnemyState_Dead::StaticClass());
	if (bIsDead && !bEnteringDeadState)
	{
		return;
	}

	TArray<UStateBase*> Found;
	GetComponents(StateClass, Found);
	if (Found.Num() == 0)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s cannot enter state %s: component is missing."),
			*GetName(), *StateClass->GetName());
		return;
	}

	UStateBase* NewState = Found[0];
	if (!IsValid(NewState) || NewState == CurrentState)
	{
		return;
	}

	const FString PreviousState = CurrentState
		? CurrentState->GetClass()->GetName()
		: TEXT("None");
	const FString NextState = NewState->GetClass()->GetName();

	if (CurrentState)
	{
		CurrentState->OnExit();
	}

	CurrentState = NewState;
	CurrentState->OnEnter();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Enemy %s state: %s -> %s."),
		*GetName(), *PreviousState, *NextState);
}

void AEnemyBase::TakeDamage(const FTakeDamageInfo& InInfo, AAttackAreaBase* InAttackArea)
{
	TakeDamageContext(FDamageContext(InInfo), InAttackArea);
}

void AEnemyBase::TakeDamageContext(const FDamageContext& InContext, AAttackAreaBase* InAttackArea)
{
	if (bIsDead)
	{
		return;
	}

	FDamageContext Context = InContext;
	Context.TargetActor = this;
	if (Context.BaseDamage <= 0.0f)
	{
		FDamageResult ZeroDamageResult;
		ZeroDamageResult.Context = Context;
		ZeroDamageResult.bNoDamage = true;
		FEnemyDamageResult DamageResult;
		DamageResult.ResolvedDamage = ZeroDamageResult;
		LastDamageResult = DamageResult;
		OnDamageResolved.Broadcast(ZeroDamageResult);
		return;
	}

	const UCombatEffectComponent* TargetEffects = CombatEffectComponent;
	const UCombatEffectComponent* SourceEffects = Context.SourceActor
		? Context.SourceActor->FindComponentByClass<UCombatEffectComponent>()
		: nullptr;

	// Invulnerability is checked before consuming a proc: a blocked hit is not
	// a valid "next hit" for a proc effect.
	if (TargetEffects
		&& !Context.bIgnoreInvulnerability
		&& TargetEffects->IsInvulnerable())
	{
		const FDamageResult BlockedResult = RiverOfInkDamage::ResolveDamage(
			Context,
			SourceEffects,
			TargetEffects,
			static_cast<float>(Defense));
		FEnemyDamageResult DamageResult;
		DamageResult.ResolvedDamage = BlockedResult;
		DamageResult.DamageInfo = Context.ToLegacyDamageInfo();
		LastDamageResult = DamageResult;
		OnDamageResolved.Broadcast(BlockedResult);
		UE_LOG(LogRiverOfInk, Verbose,
			TEXT("Enemy %s damage blocked by invulnerability: Source=%s."),
			*GetName(),
			*GetNameSafe(Context.SourceActor));
		return;
	}

	// Attack2's proc is attached to the enemy and consumed only after the
	// current hit has passed the invulnerability gate, so the application hit
	// itself never triggers its own bonus.
	if (CombatEffectComponent)
	{
		FTakeDamageInfo BonusInfo;
		if (CombatEffectComponent->ConsumeNextHitBonusDamage(BonusInfo))
		{
			Context.BaseDamage += BonusInfo.DamageValue;
			Context.HardDamage += BonusInfo.HardDamageValue;
			Context.bCanCauseDeath = Context.bCanCauseDeath || BonusInfo.bCanCauseDeath;
			Context.bIsDirectDamage = Context.bIsDirectDamage || BonusInfo.bIsDirectDamage;
			Context.bIgnoreInvulnerability = Context.bIgnoreInvulnerability || BonusInfo.bIgnoreInvincible;
		}
	}

	const FDamageResult ResolvedDamage = RiverOfInkDamage::ResolveDamage(
		Context,
		SourceEffects,
		TargetEffects,
		static_cast<float>(Defense));
	OnDamageResolved.Broadcast(ResolvedDamage);

	FEnemyDamageResult DamageResult;
	DamageResult.ResolvedDamage = ResolvedDamage;
	DamageResult.DamageInfo = Context.ToLegacyDamageInfo();
	if (!ResolvedDamage.bDamageApplied)
	{
		LastDamageResult = DamageResult;
		return;
	}

	// Cache the resolved request for death events and hard-value reactions.
	FTakeDamageInfo EffectiveInfo = Context.ToLegacyDamageInfo();
	EffectiveInfo.DamageValue = ResolvedDamage.ModifiedDamage;
	LastDamageInfo = EffectiveInfo;
	LastAttackArea = InAttackArea;

	const float PreviousHealth = CurrentHealth;
	const float HardValueBefore = CurrentHardValue;
	const bool bHardBreakSuppressed = HardBreakCooldownRemaining > KINDA_SMALL_NUMBER;
	FTakeDamageInfo HardDamageInfo = Context.ToLegacyDamageInfo();
	// Damage multipliers intentionally do not alter hard value unless the
	// effect supplied an explicit HardDamage payload.
	HardDamageInfo.DamageValue = Context.BaseDamage;
	float HardDamage = !bHardBreakSuppressed
		? ResolveHardDamage(HardDamageInfo)
		: 0.0f;
	if (CombatEffectComponent)
	{
		HardDamage *= CombatEffectComponent->GetControlResistMultiplier();
	}

	if (HardDamage > KINDA_SMALL_NUMBER)
	{
		CurrentHardValue = FMath::Max(0.0f, CurrentHardValue - HardDamage);
		HardValueRecoveryDelayRemaining = HardValueRecoveryDelay;
	}

	const bool bHardBreak = !bHardBreakSuppressed
		&& MaxHardValue > KINDA_SMALL_NUMBER
		&& HardDamage > KINDA_SMALL_NUMBER
		&& HardValueBefore > KINDA_SMALL_NUMBER
		&& CurrentHardValue <= KINDA_SMALL_NUMBER;

	// Reset the resource for the next reaction window. The resolved result
	// still reports HardValueAfter=0 so VFX/UI can identify the break.
	if (bHardBreak)
	{
		CurrentHardValue = FMath::Max(0.0f, MaxHardValue);
		HardBreakCooldownRemaining = HardBreakCooldown;
		HardValueRecoveryDelayRemaining = HardValueRecoveryDelay;
	}

	// DamageType remains legacy metadata; all damage uses one defense formula.
	const int32 FinalDamage = ResolvedDamage.FinalDamage;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - static_cast<float>(FinalDamage));

	DamageResult.DamageInfo = EffectiveInfo;
	DamageResult.FinalDamage = FinalDamage;
	DamageResult.HardValueBefore = HardValueBefore;
	DamageResult.HardValueAfter = bHardBreak ? 0.0f : CurrentHardValue;
	DamageResult.HardDamageApplied = HardDamage;
	DamageResult.bHardBreak = bHardBreak;
	DamageResult.bKilled = CurrentHealth <= 0.0f && Context.bCanCauseDeath;
	LastDamageResult = DamageResult;

	const bool bWillEnterDeadState = CurrentHealth <= 0.0f && Context.bCanCauseDeath;
	if (CurrentHealth <= 0.0f && !bWillEnterDeadState)
	{
		CurrentHealth = 1.0f;
	}

	if (FinalDamage > 0)
	{
		if (HealthWidgetComponent)
		{
			HealthWidgetComponent->SetVisibility(true);
		}
		BroadcastHealthChanged(PreviousHealth, EEnemyHealthChangeReason::Damage);

		UE_LOG(LogRiverOfInk, Log,
			TEXT("Enemy %s health widget damage refresh: ComponentVisible=%s WidgetVisible=%s Health=%.1f/%.1f."),
			*GetName(),
			HealthWidgetComponent && HealthWidgetComponent->IsVisible() ? TEXT("true") : TEXT("false"),
			HealthWidgetComponent && HealthWidgetComponent->IsWidgetVisible() ? TEXT("true") : TEXT("false"),
			CurrentHealth,
			MaxHealth);
	}

	// Keep this raw damage event for existing systems such as the temporary
	// bonus-damage buff. State interruption is handled by OnHardBreak below.
	if (Context.bIsDirectDamage)
	{
		LastAttacker = Context.SourceActor;
		OnTakeDirectDamage.Broadcast(EffectiveInfo);
	}

	UE_LOG(
		LogRiverOfInk,
		Log,
		TEXT("Enemy %s took %d damage with Defense=%d. CurrentHealth=%.1f HardValue=%.1f/%.1f HardDamage=%.1f HardBreak=%s."),
		*GetName(),
		FinalDamage,
		Defense,
		CurrentHealth,
		CurrentHardValue,
		MaxHardValue,
		HardDamage,
		bHardBreak ? TEXT("true") : TEXT("false")
	);

	if (CurrentHealth <= 0.0f)
	{
		if (Context.bCanCauseDeath)
		{
			Die();
		}
	}

	// A nested damage listener may have killed the enemy or already caused a
	// reaction. Death always wins over a hard-break response.
	if (!bIsDead && bHardBreak)
	{
		OnHardBreak.Broadcast(DamageResult);
	}
}

float AEnemyBase::GetEffectiveMoveSpeed(float BaseSpeed) const
{
	const float SafeBaseSpeed = FMath::Max(0.0f, BaseSpeed);
	return SafeBaseSpeed * (CombatEffectComponent
		? CombatEffectComponent->GetMoveSpeedMultiplier()
		: 1.0f);
}

float AEnemyBase::GetControlResistMultiplier() const
{
	return CombatEffectComponent
		? CombatEffectComponent->GetControlResistMultiplier()
		: 1.0f;
}

void AEnemyBase::TestDie()
{
	Die();
}

void AEnemyBase::Heal(float Amount)
{
	if (bIsDead || Amount <= 0.0f || CurrentHealth >= MaxHealth)
	{
		return;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	if (CurrentHealth > PreviousHealth + KINDA_SMALL_NUMBER)
	{
		BroadcastHealthChanged(PreviousHealth, EEnemyHealthChangeReason::Heal);
	}
}

void AEnemyBase::Die()
{
	if (bIsDead)
	{
		return;
	}

	const float PreviousHealth = CurrentHealth;
	bIsDead = true;
	CurrentHealth = 0.0f;
	if (CombatEffectComponent)
	{
		TArray<FCombatEffectHandle> HomingMarkHandles;
		for (const FActiveCombatEffect& ActiveEffect : CombatEffectComponent->ActiveEffects)
		{
			if (ActiveEffect.Spec.EffectTag == RiverOfInkCombatEffectTags::Effect_Debuff_HomingMark)
			{
				HomingMarkHandles.Add(ActiveEffect.Handle);
			}
		}
		for (const FCombatEffectHandle Handle : HomingMarkHandles)
		{
			CombatEffectComponent->RemoveEffect(Handle);
		}
	}
	if (PreviousHealth > KINDA_SMALL_NUMBER)
	{
		BroadcastHealthChanged(PreviousHealth, EEnemyHealthChangeReason::Death);
	}
	if (HealthWidgetComponent)
	{
		// Keep the zero-health bar visible through the existing death window;
		// actor destruction owns the final widget teardown.
		HealthWidgetComponent->SetVisibility(true);
	}
	if (CapsuleCollision)
	{
		CapsuleCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Enemy %s entered Dead state; destroy delay=%.2fs."),
		*GetName(),
		DeathDestroyDelay);

	// Dead component is created in BeginPlay. SwitchState owns the one-shot
	// notification and keeps Die() as a small state transition request.
	SwitchState(UEnemyState_Dead::StaticClass());
	if (!bDeadHandled)
	{
		// Fallback for an unusual lifecycle where the state component could not
		// be registered before Die() was called.
		HandleDeadState();
	}
}

void AEnemyBase::HandleDeadState()
{
	if (bDeadHandled)
	{
		return;
	}
	bDeadHandled = true;

	GenerateDropOnDead();
	OnDead.Broadcast(this);
	OnEnemyDeath.Broadcast(this);

	// 通告敌人死亡（携带致死伤害信息与来源攻击区域）。经济子系统在此处
	// 完成 EnemyDrop 交易；Dead 状态本身不直接依赖经济模块。
	FEventBus::Publish<FNonPlayerDiedEvent>(FNonPlayerDiedEvent(this, LastDamageInfo, LastAttackArea));

	if (DeathDestroyDelay <= KINDA_SMALL_NUMBER)
	{
		DestroyAfterDeath();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DeathDestroyTimerHandle,
			this,
			&AEnemyBase::DestroyAfterDeath,
			DeathDestroyDelay,
			false);
	}
}

void AEnemyBase::GenerateDropOnDead()
{
	UE_LOG(LogRoguelike, Log,
		TEXT("Enemy drop generated on OnDead: Enemy=%s Amount=%d."),
		*GetName(),
		PureInkDropAmount);
}

void AEnemyBase::DestroyAfterDeath()
{
	if (!IsValid(this))
	{
		return;
	}

	UE_LOG(LogRiverOfInk, Verbose, TEXT("Enemy %s destroyed after Dead delay."), *GetName());
	Destroy();
}

void AEnemyBase::NormalizeDefenseFromLegacy()
{
	Defense = RiverOfInkDamage::ResolveLegacyDefense(Defense, PhysicalResistance, MagicResistance);
	PhysicalResistance = Defense;
	MagicResistance = Defense;
}

void AEnemyBase::BroadcastHealthChanged(float PreviousHealth, EEnemyHealthChangeReason ChangeReason)
{
	const float SafeMaxHealth = FMath::Max(1.0f, MaxHealth);
	const float SafePreviousHealth = FMath::Clamp(PreviousHealth, 0.0f, SafeMaxHealth);
	const float SafeCurrentHealth = FMath::Clamp(CurrentHealth, 0.0f, SafeMaxHealth);
	OnEnemyHealthChanged.Broadcast(
		SafePreviousHealth,
		SafeCurrentHealth,
		SafeMaxHealth,
		ChangeReason);
}

void AEnemyBase::UpdateHardValue(float DeltaTime)
{
	if (MaxHardValue <= KINDA_SMALL_NUMBER || bIsDead)
	{
		return;
	}

	if (HardBreakCooldownRemaining > 0.0f)
	{
		HardBreakCooldownRemaining = FMath::Max(0.0f, HardBreakCooldownRemaining - DeltaTime);
		return;
	}

	if (HardValueRecoveryDelayRemaining > 0.0f)
	{
		HardValueRecoveryDelayRemaining = FMath::Max(0.0f, HardValueRecoveryDelayRemaining - DeltaTime);
		return;
	}

	if (HardValueRecoveryRate > KINDA_SMALL_NUMBER && CurrentHardValue < MaxHardValue)
	{
		CurrentHardValue = FMath::Min(
			MaxHardValue,
			CurrentHardValue + HardValueRecoveryRate * DeltaTime);
	}
}

float AEnemyBase::ResolveHardDamage(const FTakeDamageInfo& InInfo) const
{
	if (!InInfo.bIsDirectDamage || MaxHardValue <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	if (InInfo.HardDamageValue > KINDA_SMALL_NUMBER)
	{
		return InInfo.HardDamageValue;
	}

	return FMath::Max(0.0f, InInfo.DamageValue * DefaultHardDamageScale);
}

void AEnemyBase::RefreshCombatTarget()
{
	if (IsValid(CachedPlayer.Get()) && !CachedPlayer->IsDead())
	{
		return;
	}

	CachedPlayer = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (IsValid(CachedPlayer.Get()) && CachedPlayer->IsDead())
	{
		CachedPlayer = nullptr;
	}
}

bool AEnemyBase::HasValidCombatTarget() const
{
	return IsValid(CachedPlayer.Get()) && !CachedPlayer->IsDead();
}

void AEnemyBase::DisableStateComponentTicks()
{
	TArray<UStateBase*> States;
	GetComponents<UStateBase>(States);

	for (UStateBase* State : States)
	{
		if (IsValid(State))
		{
			// AEnemyBase is the single state update driver. Disable the native
			// component tick to prevent a second update path.
			State->SetComponentTickEnabled(false);
		}
	}
}

UStateBase* AEnemyBase::EnsureStateComponent(TSubclassOf<UStateBase> StateClass)
{
	if (!StateClass)
	{
		return nullptr;
	}

	TArray<UStateBase*> Found;
	GetComponents(StateClass, Found);
	if (Found.Num() > 0 && IsValid(Found[0]))
	{
		return Found[0];
	}

	UStateBase* NewState = NewObject<UStateBase>(this, StateClass, NAME_None, RF_Transient);
	if (!IsValid(NewState))
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s could not create runtime state component %s."),
			*GetName(),
			*StateClass->GetName());
		return nullptr;
	}

	AddInstanceComponent(NewState);
	NewState->RegisterComponent();
	UE_LOG(LogRiverOfInk, Verbose,
		TEXT("Enemy %s created runtime state component %s."),
		*GetName(),
		*StateClass->GetName());
	return NewState;
}

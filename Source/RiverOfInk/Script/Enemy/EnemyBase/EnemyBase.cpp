// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/EnemyBase.h"

#include "Common/AttackAreaBase.h"
#include "Common/StateBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CombatDamageCalculator.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"
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

	// 网格（仅显示用，碰撞走胶囊体）
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(CapsuleCollision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	NormalizeDefenseFromLegacy();
	CurrentHealth = MaxHealth;
	bIsDead = false;
	bDeadHandled = false;
	RefreshCombatTarget();
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
	if (bIsDead || InInfo.DamageValue <= 0.0f)
	{
		return;
	}

	// 缓存最近一次伤害信息与来源（供死亡事件携带）
	LastDamageInfo = InInfo;
	LastAttackArea = InAttackArea;

	// 直接性伤害通报（供状态类订阅，如击退）
	if (InInfo.bIsDirectDamage)
	{
		LastAttacker = InInfo.Attacker;
		OnTakeDirectDamage.Broadcast(InInfo);
	}

	// DamageType remains legacy metadata; all damage uses one defense formula.
	const int32 FinalDamage = RiverOfInkDamage::CalculateFinalDamage(InInfo.DamageValue, Defense);

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - static_cast<float>(FinalDamage));

	UE_LOG(
		LogRiverOfInk,
		Log,
		TEXT("Enemy %s took %d damage with Defense=%d. CurrentHealth = %.1f"),
		*GetName(),
		FinalDamage,
		Defense,
		CurrentHealth
	);

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
}

void AEnemyBase::TestDie()
{
	Die();
}

void AEnemyBase::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	CurrentHealth = 0.0f;
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

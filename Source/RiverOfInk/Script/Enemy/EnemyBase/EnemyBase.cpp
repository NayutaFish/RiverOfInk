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
#include "RiverOfInk.h"
#include "TimerManager.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Idle.h"

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

	// 显式进入初始 Idle 状态（敌人蓝图需挂载 EnemyState_Idle 组件）
	SwitchState(UEnemyState_Idle::StaticClass());
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentState)
	{
		CurrentState->Update(DeltaTime);
	}
}

void AEnemyBase::SwitchState(TSubclassOf<UStateBase> StateClass)
{
	if (!StateClass) return;

	TArray<UStateBase*> Found;
	GetComponents(StateClass, Found);
	if (Found.Num() == 0) return;

	UStateBase* NewState = Found[0];
	if (NewState == CurrentState) return;

	if (CurrentState)
	{
		CurrentState->OnExit();
	}

	CurrentState = NewState;
	CurrentState->OnEnter();
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

	UE_LOG(LogRiverOfInk, Log, TEXT("Enemy %s died."), *GetName());

	OnEnemyDeath.Broadcast(this);

	// 通告敌人死亡（携带致死伤害信息与来源攻击区域）
	FEventBus::Publish<FNonPlayerDiedEvent>(FNonPlayerDiedEvent(this, LastDamageInfo, LastAttackArea));

	Destroy();
}

void AEnemyBase::NormalizeDefenseFromLegacy()
{
	Defense = RiverOfInkDamage::ResolveLegacyDefense(Defense, PhysicalResistance, MagicResistance);
	PhysicalResistance = Defense;
	MagicResistance = Defense;
}

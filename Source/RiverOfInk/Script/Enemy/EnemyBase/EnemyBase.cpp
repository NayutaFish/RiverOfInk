// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/EnemyBase.h"

#include "Common/AttackAreaBase.h"
#include "Common/StateBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
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

	UE_LOG(
		LogRiverOfInk,
		Log,
		TEXT("Enemy %s took %.1f damage. CurrentHealth = %.1f"),
		*GetName(),
		FinalDamage,
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

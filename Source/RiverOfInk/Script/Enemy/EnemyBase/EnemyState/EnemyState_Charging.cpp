// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Charging.h"

#include "Enemy/EnemyBase/EnemyBase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RiverOfInk.h"
#include "Engine/World.h"
#include "TimerManager.h"

UEnemyState_Charging::UEnemyState_Charging()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyState_Charging::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy)
	{
		return;
	}

	// 生成蓄力特效，并写入 durationTime 用户参数（与状态持续时间保持一致）
	if (ChargeNiagaraSystem)
	{
		ChargeNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ChargeNiagaraSystem,
			Enemy->GetActorLocation(),
			Enemy->GetActorRotation());
		if (ChargeNiagaraComponent)
		{
			ChargeNiagaraComponent->SetVariableFloat(TEXT("durationTime"), DurationTime);
		}
	}

	// 倒计时结束跳转目标状态
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeFinishTimerHandle);
		// 同样注意 rate <= 0 会被 SetTimer 丢弃：DurationTime=0 表示立刻结束蓄力。
		World->GetTimerManager().SetTimer(
			ChargeFinishTimerHandle,
			this,
			&UEnemyState_Charging::FinishCharging,
			FMath::Max(DurationTime, KINDA_SMALL_NUMBER),
			false);
	}
}

void UEnemyState_Charging::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeFinishTimerHandle);
	}

	// 确保蓄力特效完全删除
	if (ChargeNiagaraComponent)
	{
		ChargeNiagaraComponent->Deactivate();
		ChargeNiagaraComponent->DestroyComponent();
		ChargeNiagaraComponent = nullptr;
	}
}

void UEnemyState_Charging::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy)
	{
		return;
	}

	// 每帧保持特效与敌人宿主的位置、朝向一致
	if (ChargeNiagaraComponent)
	{
		ChargeNiagaraComponent->SetWorldLocation(Enemy->GetActorLocation());
		ChargeNiagaraComponent->SetWorldRotation(Enemy->GetActorRotation());
	}
}

void UEnemyState_Charging::FinishCharging()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy)
	{
		return;
	}

	// 蓄力期间死亡：Die() 已自动切到死亡状态，这里不再跳转目标状态
	if (Enemy->bIsDead)
	{
		return;
	}

	if (!TargetStateClass)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s charging finished but TargetStateClass is not set; returning to Chase."),
			*Enemy->GetName());
		FallBackToChase(Enemy);
		return;
	}

	// SwitchState 只查找已挂载的状态组件；目标状态组件不在该敌人身上时，
	// 直接调用会只留下一条警告并让敌人卡死在蓄力状态，所以这里主动回退。
	if (!Enemy->FindComponentByClass(TargetStateClass))
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s charging finished but state component %s is missing on this enemy (add it in the enemy Blueprint); returning to Chase."),
			*Enemy->GetName(),
			*TargetStateClass->GetName());
		FallBackToChase(Enemy);
		return;
	}

	Enemy->SwitchState(TargetStateClass);
}

void UEnemyState_Charging::FallBackToChase(AEnemyBase* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	Enemy->RefreshCombatTarget();
	Enemy->SwitchState(Enemy->HasValidCombatTarget()
		? UEnemyState_Chase::StaticClass()
		: UEnemyState_TargetLost::StaticClass());
}

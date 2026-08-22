// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Charging.h"

#include "Enemy/EnemyBase/EnemyBase.h"
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
		World->GetTimerManager().SetTimer(
			ChargeFinishTimerHandle,
			this,
			&UEnemyState_Charging::FinishCharging,
			FMath::Max(0.0f, DurationTime),
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

	if (TargetStateClass)
	{
		Enemy->SwitchState(TargetStateClass);
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s charging finished but TargetStateClass is not set."),
			*Enemy->GetName());
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerCharacter_CommonAttackManage.h"

#include "Engine/World.h"
#include "Player/PlayerCharacter.h"
#include "Player/PlayerState/PlayerState_Attack1.h"

UPlayerCharacter_CommonAttackManage::UPlayerCharacter_CommonAttackManage()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerCharacter_CommonAttackManage::BeginPlay()
{
	Super::BeginPlay();

	OwnerPlayer = Cast<APlayerCharacter>(GetOwner());
	if (!OwnerPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerCharacter_CommonAttackManage requires an APlayerCharacter owner."));
		return;
	}

	// 收集玩家身上所有 PlayerState_Attack1 组件，并按 attackStage 升序排列。
	TArray<UPlayerState_Attack1*> Found;
	OwnerPlayer->GetComponents<UPlayerState_Attack1>(Found);
	for (UPlayerState_Attack1* Stage : Found)
	{
		if (Stage)
		{
			AttackStages.Add(Stage);
		}
	}

	AttackStages.Sort([](const UPlayerState_Attack1& A, const UPlayerState_Attack1& B)
	{
		return A.GetAttackStage() < B.GetAttackStage();
	});

	UE_LOG(LogTemp, Log, TEXT("PlayerCharacter_CommonAttackManage collected %d attack stages."), AttackStages.Num());
}

void UPlayerCharacter_CommonAttackManage::RequestNormalAttack()
{
	if (!OwnerPlayer || AttackStages.Num() == 0)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bWithinWindow = LastAttackTime >= 0.0
		&& (Now - LastAttackTime) <= static_cast<double>(AttackStages[CurrentAttackStageIndex]->maxAttackInterval);

	int32 NextIndex = 0;
	if (bWithinWindow)
	{
		NextIndex = (CurrentAttackStageIndex + 1) % AttackStages.Num();
	}
	else
	{
		NextIndex = 0;
	}

	CurrentAttackStageIndex = NextIndex;
	LastAttackTime = Now;

	UPlayerState_Attack1* TargetStage = AttackStages[CurrentAttackStageIndex];
	if (!TargetStage)
	{
		return;
	}

	// 记录当前窗口内的连续攻击段数（1-based），供调试 / HUD 使用。
	TargetStage->SetAttackStageCount(CurrentAttackStageIndex + 1);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResetTimerHandle);
		World->GetTimerManager().SetTimer(
			ResetTimerHandle,
			this,
			&UPlayerCharacter_CommonAttackManage::OnAttackIntervalExpired,
			FMath::Max(0.0f, TargetStage->maxAttackInterval),
			false);
	}

	UE_LOG(LogTemp, Log,
		TEXT("CommonAttack routed to stage %d (index %d)."),
		TargetStage->GetAttackStage(),
		CurrentAttackStageIndex);

	OwnerPlayer->SwitchToState(TargetStage);
}

void UPlayerCharacter_CommonAttackManage::OnAttackIntervalExpired()
{
	CurrentAttackStageIndex = 0;
	LastAttackTime = -1.0;

	for (const TObjectPtr<UPlayerState_Attack1>& Stage : AttackStages)
	{
		if (Stage)
		{
			Stage->ResetAttackStageCount();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("CommonAttack interval expired; attack stage reset to 1."));
}

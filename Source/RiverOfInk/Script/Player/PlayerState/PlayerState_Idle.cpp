// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_Idle.h"
#include "RiverOfInk.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Player/PlayerState/PlayerState_Attack1.h"
#include "Player/PlayerState/PlayerState_Attack2.h"
#include "Player/PlayerState/PlayerState_HitBack.h"
#include "Player/PlayerState/PlayerState_Skill1.h"
#include "Player/PlayerState/PlayerState_Skill2.h"
#include "Input/PlayerInputComponent.h"
#include "Player/PlayerCharacter.h"
#include "Player/Skill/SkillComponent.h"
#include "Player/Skill/PlayerSkillTypes.h"

void UPlayerState_Idle::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 订阅直接性受击事件，受击时切 HitBack
	Player->OnTakeDirectDamage.AddDynamic(this, &UPlayerState_Idle::OnTakeDirectDamage);

	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (!Input) return;

	Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Idle::OnMoveInput);
	Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Idle::OnMoveInput);
	Input->OnLmbDelegate.AddUObject(this, &UPlayerState_Idle::OnLmb);
	Input->OnRmbDelegate.AddUObject(this, &UPlayerState_Idle::OnRmb);
	Input->OnQDelegate.AddUObject(this, &UPlayerState_Idle::OnQ);
	Input->OnEDelegate.AddUObject(this, &UPlayerState_Idle::OnE);
}

void UPlayerState_Idle::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 取消订阅
	Player->OnTakeDirectDamage.RemoveDynamic(this, &UPlayerState_Idle::OnTakeDirectDamage);

	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (!Input) return;

	Input->OnMoveXDelegate.RemoveAll(this);
	Input->OnMoveYDelegate.RemoveAll(this);
	Input->OnLmbDelegate.RemoveAll(this);
	Input->OnRmbDelegate.RemoveAll(this);
	Input->OnQDelegate.RemoveAll(this);
	Input->OnEDelegate.RemoveAll(this);
}

void UPlayerState_Idle::OnTakeDirectDamage(const FTakeDamageInfo& DamageInfo)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || Player->IsDead()) return;

	Player->SwitchState(UPlayerState_HitBack::StaticClass());
}

void UPlayerState_Idle::OnMoveInput(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player)
	{
		Player->SwitchState(UPlayerState_Move::StaticClass());
	}
}

void UPlayerState_Idle::OnLmb()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player && Player->bCanAttack1)
	{
		Player->SwitchState(UPlayerState_Attack1::StaticClass());
	}
}

void UPlayerState_Idle::OnRmb()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player && Player->bCanAttack2)
	{
		Player->SwitchState(UPlayerState_Attack2::StaticClass());
	}
}

void UPlayerState_Idle::OnQ()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !Player->SkillComponent) return;

	if (!Player->SkillComponent->IsOnCooldown(EPlayerSkillID::TripleProjectile, Player->SkillComponent->GetTripleProjectileCooldown()))
	{
		Player->SwitchState(UPlayerState_Skill1::StaticClass());
	}
}

void UPlayerState_Idle::OnE()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !Player->SkillComponent) return;

	if (Player->SkillComponent->CanTriggerCircularSlashInput())
	{
		Player->SwitchState(UPlayerState_Skill2::StaticClass());
	}
}

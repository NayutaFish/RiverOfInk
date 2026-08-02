// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerState/PlayerState_Idle.h"
#include "Test_GamePlay.h"
#include "PlayerState/PlayerState_Move.h"
#include "PlayerState/PlayerState_Attack1.h"
#include "PlayerState/PlayerState_Attack2.h"
#include "PlayerState/PlayerState_Skill1.h"
#include "Input/PlayerInputComponent.h"
#include "HikariPlayerCharacter.h"
#include "Skill/HikariSkillComponent.h"
#include "Skill/SkillTypes.h"

void UPlayerState_Idle::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (!Player) return;

	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (!Input) return;

	Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Idle::OnMoveInput);
	Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Idle::OnMoveInput);
	Input->OnLmbDelegate.AddUObject(this, &UPlayerState_Idle::OnLmb);
	Input->OnRmbDelegate.AddUObject(this, &UPlayerState_Idle::OnRmb);
	Input->OnQDelegate.AddUObject(this, &UPlayerState_Idle::OnQ);

	UE_LOG(LogTest_GamePlay, Log, TEXT("State: Idle"));
}

void UPlayerState_Idle::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (!Player) return;

	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (!Input) return;

	Input->OnMoveXDelegate.RemoveAll(this);
	Input->OnMoveYDelegate.RemoveAll(this);
	Input->OnLmbDelegate.RemoveAll(this);
	Input->OnRmbDelegate.RemoveAll(this);
	Input->OnQDelegate.RemoveAll(this);
}

void UPlayerState_Idle::OnMoveInput(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;

	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (Player)
	{
		Player->SwitchState(UPlayerState_Move::StaticClass());
	}
}

void UPlayerState_Idle::OnLmb()
{
	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (Player && Player->bCanAttack1)
	{
		Player->SwitchState(UPlayerState_Attack1::StaticClass());
	}
}

void UPlayerState_Idle::OnRmb()
{
	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (Player && Player->bCanAttack2)
	{
		Player->SwitchState(UPlayerState_Attack2::StaticClass());
	}
}

void UPlayerState_Idle::OnQ()
{
	AHikariPlayerCharacter* Player = Cast<AHikariPlayerCharacter>(GetOwner());
	if (!Player || !Player->SkillComponent) return;

	if (!Player->SkillComponent->IsOnCooldown(EPlayerSkillID::TripleProjectile, Player->SkillComponent->TripleProjectileCooldown))
	{
		Player->SwitchState(UPlayerState_Skill1::StaticClass());
	}
}
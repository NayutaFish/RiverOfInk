// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PlayerState/PlayerState_Move.h"
#include "RiverOfInk.h"
#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Attack1.h"
#include "Player/PlayerState/PlayerState_Attack2.h"
#include "Player/PlayerState/PlayerState_Dash.h"
#include "Player/PlayerState/PlayerState_HitBack.h"
#include "Player/PlayerState/PlayerState_Skill1.h"
#include "Input/PlayerInputComponent.h"
#include "Player/PlayerCharacter.h"
#include "Player/Skill/SkillComponent.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "GameFramework/CharacterMovementComponent.h"

UPlayerState_Move::UPlayerState_Move()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerState_Move::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	LastInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	LastShiftTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 订阅直接性受击事件，受击时切 HitBack
	Player->OnTakeDirectDamage.AddDynamic(this, &UPlayerState_Move::OnTakeDirectDamage);

	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (!Input) return;

	Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Move::OnMoveX);
	Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Move::OnMoveY);
	Input->OnShiftDelegate.AddUObject(this, &UPlayerState_Move::OnShift);
		Input->OnLmbDelegate.AddUObject(this, &UPlayerState_Move::OnLmb);
		Input->OnRmbDelegate.AddUObject(this, &UPlayerState_Move::OnRmb);
		Input->OnSpaceDelegate.AddUObject(this, &UPlayerState_Move::OnSpace);
		Input->OnQDelegate.AddUObject(this, &UPlayerState_Move::OnQ);
}

void UPlayerState_Move::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 取消订阅
	Player->OnTakeDirectDamage.RemoveDynamic(this, &UPlayerState_Move::OnTakeDirectDamage);

	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (Input)
	{
		Input->OnMoveXDelegate.RemoveAll(this);
		Input->OnMoveYDelegate.RemoveAll(this);
		Input->OnShiftDelegate.RemoveAll(this);
		Input->OnLmbDelegate.RemoveAll(this);
		Input->OnRmbDelegate.RemoveAll(this);
		Input->OnSpaceDelegate.RemoveAll(this);
		Input->OnQDelegate.RemoveAll(this);
	}

	Player->GetCharacterMovement()->MaxWalkSpeed = Player->WalkSpeed;
}

void UPlayerState_Move::OnTakeDirectDamage(const FTakeDamageInfo& DamageInfo)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || Player->bIsDead) return;

	Player->SwitchState(UPlayerState_HitBack::StaticClass());
}

void UPlayerState_Move::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	// 疾跑：按 Shift 时加速
	float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bool bSprinting = (Now - LastShiftTime) < 0.15f;
	Player->GetCharacterMovement()->MaxWalkSpeed = bSprinting ? Player->SprintSpeed : Player->WalkSpeed;

	// 无输入一段时间后切回 Idle
	if ((Now - LastInputTime) > 0.15f)
	{
		Player->SwitchState(UPlayerState_Idle::StaticClass());
	}
}

void UPlayerState_Move::OnMoveX(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;

	LastInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	const FVector Dir = (FVector::RightVector - FVector::ForwardVector).GetSafeNormal();
	Player->AddMovementInput(Dir, Value);

	FString DirText = Value > 0.0f ? TEXT("→ D") : TEXT("← A");
}

void UPlayerState_Move::OnMoveY(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;

	LastInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	const FVector Dir = (FVector::ForwardVector + FVector::RightVector).GetSafeNormal();
	Player->AddMovementInput(Dir, Value);

	FString DirText = Value > 0.0f ? TEXT("↑ W") : TEXT("↓ S");
}

void UPlayerState_Move::OnQ()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !Player->SkillComponent) return;

	if (!Player->SkillComponent->IsOnCooldown(EPlayerSkillID::TripleProjectile, Player->SkillComponent->TripleProjectileCooldown))
	{
		Player->SwitchState(UPlayerState_Skill1::StaticClass());
	}
}

void UPlayerState_Move::OnSpace()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player && Player->bCanDash)
	{
		Player->SwitchState(UPlayerState_Dash::StaticClass());
	}
}

void UPlayerState_Move::OnShift(float Value)
{
	LastShiftTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

}

void UPlayerState_Move::OnLmb()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player && Player->bCanAttack1)
	{
		Player->SwitchState(UPlayerState_Attack1::StaticClass());
	}
}

void UPlayerState_Move::OnRmb()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (Player && Player->bCanAttack2)
	{
		Player->SwitchState(UPlayerState_Attack2::StaticClass());
	}
}
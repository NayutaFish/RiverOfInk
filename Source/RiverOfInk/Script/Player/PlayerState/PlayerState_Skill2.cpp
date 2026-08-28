// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/PlayerState/PlayerState_Skill2.h"

#include "Player/PlayerState/PlayerState_Idle.h"
#include "Player/PlayerState/PlayerState_Move.h"
#include "Input/PlayerInputComponent.h"
#include "Player/PlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void UPlayerState_Skill2::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>();
	if (Input)
	{
		Input->OnMoveXDelegate.AddUObject(this, &UPlayerState_Skill2::OnMoveX);
		Input->OnMoveYDelegate.AddUObject(this, &UPlayerState_Skill2::OnMoveY);
		Input->OnEDelegate.AddUObject(this, &UPlayerState_Skill2::OnE);
	}

	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		FHitResult Hit;
		PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit);
		if (Hit.bBlockingHit)
		{
			FVector ToTarget = Hit.Location - Player->GetActorLocation();
			ToTarget.Z = 0.0f;
			if (!ToTarget.IsNearlyZero())
			{
				Player->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
			}
		}
	}

	Player->TryCastSkillSlot2();
	Player->BeginAttack();

	const FVector CurrentVelocity = Player->GetCharacterMovement()->Velocity;
	SlideDirection = !CurrentVelocity.IsNearlyZero()
		? CurrentVelocity.GetSafeNormal()
		: FVector::ZeroVector;

	bHadMoveInput = false;
	GetWorld()->GetTimerManager().SetTimer(SkillTimerHandle, this, &UPlayerState_Skill2::OnSkillTimer, 0.3f, false);
}

void UPlayerState_Skill2::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	if (UPlayerInputComponent* Input = Player->FindComponentByClass<UPlayerInputComponent>())
	{
		Input->OnMoveXDelegate.RemoveAll(this);
		Input->OnMoveYDelegate.RemoveAll(this);
		Input->OnEDelegate.RemoveAll(this);
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SkillTimerHandle);
	}
}

void UPlayerState_Skill2::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	if (!SlideDirection.IsNearlyZero())
	{
		Player->GetCharacterMovement()->Velocity = SlideDirection * 200.0f;
	}
	else
	{
		Player->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	}
}

void UPlayerState_Skill2::OnMoveX(float Value)
{
	if (!FMath::IsNearlyZero(Value)) bHadMoveInput = true;
}

void UPlayerState_Skill2::OnMoveY(float Value)
{
	if (!FMath::IsNearlyZero(Value)) bHadMoveInput = true;
}

void UPlayerState_Skill2::OnE()
{
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner()))
	{
		Player->RequestSkill2Input();
	}
}

void UPlayerState_Skill2::OnSkillTimer()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player) return;

	Player->SwitchState(bHadMoveInput
		? UPlayerState_Move::StaticClass()
		: UPlayerState_Idle::StaticClass());
}


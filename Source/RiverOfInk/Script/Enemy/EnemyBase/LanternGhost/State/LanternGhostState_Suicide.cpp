// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/LanternGhost/State/LanternGhostState_Suicide.h"

#include "Common/AttackAreaBase.h"
#include "Core/Audio/AudioManager.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RiverOfInk.h"
#include "TimerManager.h"

ULanternGhostState_Suicide::ULanternGhostState_Suicide()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULanternGhostState_Suicide::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DetonateTimerHandle,
			this,
			&ULanternGhostState_Suicide::Detonate,
			0.1f,
			false);
	}
}

void ULanternGhostState_Suicide::OnExit_Implementation()
{
	Super::OnExit_Implementation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DetonateTimerHandle);
	}
}

void ULanternGhostState_Suicide::Detonate()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (!ExplosionSoundName.IsEmpty())
		{
			FAudioManager::Play(ExplosionSoundName);
		}

		if (Enemy->AttackAreaClass)
		{
			const FTransform SpawnTransform(FRotator::ZeroRotator, Enemy->GetActorLocation());
			if (AAttackAreaBase* Explosion = World->SpawnActorDeferred<AAttackAreaBase>(
				Enemy->AttackAreaClass,
				SpawnTransform,
				Enemy,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Explosion->Radius = ExplosionRadius;
				Explosion->bDamageOpponentOnly = true;
				Explosion->bDetectObstacle = false;
				Explosion->bIsEnemyProjectile = false;
				Explosion->Initialize(ExplosionLifetime, 0.0f, true, nullptr);
				UGameplayStatics::FinishSpawningActor(Explosion, SpawnTransform);

				UE_LOG(LogRiverOfInk, Log,
					TEXT("Enemy %s suicide explosion spawned: Area=%s Radius=%.1f Lifetime=%.2f."),
					*Enemy->GetName(), *Explosion->GetName(), ExplosionRadius, ExplosionLifetime);
			}
		}
		else
		{
			UE_LOG(LogRiverOfInk, Warning,
				TEXT("Enemy %s suicide skipped: AttackAreaClass is missing."),
				*Enemy->GetName());
		}
	}

	Enemy->Die();
}

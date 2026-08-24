// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/LanternGhost/State/LanternGhostState_Melee.h"

#include "Core/Audio/AudioManager.h"

ULanternGhostState_Melee::ULanternGhostState_Melee()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULanternGhostState_Melee::OnEnter_Implementation()
{
	if (!DashSoundName.IsEmpty())
	{
		FAudioManager::Play(DashSoundName);
	}

	Super::OnEnter_Implementation();
}

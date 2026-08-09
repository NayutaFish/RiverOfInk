// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/EnemyState/EnemyState_Dead.h"

#include "Enemy/EnemyBase/EnemyBase.h"

void UEnemyState_Dead::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		Enemy->HandleDeadState();
	}
}

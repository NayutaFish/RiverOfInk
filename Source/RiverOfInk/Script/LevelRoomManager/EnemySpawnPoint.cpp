// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelRoomManager/EnemySpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

AEnemySpawnPoint::AEnemySpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(SceneRoot);
	Arrow->ArrowSize = 1.5f;

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(SceneRoot);
}

FTransform AEnemySpawnPoint::GetSpawnTransform() const
{
	FTransform Result = GetActorTransform();

	if (SpawnRadius > KINDA_SMALL_NUMBER)
	{
		// 在以本点为圆心的水平圆内取随机位置（XY 平面）
		const float Radius = SpawnRadius * FMath::Sqrt(FMath::FRand());
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
		Result.SetLocation(Result.GetLocation() + Offset);
	}

	return Result;
}

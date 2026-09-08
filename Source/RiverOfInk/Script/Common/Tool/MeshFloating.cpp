// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/Tool/MeshFloating.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"

UMeshFloating::UMeshFloating()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMeshFloating::BeginPlay()
{
	Super::BeginPlay();

	Meshes.Reset();
	OriginalRelativeLocations.Reset();
	ElapsedTime = 0.0f;

	if (!GetOwner())
	{
		return;
	}

	TArray<UMeshComponent*> FoundMeshes;
	GetOwner()->GetComponents<UMeshComponent>(FoundMeshes);
	for (UMeshComponent* Mesh : FoundMeshes)
	{
		if (Mesh)
		{
			Meshes.Add(Mesh);
			OriginalRelativeLocations.Add(Mesh->GetRelativeLocation());
		}
	}
}

void UMeshFloating::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Meshes.Num() == 0)
	{
		return;
	}

	ElapsedTime += DeltaTime;

	const float CycleTime = FMath::Max(0.01f, FloatCycleTime);
	const float Angle = 2.0f * PI * ElapsedTime / CycleTime;
	const float Offset = FloatAmplitude * FMath::Sin(Angle);

	for (int32 Index = 0; Index < Meshes.Num(); ++Index)
	{
		if (Meshes[Index])
		{
			const FVector NewLocation = OriginalRelativeLocations[Index] + FVector(0.0f, 0.0f, Offset);
			Meshes[Index]->SetRelativeLocation(NewLocation);
		}
	}
}

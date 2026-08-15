// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/StageIntro/InkOverlay.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AInkOverlay::AInkOverlay()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	OverlayPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OverlayPlane"));
	OverlayPlane->SetupAttachment(RootComponent);
	OverlayPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverlayPlane->SetCastShadow(false);
	OverlayPlane->bReceivesDecals = false;
	// Keep the translucent ink layer in front of the opaque paper plane at
	// this close separation. This avoids depth/sort ambiguity when the camera
	// is nearly orthographic to the scroll surface.
	OverlayPlane->TranslucencySortPriority = 100;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		OverlayPlane->SetStaticMesh(PlaneFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InkMaterialFinder(
		TEXT("/Game/Presentation/StageIntro/M_InkPollution_Simple.M_InkPollution_Simple"));
	if (InkMaterialFinder.Succeeded())
	{
		OverlayPlane->SetMaterial(0, InkMaterialFinder.Object);
	}

	Tags.Add(TEXT("Stage01InkOverlay"));
}

void AInkOverlay::BeginPlay()
{
	Super::BeginPlay();
	CreateDynamicMaterial();
	SetInkProgress(InkProgress);
}

void AInkOverlay::CreateDynamicMaterial()
{
	if (!IsValid(OverlayPlane))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 InkOverlay cannot create MID: OverlayPlane is invalid."));
		return;
	}

	// Prefer the named asset even when a Blueprint template supplied a
	// placeholder/world-grid material. This keeps the MID contract stable
	// after the asset is created or replaced in the editor.
	UMaterialInterface* SourceMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Presentation/StageIntro/M_InkPollution_Simple.M_InkPollution_Simple"));
	if (IsValid(SourceMaterial))
	{
		OverlayPlane->SetMaterial(0, SourceMaterial);
	}
	else
	{
		SourceMaterial = OverlayPlane->GetMaterial(0);
	}

	if (!IsValid(SourceMaterial))
	{
		UE_LOG(LogTemp, Error,
			TEXT("Stage01 InkOverlay cannot create MID: M_InkPollution_Simple is unavailable."));
		return;
	}

	DynamicMaterial = UMaterialInstanceDynamic::Create(SourceMaterial, this);
	if (!IsValid(DynamicMaterial))
	{
		UE_LOG(LogTemp, Error, TEXT("Stage01 InkOverlay failed to create dynamic material instance."));
		return;
	}

	OverlayPlane->SetMaterial(0, DynamicMaterial);
	ApplyMaterialParameters();
}

void AInkOverlay::ApplyMaterialParameters()
{
	if (!IsValid(DynamicMaterial))
	{
		return;
	}

	DynamicMaterial->SetScalarParameterValue(TEXT("InkProgress"), InkProgress);
	DynamicMaterial->SetVectorParameterValue(TEXT("InkColor"), InkColor);
	DynamicMaterial->SetScalarParameterValue(TEXT("InkNoiseStrength"), InkNoiseStrength);
	DynamicMaterial->SetVectorParameterValue(
		TEXT("InkCenter"),
		FLinearColor(InkCenter.X, InkCenter.Y, 0.0f, 0.0f));
	DynamicMaterial->SetScalarParameterValue(TEXT("InkScale"), InkScale);
}

void AInkOverlay::SetInkProgress(float InProgress)
{
	InkProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);

	if (!IsValid(DynamicMaterial))
	{
		CreateDynamicMaterial();
	}

	if (!IsValid(DynamicMaterial))
	{
		UE_LOG(LogTemp, Error,
			TEXT("Stage01 InkOverlay SetInkProgress(%f) failed: DynamicMaterial is invalid."),
			InkProgress);
		return;
	}

	ApplyMaterialParameters();
}

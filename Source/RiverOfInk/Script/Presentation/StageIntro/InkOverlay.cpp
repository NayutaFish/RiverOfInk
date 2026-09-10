// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/StageIntro/InkOverlay.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	float InkSmoothStep(const float Edge0, const float Edge1, const float Value)
	{
		const float Range = FMath::Max(KINDA_SMALL_NUMBER, Edge1 - Edge0);
		const float T = FMath::Clamp((Value - Edge0) / Range, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}
}

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
		TEXT("/Game/Presentation/StageIntro/M_InkPollution_Opening_v6.M_InkPollution_Opening_v6"));
	if (InkMaterialFinder.Succeeded())
	{
		OverlayPlane->SetMaterial(0, InkMaterialFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> ParameterCollectionFinder(
		TEXT("/Game/Presentation/StageIntro/MPC_InkOpening.MPC_InkOpening"));
	if (ParameterCollectionFinder.Succeeded())
	{
		InkOpeningParameters = ParameterCollectionFinder.Object;
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

	// Prefer the opening material even when a Blueprint template supplied a
	// placeholder. The simple material remains a compatibility fallback.
	UMaterialInterface* SourceMaterial = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Presentation/StageIntro/M_InkPollution_Opening_v6.M_InkPollution_Opening_v6"));
	if (!IsValid(SourceMaterial))
	{
		SourceMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Presentation/StageIntro/M_InkPollution_Opening_v2.M_InkPollution_Opening_v2"));
	}
	if (!IsValid(SourceMaterial))
	{
		SourceMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Presentation/StageIntro/M_InkPollution_Opening.M_InkPollution_Opening"));
	}
	if (!IsValid(SourceMaterial))
	{
		SourceMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Presentation/StageIntro/M_InkPollution_Simple.M_InkPollution_Simple"));
	}	if (IsValid(SourceMaterial))
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
			TEXT("Stage01 InkOverlay cannot create MID: opening ink material is unavailable."));
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
	if (IsValid(DynamicMaterial))
	{
		// Retain the original MID contract for the legacy fallback material.
		DynamicMaterial->SetScalarParameterValue(TEXT("InkProgress"), InkProgress);
		DynamicMaterial->SetVectorParameterValue(TEXT("InkColor"), InkColor);
		DynamicMaterial->SetScalarParameterValue(TEXT("InkNoiseStrength"), InkNoiseStrength);
		DynamicMaterial->SetVectorParameterValue(
			TEXT("InkCenter"),
			FLinearColor(InkCenter.X, InkCenter.Y, 0.0f, 0.0f));
		DynamicMaterial->SetScalarParameterValue(TEXT("InkScale"), InkScale);
	}

	ApplyOpeningCollectionParameters();
}

void AInkOverlay::ApplyOpeningCollectionParameters()
{
	if (!IsValid(InkOpeningParameters))
	{
		InkOpeningParameters = LoadObject<UMaterialParameterCollection>(
			nullptr,
			TEXT("/Game/Presentation/StageIntro/MPC_InkOpening.MPC_InkOpening"));
	}

	if (!IsValid(InkOpeningParameters) || !GetWorld())
	{
		return;
	}

	UMaterialParameterCollectionInstance* CollectionInstance =
		GetWorld()->GetParameterCollectionInstance(InkOpeningParameters);
	if (!IsValid(CollectionInstance))
	{
		return;
	}

	const float Progress = FMath::Clamp(InkProgress, 0.0f, 1.0f);
	const float SlowFastSlow = InkSmoothStep(0.0f, 1.0f, Progress);
	const float MiddleFlow = FMath::Sin(PI * Progress);
	const float WetEnvelope = InkSmoothStep(0.05f, 0.28f, Progress)
		* (1.0f - 0.18f * InkSmoothStep(0.86f, 1.0f, Progress));
	const float SplatterWindow = InkSmoothStep(0.14f, 0.27f, Progress)
		* (1.0f - InkSmoothStep(0.58f, 0.72f, Progress));

	CollectionInstance->SetScalarParameterValue(
		TEXT("InkGrowth"),
		FMath::Clamp(SlowFastSlow * FMath::Max(0.25f, InkScale / 0.52f), 0.0f, 1.0f));
	CollectionInstance->SetScalarParameterValue(
		TEXT("InkOpacity"), InkSmoothStep(0.005f, 0.045f, Progress));
	CollectionInstance->SetScalarParameterValue(
		TEXT("InkEdgeWidth"),
		FMath::Clamp(InkEdgeWidth * (0.70f + InkNoiseStrength * 0.50f), 0.010f, 0.12f));
	CollectionInstance->SetScalarParameterValue(
		TEXT("InkWetness"), FMath::Clamp(InkWetness * WetEnvelope, 0.0f, 1.0f));
	CollectionInstance->SetScalarParameterValue(
		TEXT("InkFlowSpeed"), FMath::Clamp(InkFlowSpeed * (0.14f + MiddleFlow * 0.86f), 0.0f, 2.0f));
	CollectionInstance->SetScalarParameterValue(
		TEXT("InkCoreDensity"), InkCoreDensity * InkSmoothStep(0.015f, 0.24f, Progress));
	CollectionInstance->SetScalarParameterValue(
		TEXT("InkSplatterAmount"), InkSplatterAmount * SplatterWindow);
	// The blackout is deliberately two-stage: a long, visible lead-in avoids
// a final-frame pop, then a short settle reaches pure black before travel.
const float BlackoutLeadIn = 0.84f * InkSmoothStep(0.66f, 0.89f, Progress);
const float BlackoutSettle = 0.16f * InkSmoothStep(0.89f, 0.98f, Progress);
CollectionInstance->SetScalarParameterValue(
    TEXT("InkBlackout"), InkBlackout * FMath::Clamp(BlackoutLeadIn + BlackoutSettle, 0.0f, 1.0f));
	CollectionInstance->SetVectorParameterValue(
		TEXT("InkOrigin"), FLinearColor(InkCenter.X, InkCenter.Y, 0.0f, 0.0f));
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
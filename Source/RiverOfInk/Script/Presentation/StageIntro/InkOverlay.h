// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InkOverlay.generated.h"

class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UStaticMeshComponent;

/** Runtime-owned, world-space ink overlay for the Stage 1 opening. */
UCLASS(Blueprintable)
class RIVEROFINK_API AInkOverlay : public AActor
{
	GENERATED_BODY()

public:
	AInkOverlay();

	virtual void BeginPlay() override;

	/** Set the normalized ink reveal amount. 0 is clean paper; 1 is black-out. */
	UFUNCTION(BlueprintCallable, Category = "Stage Intro|Ink")
	void SetInkProgress(float InProgress);

	UFUNCTION(BlueprintPure, Category = "Stage Intro|Ink")
	float GetInkProgress() const { return InkProgress; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Intro|Ink")
	TObjectPtr<UStaticMeshComponent> OverlayPlane;

	/** Legacy fallback tint; the opening material uses a warm-black ink palette. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink")
	FLinearColor InkColor = FLinearColor(0.018f, 0.012f, 0.009f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InkNoiseStrength = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink")
	FVector2D InkCenter = FVector2D(0.58f, 0.46f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.01"))
	float InkScale = 0.52f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.01", ClampMax = "0.20"))
	float InkEdgeWidth = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InkWetness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float InkFlowSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InkCoreDensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InkSplatterAmount = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InkBlackout = 1.0f;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	/** Single Sequencer-facing control collection shared by material and VFX. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage Intro|Ink")
	TObjectPtr<UMaterialParameterCollection> InkOpeningParameters;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InkProgress = 0.0f;

private:
	void CreateDynamicMaterial();
	void ApplyMaterialParameters();
	void ApplyOpeningCollectionParameters();
};
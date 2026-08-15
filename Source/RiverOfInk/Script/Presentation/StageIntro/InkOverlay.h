// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InkOverlay.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** Runtime-owned ink overlay for the Stage 1 intro placeholder. */
UCLASS(Blueprintable)
class RIVEROFINK_API AInkOverlay : public AActor
{
	GENERATED_BODY()

public:
	AInkOverlay();

	virtual void BeginPlay() override;

	/** Set the normalized ink reveal amount. 0 is invisible; 1 is fully open. */
	UFUNCTION(BlueprintCallable, Category = "Stage Intro|Ink")
	void SetInkProgress(float InProgress);

	UFUNCTION(BlueprintPure, Category = "Stage Intro|Ink")
	float GetInkProgress() const { return InkProgress; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Intro|Ink")
	TObjectPtr<UStaticMeshComponent> OverlayPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink")
	FLinearColor InkColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InkNoiseStrength = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink")
	FVector2D InkCenter = FVector2D(0.58f, 0.46f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage Intro|Ink", meta = (ClampMin = "0.01"))
	float InkScale = 0.52f;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage Intro|Ink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InkProgress = 0.0f;

private:
	void CreateDynamicMaterial();
	void ApplyMaterialParameters();
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatBuildIconPlaceholderWidget.generated.h"

/**
 * Lightweight geometry used when a build icon has not been imported yet.
 *
 * This is intentionally a UUserWidget because it exposes NativePaint while
 * remaining a normal UWidget that can be inserted into the native HUD tree.
 * It is a visual fallback only; it carries no build data or gameplay logic.
 */
UENUM(BlueprintType)
enum class ECombatBuildIconPlaceholderKind : uint8
{
	Generic UMETA(DisplayName = "Generic"),
	TwoStageArc UMETA(DisplayName = "Two Stage Arc"),
	TwinSlash UMETA(DisplayName = "Twin Slash"),
	Cooldown UMETA(DisplayName = "Cooldown")
};

UCLASS(Blueprintable)
class RIVEROFINK_API UCombatBuildIconPlaceholderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shape family selected by the centralized build-icon resolver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder")
	ECombatBuildIconPlaceholderKind PlaceholderKind = ECombatBuildIconPlaceholderKind::Generic;

	/** Ink tint used by the whitebox geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder")
	FLinearColor LineColor = FLinearColor(0.035f, 0.03f, 0.025f, 0.94f);

	/** Stroke width in Slate pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder", meta = (ClampMin = "0.5", ClampMax = "12.0"))
	float LineThickness = 3.5f;

	/** Inset from the allotted slot before drawing the geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placeholder", meta = (ClampMin = "0.0", ClampMax = "64.0"))
	float InnerPadding = 18.0f;

	UFUNCTION(BlueprintCallable, Category = "HUD|Build")
	void SetPlaceholderKind(ECombatBuildIconPlaceholderKind InPlaceholderKind);

	UFUNCTION(BlueprintCallable, Category = "HUD|Build")
	void SetLineColor(FLinearColor InLineColor);

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
};

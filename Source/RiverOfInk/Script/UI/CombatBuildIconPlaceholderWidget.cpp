// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CombatBuildIconPlaceholderWidget.h"

#include "Rendering/DrawElements.h"

namespace
{
	void DrawPlaceholderLines(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FPaintGeometry& PaintGeometry,
		TArray<FVector2f>&& Points,
		const FLinearColor& Tint,
		float Thickness)
	{
		if (Points.Num() < 2)
		{
			return;
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			MoveTemp(Points),
			ESlateDrawEffect::None,
			Tint,
			true,
			Thickness);
	}

	void DrawPlaceholderArc(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FPaintGeometry& PaintGeometry,
		const FVector2D& Center,
		float Radius,
		float StartDegrees,
		float EndDegrees,
		int32 SegmentCount,
		const FLinearColor& Tint,
		float Thickness)
	{
		if (Radius <= 0.0f || SegmentCount < 1)
		{
			return;
		}

		TArray<FVector2f> Points;
		Points.Reserve(SegmentCount + 1);
		for (int32 SegmentIndex = 0; SegmentIndex <= SegmentCount; ++SegmentIndex)
		{
			const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
			const float AngleRadians = FMath::DegreesToRadians(FMath::Lerp(StartDegrees, EndDegrees, Alpha));
			Points.Add(FVector2f(
				static_cast<float>(Center.X + FMath::Cos(AngleRadians) * Radius),
				static_cast<float>(Center.Y + FMath::Sin(AngleRadians) * Radius)));
		}

		DrawPlaceholderLines(OutDrawElements, LayerId, PaintGeometry, MoveTemp(Points), Tint, Thickness);
	}
}

void UCombatBuildIconPlaceholderWidget::SetPlaceholderKind(ECombatBuildIconPlaceholderKind InPlaceholderKind)
{
	if (PlaceholderKind == InPlaceholderKind)
	{
		return;
	}

	PlaceholderKind = InPlaceholderKind;
	InvalidateLayoutAndVolatility();
}

void UCombatBuildIconPlaceholderWidget::SetLineColor(FLinearColor InLineColor)
{
	if (LineColor == InLineColor)
	{
		return;
	}

	LineColor = InLineColor;
	InvalidateLayoutAndVolatility();
}

int32 UCombatBuildIconPlaceholderWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 MaxLayerId = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (LocalSize.X <= KINDA_SMALL_NUMBER || LocalSize.Y <= KINDA_SMALL_NUMBER)
	{
		return MaxLayerId;
	}

	const FVector2D Center = LocalSize * 0.5f;
	const float Radius = FMath::Max(1.0f, FMath::Min(LocalSize.X, LocalSize.Y) * 0.5f - InnerPadding);
	const float SafeThickness = FMath::Max(0.5f, LineThickness);
	const FLinearColor Tint = LineColor * InWidgetStyle.GetColorAndOpacityTint();
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const int32 DrawLayer = MaxLayerId + 1;

	switch (PlaceholderKind)
	{
	case ECombatBuildIconPlaceholderKind::TwoStageArc:
		// Two separated, unequal arcs communicate the two release stages without
		// pretending to be the final authored icon.
		DrawPlaceholderArc(
			OutDrawElements,
			DrawLayer,
			PaintGeometry,
			Center + FVector2D(-Radius * 0.08f, Radius * 0.04f),
			Radius * 0.88f,
			-165.0f,
			28.0f,
			16,
			Tint,
			SafeThickness);
		DrawPlaceholderArc(
			OutDrawElements,
			DrawLayer,
			PaintGeometry,
			Center + FVector2D(Radius * 0.08f, -Radius * 0.02f),
			Radius * 0.67f,
			28.0f,
			220.0f,
			14,
			Tint,
			SafeThickness * 0.9f);
		break;

	case ECombatBuildIconPlaceholderKind::TwinSlash:
		{
			TArray<FVector2f> FirstSlash;
			FirstSlash.Add(FVector2f(
				static_cast<float>(Center.X - Radius * 0.68f),
				static_cast<float>(Center.Y + Radius * 0.48f)));
			FirstSlash.Add(FVector2f(
				static_cast<float>(Center.X + Radius * 0.52f),
				static_cast<float>(Center.Y - Radius * 0.62f)));
			DrawPlaceholderLines(
				OutDrawElements,
				DrawLayer,
				PaintGeometry,
				MoveTemp(FirstSlash),
				Tint,
				SafeThickness);

			TArray<FVector2f> SecondSlash;
			SecondSlash.Add(FVector2f(
				static_cast<float>(Center.X - Radius * 0.42f),
				static_cast<float>(Center.Y + Radius * 0.68f)));
			SecondSlash.Add(FVector2f(
				static_cast<float>(Center.X + Radius * 0.78f),
				static_cast<float>(Center.Y - Radius * 0.20f)));
			DrawPlaceholderLines(
				OutDrawElements,
				DrawLayer,
				PaintGeometry,
				MoveTemp(SecondSlash),
				Tint,
				SafeThickness * 0.82f);
		}
		break;

	case ECombatBuildIconPlaceholderKind::Cooldown:
		// Broken repeated arc segments leave clear gaps, matching the idea of a
		// cooldown cycle while keeping the fallback deliberately geometric.
		DrawPlaceholderArc(OutDrawElements, DrawLayer, PaintGeometry, Center, Radius * 0.83f, -145.0f, -38.0f, 8, Tint, SafeThickness);
		DrawPlaceholderArc(OutDrawElements, DrawLayer, PaintGeometry, Center, Radius * 0.83f, -8.0f, 78.0f, 8, Tint, SafeThickness);
		DrawPlaceholderArc(OutDrawElements, DrawLayer, PaintGeometry, Center, Radius * 0.83f, 108.0f, 205.0f, 8, Tint, SafeThickness);
		DrawPlaceholderArc(OutDrawElements, DrawLayer, PaintGeometry, Center, Radius * 0.52f, 212.0f, 296.0f, 6, Tint, SafeThickness * 0.75f);
		break;

	case ECombatBuildIconPlaceholderKind::Generic:
	default:
		DrawPlaceholderArc(OutDrawElements, DrawLayer, PaintGeometry, Center, Radius * 0.76f, -12.0f, 328.0f, 24, Tint, SafeThickness);
		DrawPlaceholderArc(OutDrawElements, DrawLayer, PaintGeometry, Center, Radius * 0.28f, 42.0f, 318.0f, 12, Tint, SafeThickness * 0.8f);
		break;
	}

	return DrawLayer;
}

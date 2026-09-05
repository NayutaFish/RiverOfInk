// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/AttackArea/AttackAreaBase_Bezier.h"

AAttackAreaBase_Bezier::AAttackAreaBase_Bezier()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAttackAreaBase_Bezier::BeginPlay()
{
	Super::BeginPlay();

	// 贝塞尔移动完全由曲线驱动，不叠加基类的直线 Speed 移动。
	Speed = 0.0f;

	StartPoint = GetActorLocation();
	BezierElapsedTime = 0.0f;
	bBezierInitialized = false;
}

void AAttackAreaBase_Bezier::SetBezierTarget(const FVector& InTarget)
{
	EndPoint = InTarget;

	if (StartPoint.IsNearlyZero())
	{
		StartPoint = GetActorLocation();
	}

	const FVector ToEnd = EndPoint - StartPoint;
	ControlPoint = StartPoint + ToEnd * FMath::Clamp(BezierP1PositionRate, 0.0f, 1.0f);

	const FVector Direction = ToEnd.GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		// 水平面内垂直于 P0->P2 的方向
		const FVector Perpendicular(-Direction.Y, Direction.X, 0.0f);
		ControlPoint += Perpendicular * BezierP1Offset;
	}

	bBezierInitialized = true;
	BezierElapsedTime = 0.0f;
}

void AAttackAreaBase_Bezier::Tick(float DeltaTime)
{
	if (bBezierInitialized)
	{
		BezierElapsedTime += DeltaTime;
		const float Duration = FMath::Max(0.001f, LifeTime);
		const float Alpha = FMath::Clamp(BezierElapsedTime / Duration, 0.0f, 1.0f);
		SetActorLocation(CalculateBezierPosition(Alpha));
	}

	Super::Tick(DeltaTime);
}

FVector AAttackAreaBase_Bezier::CalculateBezierPosition(float Alpha) const
{
	// 二阶贝塞尔：B(t) = (1-t)^2 * P0 + 2*(1-t)*t * P1 + t^2 * P2
	const float OneMinusT = 1.0f - Alpha;
	return OneMinusT * OneMinusT * StartPoint
		+ 2.0f * OneMinusT * Alpha * ControlPoint
		+ Alpha * Alpha * EndPoint;
}

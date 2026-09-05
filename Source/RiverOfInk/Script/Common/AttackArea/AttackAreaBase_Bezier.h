// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/AttackAreaBase.h"
#include "AttackAreaBase_Bezier.generated.h"

/**
 * 使用二阶贝塞尔曲线移动的攻击区域。
 *
 * P0 = 生成时的出生起点
 * P2 = 生成后通过 SetBezierTarget 传入的目标位置
 * P1 = P0 到 P2 方向上的 BezierP1PositionRate 位置，
 *      再沿 P0-P2 垂直方向偏移 BezierP1Offset
 */
UCLASS(Blueprintable)
class RIVEROFINK_API AAttackAreaBase_Bezier : public AAttackAreaBase
{
GENERATED_BODY()

public:
AAttackAreaBase_Bezier();

/** 设置贝塞尔终点 P2，并自动根据 P0/P2 计算控制点 P1。 */
UFUNCTION(BlueprintCallable, Category = "Attack|Bezier")
void SetBezierTarget(const FVector& InTarget);

/** P1 沿 P0->P2 方向的比例（0~1）。 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier", meta = (ClampMin = "0.0", ClampMax = "1.0"))
float BezierP1PositionRate = 0.5f;

/** P1 在 P0-P2 垂直方向上的偏移距离。 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier")
float BezierP1Offset = 100.0f;

/** 贝塞尔曲线段占 LifeTime 的比例；到达 P2 后剩余时间沿切线直线飞出。 */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier", meta = (ClampMin = "0.05", ClampMax = "0.95"))
float BezierReachTargetRatio = 0.5f;

protected:
virtual void BeginPlay() override;
virtual void Tick(float DeltaTime) override;

/** 子类可覆盖以改变贝塞尔移动的时间流速（例如慢启动）。 */
virtual float GetMovementTimeScale(float RealTime) const;

private:
FVector CalculateBezierPosition(float Alpha) const;
FVector TangentExitVelocity = FVector::ZeroVector;

FVector StartPoint = FVector::ZeroVector;
FVector ControlPoint = FVector::ZeroVector;
FVector EndPoint = FVector::ZeroVector;

float BezierElapsedTime = 0.0f;
float RealElapsedTime = 0.0f;
bool bBezierInitialized = false;
};
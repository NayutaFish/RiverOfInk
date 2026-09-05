// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/AttackArea/AttackAreaBase_Bezier.h"
#include "AttackAreaBaseBz_LanternGhostRange.generated.h"

class UNiagaraComponent;

/**
 * 灯笼怪远程攻击专用的贝塞尔攻击区域。
 *
 * 在基类贝塞尔曲线移动基础上，增加：
 * - 召唤后延迟 0.5 秒再进入 0.8 秒的 20% 慢速窗口
 * - 子物体 Niagara 的 ScaleSize 在出生后 0.5 秒内从 0.2 渐变到 1
 */
UCLASS(Blueprintable)
class RIVEROFINK_API AAttackAreaBaseBz_LanternGhostRange : public AAttackAreaBase_Bezier
{
	GENERATED_BODY()

public:
	AAttackAreaBaseBz_LanternGhostRange();

	/** 初始速度倍率。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier|SlowStart", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialSpeedScale = 0.2f;

	/** 减速持续的真实时间（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier|SlowStart", meta = (ClampMin = "0.0", Units = "s"))
	float SlowDuration = 0.8f;

	/** 召唤后延迟多少秒再开始减速。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier|SlowStart", meta = (ClampMin = "0.0", Units = "s"))
	float SlowStartDelay = 0.5f;

	/** 子物体 Niagara 的缩放用户参数名。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier|Niagara")
	FName ScaleSizeParameterName = TEXT("User.ScaleSize");

	/** ScaleSize 起始值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier|Niagara", meta = (ClampMin = "0.0"))
	float ScaleSizeStart = 1.0f;

	/** ScaleSize 目标值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier|Niagara", meta = (ClampMin = "0.0"))
	float ScaleSizeEnd = 1.0f;

	/** ScaleSize 渐变时间（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Bezier|Niagara", meta = (ClampMin = "0.01", Units = "s"))
	float ScaleSizeDuration = 0.5f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float GetMovementTimeScale(float RealTime) const override;

private:
	TObjectPtr<UNiagaraComponent> ChildNiagara;
	float ScaleElapsedTime = 0.0f;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/AttackAreaBase.h"
#include "AttackAreaBase_Orbit.generated.h"

/**
 * 绕着一个中心 Actor 旋转的攻击区域（“攻击法球”）。
 *
 * 生命周期分两段：
 *   1. 绕行：每帧把自身摆到 “中心 + 半径方向 + 高度偏移” 上，角度按角速度推进。
 *      这一阶段按近战处理（持续存在、同一目标只结算一次接触伤害），所以法球可以一直贴身在场上。
 *   2. 发射：调用 LaunchAsProjectile 后停止绕行，按给定方向与速度直线飞出，转为普通射弹
 *      （命中即结算并销毁、可被净墨环消除、按 bDetectObstacle 撞墙消失）。
 *
 * 生死由本类自己按时间判定：绕行寿命到了、或者中心 Actor 被销毁（施放者死亡/消失）时自行消失。
 */
UCLASS(Blueprintable)
class RIVEROFINK_API AAttackAreaBase_Orbit : public AAttackAreaBase
{
	GENERATED_BODY()

public:
	AAttackAreaBase_Orbit();

	/**
	 * 开始绕行。
	 *
	 * @param InCenter					绕行中心（一般是施放者；用弱引用持有，中心没了法球会自行收场）
	 * @param InRadius					绕行半径（cm）
	 * @param InAngularSpeedDegrees		角速度（度/秒，世界坐标系绕 Z 轴；正负决定旋向）
	 * @param InStartAngleDegrees		起始角度（度，世界坐标系；0 = +X，90 = +Y）
	 * @param InHeightOffset			相对中心的高度偏移（cm）
	 * @param InOrbitLifeTime			绕行寿命（秒）；<= 0 表示不限时
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack|Orbit")
	void InitializeOrbit(
		AActor* InCenter,
		float InRadius,
		float InAngularSpeedDegrees,
		float InStartAngleDegrees,
		float InHeightOffset,
		float InOrbitLifeTime);

	/** 停止绕行并沿 InDirection（只取水平方向）直线飞出。 */
	UFUNCTION(BlueprintCallable, Category = "Attack|Orbit")
	void LaunchAsProjectile(const FVector& InDirection, float InSpeed, float InFlightLifeTime);

	/** 是否仍在绕行（已发射或已停止时为 false）。 */
	UFUNCTION(BlueprintPure, Category = "Attack|Orbit")
	bool IsOrbiting() const { return bOrbiting; }

protected:
	virtual void Tick(float DeltaTime) override;

private:
	/** 按当前角度把自身摆到绕行位置上（DeltaTime 用于推进角度）。 */
	void UpdateOrbitLocation(const AActor& Center, float DeltaTime);

	/** 绕行中心（弱引用）。 */
	TWeakObjectPtr<AActor> OrbitCenter;

	float OrbitRadius = 180.0f;

	/** 角速度（度/秒）。 */
	float OrbitAngularSpeedDegrees = 180.0f;

	/** 当前角度（度，世界坐标系）。 */
	float OrbitCurrentAngleDegrees = 0.0f;

	float OrbitHeightOffset = 0.0f;

	/** 绕行寿命（秒）；<= 0 表示不限时。 */
	float OrbitLifeTime = 0.0f;

	/** 出生到现在的存活时间：绕行/飞行两段共用（基类的 ElapsedTime 是私有的且只在 BeginPlay 归零）。 */
	float OrbitElapsedTime = 0.0f;

	/** 发射时刻的 OrbitElapsedTime，用于计算飞行寿命。 */
	float LaunchElapsedTime = 0.0f;

	/** 发射后的飞行寿命（秒）。 */
	float FlightLifeTime = 0.0f;

	bool bOrbiting = false;
	bool bLaunched = false;
};

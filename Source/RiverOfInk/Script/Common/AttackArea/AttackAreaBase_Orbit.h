// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/AttackAreaBase.h"
#include "AttackAreaBase_Orbit.generated.h"

/**
 * 绑定在宿主身上的“攻击法球”：会绕宿主旋转、跟着宿主排成编队、被发射出去或飞回宿主。
 *
 * 存活期间处于四种状态之一（互斥）：
 *   1. 绕行（InitializeOrbit）：每帧摆到 “宿主 + 半径方向 + 高度偏移” 上，角度按角速度推进。
 *      这一阶段按近战处理（持续存在、同一目标只结算一次接触伤害），所以法球可以一直贴身在场上。
 *   2. 编队（InitializeFormation）：保持出生时相对宿主朝向的本地偏移，跟着宿主移动/转向
 *      （用于“四个法球和宿主横向排成一排”这类造型）。
 *   3. 发射（LaunchAsProjectile）：停止绕行/编队，按给定方向与速度直线飞出，转为普通射弹
 *      （命中即结算并销毁、可被净墨环消除、按 bDetectObstacle 撞墙消失）。
 *   4. 回收（RecallToHost）：朝宿主飞回去，进入 AcceptRadius 后自行消失。
 *
 * 凡是“跟着宿主”的状态，宿主被销毁（施放者死亡/消失）时法球会自行收场，不会留在场上。
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

	/**
	 * 以“编队”方式存在：保持出生时相对宿主朝向的本地偏移，跟着宿主一起移动与转向。
	 * 调用前请先把 Actor 摆到想要的相对位置上。
	 *
	 * @param InHost		跟随的宿主（一般是施放者）
	 * @param InLifeTime	存活时间（秒）；<= 0 表示不限时（活到被回收或宿主消失）
	 */
	UFUNCTION(BlueprintCallable, Category = "Attack|Orbit")
	void InitializeFormation(AActor* InHost, float InLifeTime);

	/** 停止绕行/编队并沿 InDirection（只取水平方向）直线飞出。 */
	UFUNCTION(BlueprintCallable, Category = "Attack|Orbit")
	void LaunchAsProjectile(const FVector& InDirection, float InSpeed, float InFlightLifeTime);

	/** 朝宿主飞回去；进入 InAcceptRadius 后自行消失。 */
	UFUNCTION(BlueprintCallable, Category = "Attack|Orbit")
	void RecallToHost(float InSpeed, float InAcceptRadius);

	/** 是否仍在绕行（已发射/已回收/编队中时为 false）。 */
	UFUNCTION(BlueprintPure, Category = "Attack|Orbit")
	bool IsOrbiting() const { return bOrbiting; }

	/** 是否正在编队跟随宿主。 */
	UFUNCTION(BlueprintPure, Category = "Attack|Orbit")
	bool IsFollowingHost() const { return bFollowing; }

	/** 是否正在被回收（朝宿主飞回）。 */
	UFUNCTION(BlueprintPure, Category = "Attack|Orbit")
	bool IsRecallingToHost() const { return bRecalling; }

protected:
	virtual void Tick(float DeltaTime) override;

private:
	/** 按当前角度把自身摆到绕行位置上（DeltaTime 用于推进角度）。 */
	void UpdateOrbitLocation(const AActor& Host, float DeltaTime);

	/** 按本地偏移把自身摆到编队位置上（跟随宿主朝向）。 */
	void UpdateFormationLocation(const AActor& Host);

	/** 朝宿主靠近；返回 true 表示已经到达并销毁。 */
	bool UpdateRecallLocation(const AActor& Host, float DeltaTime);

	/** 宿主（弱引用）：绕行中心 / 编队跟随目标 / 回收目标。 */
	TWeakObjectPtr<AActor> HostActor;

	float OrbitRadius = 180.0f;

	/** 角速度（度/秒）。 */
	float OrbitAngularSpeedDegrees = 180.0f;

	/** 当前角度（度，世界坐标系）。 */
	float OrbitCurrentAngleDegrees = 0.0f;

	float OrbitHeightOffset = 0.0f;

	/** 绕行寿命（秒）；<= 0 表示不限时。 */
	float OrbitLifeTime = 0.0f;

	/** 编队时相对宿主朝向的本地偏移（水平面内跟随宿主 Yaw 旋转）。 */
	FVector FormationLocalOffset = FVector::ZeroVector;

	/** 回收速度（cm/s）。 */
	float RecallSpeed = 1200.0f;

	/** 回收时距离宿主多近算“到位”。 */
	float RecallAcceptRadius = 60.0f;

	/** 出生到现在的存活时间：绕行/飞行两段共用（基类的 ElapsedTime 是私有的且只在 BeginPlay 归零）。 */
	float OrbitElapsedTime = 0.0f;

	/** 发射时刻的 OrbitElapsedTime，用于计算飞行寿命。 */
	float LaunchElapsedTime = 0.0f;

	/** 发射后的飞行寿命（秒）。 */
	float FlightLifeTime = 0.0f;

	bool bOrbiting = false;
	bool bFollowing = false;
	bool bRecalling = false;
	bool bLaunched = false;
};

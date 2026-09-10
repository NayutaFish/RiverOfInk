// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "LanternGhostState_Ranged_Elite.generated.h"

class AAttackAreaBase;
class AEnemyBase;

/**
 * 远程精英灯笼怪攻击状态。
 *
 * 每次进入该状态（即每次蓄力结束）执行**一种**攻击方式，三种方式依次循环，三次蓄力攻击构成一轮：
 *   模式 0：左右两侧各一枚贝塞尔曲线射弹打向**自身朝向**，连续发射 2 次，两次间隔 1 秒；
 *   模式 1：朝**自身朝向**发射 3 枚直线散射射弹（相邻夹角 30 度），连续发射 3 次，每次间隔 1 秒；
 *   模式 2：以自身为中心、面朝方向为前，前/后/左/右/左前/左后/右前/右后八个方向各一枚直线射弹。
 *
 * 三种方式的弹道方向**全部以自身朝向为准**，不会锁定玩家当前位置：
 * 攻击状态会把朝向锁定在“进入攻击时面向目标”的方向，之后玩家靠走位即可躲开弹幕。
 *
 * 序列期间的每一发之间用计时器排程；状态被硬值打断/切换时会把排程全部清理，不会补发。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API ULanternGhostState_Ranged_Elite : public UEnemyState_Attack
{
	GENERATED_BODY()

public:
	ULanternGhostState_Ranged_Elite();

	// ── 射弹类 ──

	/** 模式 0 使用的贝塞尔射弹类；留空则回退到敌人的 AttackAreaClass（要求是贝塞尔子类）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Class")
	TSubclassOf<AAttackAreaBase> BezierProjectileClass;

	/** 模式 1/2 使用的直线射弹类；留空则回退到敌人的 AttackAreaClass（要求不是贝塞尔子类）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Class")
	TSubclassOf<AAttackAreaBase> StraightProjectileClass;

	// ── 节奏 ──

	/** 同一种攻击方式内两次发射之间的间隔（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float RepeatInterval = 1.0f;

	// ── 直线射弹 ──

	/** 直线射弹飞行速度（cm/s）；贝塞尔弹道由曲线自身控制速度，不使用该值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Straight", meta = (ClampMin = "1.0"))
	float StraightProjectileSpeed = 900.0f;

	/** 直线射弹沿发射方向的前向生成偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Straight", meta = (ClampMin = "0.0", Units = "cm"))
	float StraightMuzzleOffset = 80.0f;

	// ── 模式 0：两侧贝塞尔连发 ──

	/** 贝塞尔连发次数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "1"))
	int32 BezierVolleyCount = 2;

	/** 左右两枚贝塞尔弹的控制点横向偏移（一侧 +，一侧 −）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (Units = "cm"))
	float BezierSideOffset = 400.0f;

	/** 贝塞尔控制点 P1 沿 起点->终点 方向的比例。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BezierP1PositionRate = 0.3f;

	/**
	 * 贝塞尔弹终点相对自身的“前向距离”：终点 = 自身位置 + 朝向 × 该值。
	 * 不再取玩家当前位置，弹道方向完全由自身朝向决定；到达终点后剩余时间沿切线直线飞出，所以这个值只需给一个合理的作用距离。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "0.0", Units = "cm"))
	float BezierTargetForwardDistance = 1200.0f;

	/** 初始速度倍率（仅对灯笼怪远程专用贝塞尔子类生效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BezierInitialSpeedScale = 0.2f;

	/** 慢速窗口持续的真实时间（秒，仅对灯笼怪远程专用贝塞尔子类生效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "0.0", Units = "s"))
	float BezierSlowDuration = 0.8f;

	/** 召唤后延迟多少秒开始减速（秒，仅对灯笼怪远程专用贝塞尔子类生效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "0.0", Units = "s"))
	float BezierSlowStartDelay = 0.5f;

	/** Niagara 缩放起始值（仅对灯笼怪远程专用贝塞尔子类生效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "0.0"))
	float BezierScaleSizeStart = 0.2f;

	/** Niagara 缩放目标值（仅对灯笼怪远程专用贝塞尔子类生效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "0.0"))
	float BezierScaleSizeEnd = 1.0f;

	/** Niagara 缩放渐变时间（秒，仅对灯笼怪远程专用贝塞尔子类生效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Bezier", meta = (ClampMin = "0.01", Units = "s"))
	float BezierScaleSizeDuration = 0.5f;

	// ── 模式 1：直线散射连发 ──

	/** 每次散射的射弹数量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Spread", meta = (ClampMin = "1"))
	int32 SpreadProjectileCount = 3;

	/** 相邻两枚射弹之间的角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Spread", meta = (ClampMin = "0.0", Units = "deg"))
	float SpreadAngleStep = 30.0f;

	/** 散射连发次数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Spread", meta = (ClampMin = "1"))
	int32 SpreadBurstCount = 3;

	// ── 模式 2：八方向环形 ──

	/** 环形射弹数量；默认 8 = 前/后/左/右 + 四个斜向（按面朝方向均匀展开）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ranged Elite|Ring", meta = (ClampMin = "1"))
	int32 RingProjectileCount = 8;

	// ── 运行时 ──

	/** 下一次进入攻击状态要使用的攻击方式（0/1/2）；每次进入前进一位，三次一轮循环。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Ranged Elite|Runtime")
	int32 CurrentAttackMode = 0;

	/** 当前正在执行的攻击方式（运行时）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Ranged Elite|Runtime")
	int32 ActiveAttackMode = 0;

	/** 本次攻击方式还剩几发（运行时）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Ranged Elite|Runtime")
	int32 ShotsRemaining = 0;

	UFUNCTION(BlueprintPure, Category = "Enemy|Ranged Elite|Runtime")
	int32 GetCurrentAttackMode() const { return CurrentAttackMode; }

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;

	/** 进入状态后（前摇结束）执行本次攻击方式的第一发。 */
	virtual void ExecuteAttack() override;

private:
	/** 执行一发；还有剩余发数则排下一次，否则收尾回 Chase。 */
	void PerformShotStep();

	/** 按当前模式发射一次（模式 0 一次 = 两枚贝塞尔；模式 1 一次 = 一组散射；模式 2 一次 = 整圈）。 */
	void FireOnce(AEnemyBase& Enemy);

	/** 两侧贝塞尔连发：弹道与终点都以 FacingYaw 为准。 */
	void FireBezierVolley(AEnemyBase& Enemy, float FacingYaw);

	/** 直线散射：以 FacingYaw 为中心对称展开。 */
	void FireSpreadBurst(AEnemyBase& Enemy, float FacingYaw);

	/** 八方向环形：以 FacingYaw 为“前”。 */
	void FireRingBurst(AEnemyBase& Enemy, float FacingYaw);

	/** 生成一枚直线射弹（方向由 Yaw 决定）。 */
	void SpawnStraightProjectile(AEnemyBase& Enemy, TSubclassOf<AAttackAreaBase> ProjectileClass, float Yaw);

	/** 解析模式 0 使用的贝塞尔射弹类。 */
	TSubclassOf<AAttackAreaBase> ResolveBezierClass(const AEnemyBase& Enemy) const;

	/** 解析模式 1/2 使用的直线射弹类。 */
	TSubclassOf<AAttackAreaBase> ResolveStraightClass(const AEnemyBase& Enemy) const;

	/** 该模式的发射次数。 */
	int32 GetShotCountForMode(int32 Mode) const;

	/** 序列结束后回 Chase。 */
	void ScheduleReturnToChase();

	/** 本状态是否仍是敌人当前状态（防止排程在切状态后继续补发）。 */
	bool IsStillCurrentState() const;

	/** 结束（或已无剩余）后回追击的计时器。 */
	FTimerHandle ReturnTimerHandle;

	/** 同一模式内两发之间的计时器。 */
	FTimerHandle RepeatTimerHandle;

	/** 序列是否正在执行，避免重复进入。 */
	bool bSequenceActive = false;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "BiteyState_Common.generated.h"

class AAttackAreaBase;
class AAttackAreaBase_Orbit;
class AEnemyBase;

/**
 * 阿咬（Bitey）的通用攻击状态。
 *
 * 每次进入只执行**一种**攻击方式，两种方式交替循环（与远程精英灯笼怪的攻击循环同构）：
 *   模式 0 召唤法球：在自身左右各召唤一枚攻击法球，绕着自身旋转；法球绕行期间持续接触伤害玩家。
 *   模式 1 冲撞+发射：朝目标方向冲撞（类似近战灯笼怪），冲撞结束 OrbLaunchDelay（默认 0.6s）后，
 *                    把仍在绕行的法球朝目标方向发射出去；法球已经不在就不发射，只冲撞。
 *
 * 配置要点：
 *   - 冲撞前摇复用基类攻击状态的 AttackWindupTime；冲撞速度/时长/后摇复用敌人身上的
 *     ChargeSpeed / ChargeDuration / AttackRecoveryTime（和近战灯笼怪同一套参数）。
 *   - 冲撞命中盒与法球类在本状态上分别赋值（ChargeAttackAreaClass / OrbAttackAreaClass），
 *     冲撞类留空时回退到敌人的 AttackAreaClass。
 *   - 本状态自己驱动冲撞位移，不使用敌人的 AttackMoveSpeed，也不需要开启 bUseChargeAttack。
 *   - 两种方式的衔接节奏由 Chase 上的 AttackInterval 控制（与远程精英一致），
 *     所以 AttackInterval 建议小于 OrbLifeTime，否则法球会在下一次冲撞前先自行消失。
 *   - 法球是独立 Actor：状态退出（含被硬值打断）不会被清理。下一次模式 1 会发射仍在场的法球，
 *     下一次模式 0 会先清掉残留再重新召唤，保证每次都是“左右各一”的固定造型。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UBiteyState_Common : public UEnemyState_Attack
{
	GENERATED_BODY()

public:
	UBiteyState_Common();

	// ── 攻击方式 ──

	/** 下一次进入状态要使用的方式（0/1）；每次执行后推进一位，两种一轮循环。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Bitey|Runtime")
	int32 CurrentAttackMode = 0;

	/** 本次正在执行的方式（运行时）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Bitey|Runtime")
	int32 ActiveAttackMode = 0;

	UFUNCTION(BlueprintPure, Category = "Enemy|Bitey|Runtime")
	int32 GetCurrentAttackMode() const { return CurrentAttackMode; }

	// ── 攻击区域类（编辑器赋值） ──

	/** 法球类；必须是 AAttackAreaBase_Orbit 子类。留空则不会召唤法球（只打日志告警）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Class")
	TSubclassOf<AAttackAreaBase_Orbit> OrbAttackAreaClass;

	/** 冲撞命中盒类；留空则回退到敌人的 AttackAreaClass（仍为空则冲撞不带伤害）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Class")
	TSubclassOf<AAttackAreaBase> ChargeAttackAreaClass;

	// ── 法球 ──

	/** 召唤数量；默认 2（左右各一）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Orb", meta = (ClampMin = "1"))
	int32 OrbCount = 2;

	/** 绕行半径（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Orb", meta = (ClampMin = "0.0", Units = "cm"))
	float OrbOrbitRadius = 180.0f;

	/** 绕行角速度（度/秒）；正负决定旋向。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Orb", meta = (Units = "deg"))
	float OrbOrbitSpeed = 180.0f;

	/** 第一枚法球的出生角度（相对自身朝向，度）；默认 90 = 自身右侧，其余按 OrbCount 均分，第二枚落在左侧。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Orb", meta = (Units = "deg"))
	float OrbFirstAngleOffset = 90.0f;

	/** 法球相对自身中心的高度偏移（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Orb", meta = (Units = "cm"))
	float OrbHeightOffset = 0.0f;

	/** 法球绕行阶段寿命（秒）；<= 0 表示不限时。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Orb", meta = (ClampMin = "0.0", Units = "s"))
	float OrbLifeTime = 12.0f;

	/** 发射速度（cm/s）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Orb|Launch", meta = (ClampMin = "1.0"))
	float OrbLaunchSpeed = 1400.0f;

	/** 发射后的飞行寿命（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Orb|Launch", meta = (ClampMin = "0.05", Units = "s"))
	float OrbFlightLifeTime = 6.0f;

	// ── 节奏 ──

	/** 冲撞结束后到发射法球之间的延迟（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float OrbLaunchDelay = 0.6f;

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

	/** 前摇结束后执行本次攻击方式。 */
	virtual void ExecuteAttack() override;

private:
	/** 模式 0：召唤法球，然后回 Chase。 */
	void EnterSummonMode(AEnemyBase& Enemy);

	/** 模式 1：锁定冲撞方向并开始冲撞。 */
	void EnterChargeMode(AEnemyBase& Enemy);

	void BeginCharge(AEnemyBase& Enemy);

	/** 冲撞收束：清命中盒并开始“延迟发射法球”的计时。 */
	void FinishCharge(AEnemyBase& Enemy, const TCHAR* EndReason);

	/** 冲撞时长（ChargeDuration）到点。 */
	void FinishChargeByDuration();

	/** 延迟到点：发射法球并进入后摇。 */
	void LaunchOrbsAndRecover();

	void LaunchOrbs(AEnemyBase& Enemy);

	void ScheduleReturnToChase(AEnemyBase& Enemy);

	/** 清掉上一轮残留的法球，再按 OrbCount 重新召唤。 */
	void SummonOrbs(AEnemyBase& Enemy);

	void SpawnOrb(AEnemyBase& Enemy, float StartAngleDegrees);

	void ClearOrbs();

	/** 收集仍在绕行的法球（已销毁/已发射的会顺手从列表剔除）。 */
	void GatherOrbitingOrbs(TArray<AAttackAreaBase_Orbit*>& OutOrbs);

	void ClearChargeAttackArea();

	TSubclassOf<AAttackAreaBase> ResolveChargeClass(const AEnemyBase& Enemy) const;

	/** 本状态是否仍是敌人当前状态（防止计时器在切状态后继续补动作）。 */
	bool IsStillCurrentState() const;

	/** 本状态召唤的法球（弱引用：法球可能被寿命/净墨环/撞墙销毁）。 */
	TArray<TWeakObjectPtr<AAttackAreaBase_Orbit>> Orbs;

	/** 冲撞期间的跟随型命中盒。 */
	UPROPERTY()
	TObjectPtr<AAttackAreaBase> ChargeAttackArea;

	FVector ChargeDirection = FVector::ForwardVector;

	bool bCharging = false;

	FTimerHandle ChargeEndTimerHandle;
	FTimerHandle OrbLaunchTimerHandle;
	FTimerHandle ReturnTimerHandle;
};

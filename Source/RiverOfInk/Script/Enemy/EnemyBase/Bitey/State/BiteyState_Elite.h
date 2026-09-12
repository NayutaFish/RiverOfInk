// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "BiteyState_Elite.generated.h"

class AAttackAreaBase;
class AAttackAreaBase_Orbit;
class AEnemyBase;

/**
 * 精英阿咬（Bitey Elite）的攻击状态：独立脚本，三种攻击方式轮流循环。
 *
 *   模式 0「四斜向法球」：在左前 / 右前 / 左后 / 右后各召唤一枚法球绕着自己旋转
 *                        （OrbCount=4、起始角 45°，按 360°/4 均分正好落在四个斜向）。
 *   模式 1「三段冲撞」：朝目标连续冲撞 ChargeRepeatCount（3）次，每段之间间隔
 *                        ChargeRepeatInterval（0.3s），每段开始时重新瞄准目标；
 *                        最后一段结束 TripleChargeRecallDelay（0.6s）后，把仍在绕行的法球
 *                        朝自身位置收回，靠近到 OrbRecallAcceptRadius 内自行消失。
 *   模式 2「横向排开 → 冲撞 → 回收」：在自身朝向的左边 LineOrbsPerSide 枚、右边同样数量
 *                        （默认每侧 2 枚，共 4 枚），四枚法球与自身横向排成一排；
 *                        LineUpDashDelay（0.6s）后朝目标冲撞（此时法球仍然跟着宿主一起移动），
 *                        冲撞结束 OrbRecallDelay（0.4s）后留存的法球朝自身位置飞回，
 *                        靠近到 OrbRecallAcceptRadius 内自行消失。
 *
 * 配置要点（与通用版 BiteyState_Common 一致）：
 *   - 冲撞前摇复用基类攻击状态的 AttackWindupTime；冲撞速度/时长/后摇复用敌人身上的
 *     ChargeSpeed / ChargeDuration / AttackRecoveryTime。
 *   - 冲撞命中盒与法球类在本状态上分别赋值（ChargeAttackAreaClass / OrbAttackAreaClass），
 *     冲撞类留空时回退到敌人的 AttackAreaClass。
 *   - 本状态自己驱动冲撞位移，不使用敌人的 AttackMoveSpeed，也不需要开启 bUseChargeAttack。
 *   - 攻击状态期间本状态接管朝向：冲撞时对准该段冲撞方向，其余时间持续转向目标
 *     （基类每帧会把朝向锁回进入状态时的方向，不接管就会“冲完转回去”）。
 *   - 三种方式的衔接节奏由 Chase 上的 AttackInterval 控制。
 *   - 法球是独立 Actor：状态退出（含被硬值打断）不会清理它们。下一次模式 0 会先清掉残留再重新召唤，
 *     模式 1 / 模式 2 结束后的回收会让法球飞回自身并删除。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UBiteyState_Elite : public UEnemyState_Attack
{
	GENERATED_BODY()

public:
	UBiteyState_Elite();

	// ── 攻击方式 ──

	/** 下一次进入状态要使用的方式（0/1/2）；每次执行后推进一位，三种一轮循环。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Bitey Elite|Runtime")
	int32 CurrentAttackMode = 0;

	/** 本次正在执行的方式（运行时）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Bitey Elite|Runtime")
	int32 ActiveAttackMode = 0;

	UFUNCTION(BlueprintPure, Category = "Enemy|Bitey Elite|Runtime")
	int32 GetCurrentAttackMode() const { return CurrentAttackMode; }

	// ── 攻击区域类（编辑器赋值） ──

	/** 法球类；必须是 AAttackAreaBase_Orbit 子类。留空则不会召唤法球（只打日志告警）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Class")
	TSubclassOf<AAttackAreaBase_Orbit> OrbAttackAreaClass;

	/** 冲撞命中盒类；留空则回退到敌人的 AttackAreaClass（仍为空则冲撞不带伤害）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Class")
	TSubclassOf<AAttackAreaBase> ChargeAttackAreaClass;

	// ── 法球（模式 0 的绕行 / 两个模式结束后的回收） ──

	/** 模式 0 的召唤数量；默认 4（左右前后四个斜向）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Orb", meta = (ClampMin = "1"))
	int32 OrbCount = 4;

	/** 绕行半径（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Orb", meta = (ClampMin = "0.0", Units = "cm"))
	float OrbOrbitRadius = 180.0f;

	/** 绕行角速度（度/秒）；正负决定旋向。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Orb", meta = (Units = "deg"))
	float OrbOrbitSpeed = 180.0f;

	/** 第一枚法球的出生角度（相对自身朝向，度）；默认 45 = 右前，其余按 OrbCount 均分。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Orb", meta = (Units = "deg"))
	float OrbFirstAngleOffset = 45.0f;

	/** 法球相对自身中心的高度偏移（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Orb", meta = (Units = "cm"))
	float OrbHeightOffset = 0.0f;

	/**
	 * 法球寿命（秒）：模式 0 的绕行 / 模式 2 的编队都用它兜底。
	 * 0（默认）= 不限时 —— 法球要活过整个“召唤 → 下一次冲撞后回收”的周期，
	 * 而周期长度由 Chase 的 AttackInterval 决定，配成固定秒数很容易在回收前先到寿。
	 * 想限制就填正数；两个模式的回收都会主动删除法球，不受这里影响。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Orb", meta = (ClampMin = "0.0", Units = "s"))
	float OrbLifeTime = 0.0f;

	/** 模式 1（三段冲撞）最后一段结束后到开始回收法球之间的延迟（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Charge", meta = (ClampMin = "0.0", Units = "s"))
	float TripleChargeRecallDelay = 0.6f;

	// ── 模式 1：连续冲撞 ──

	/** 一次攻击方式里连续冲撞的段数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Charge", meta = (ClampMin = "1"))
	int32 ChargeRepeatCount = 3;

	/** 相邻两段冲撞之间的间隔（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Charge", meta = (ClampMin = "0.0", Units = "s"))
	float ChargeRepeatInterval = 0.3f;

	// ── 模式 2：横向排开 → 冲撞 → 回收 ──

	/** 每一侧排开的法球数量（默认每侧 2 枚，共 4 枚）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Line", meta = (ClampMin = "1"))
	int32 LineOrbsPerSide = 2;

	/** 排开时相邻法球的横向间距（cm）；最靠外的一枚距离自身 = 间距 × 每侧数量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Line", meta = (ClampMin = "1.0", Units = "cm"))
	float LineOrbSpacing = 150.0f;

	/** 排开之后到开始冲撞的延迟（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Line", meta = (ClampMin = "0.0", Units = "s"))
	float LineUpDashDelay = 0.6f;

	/** 冲撞结束后到开始回收法球的延迟（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Line", meta = (ClampMin = "0.0", Units = "s"))
	float OrbRecallDelay = 0.4f;

	/** 回收飞行速度（cm/s）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Line", meta = (ClampMin = "1.0"))
	float OrbRecallSpeed = 1200.0f;

	/** 回收时距离自身多近算“到位”（到位即删除该法球）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Bitey Elite|Line", meta = (ClampMin = "1.0", Units = "cm"))
	float OrbRecallAcceptRadius = 60.0f;

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

	/** 前摇结束后执行本次攻击方式。 */
	virtual void ExecuteAttack() override;

private:
	// ── 模式 0：四个斜向法球绕行 ──

	void EnterOrbitOrbMode(AEnemyBase& Enemy);
	void SummonOrbs(AEnemyBase& Enemy);
	void SpawnOrb(AEnemyBase& Enemy, float StartAngleDegrees);

	// ── 模式 1：三段冲撞 → 延迟发射法球 ──

	void EnterTripleChargeMode(AEnemyBase& Enemy);

	/** 设定本次冲撞段数并冲第一段。 */
	void BeginChargeSequence(AEnemyBase& Enemy);

	/** 朝目标重新瞄准并冲一段。 */
	void StartCharge(AEnemyBase& Enemy);

	/** 多段冲撞之间的间隔到点：冲下一段。 */
	void StartChargeRepeat();

	void BeginCharge(AEnemyBase& Enemy);

	/** 单段冲撞收束：清命中盒；还有剩余段数就排下一段，否则进入回收流程。 */
	void FinishCharge(AEnemyBase& Enemy, const TCHAR* EndReason);

	/** 冲撞时长（ChargeDuration）到点。 */
	void FinishChargeByDuration();

	// ── 模式 2：横向排开 → 冲撞 → 回收 ──

	void EnterLineUpMode(AEnemyBase& Enemy);
	void SummonLineOrbs(AEnemyBase& Enemy);

	/** 排开延迟到点：朝目标冲一段。 */
	void StartLineUpDash();

	/** 冲撞结束延迟到点：回收法球并进入后摇（模式 1 / 模式 2 共用）。 */
	void RecallOrbsAndRecover();
	void RecallOrbs(AEnemyBase& Enemy);

	// ── 工具 ──

	/**
	 * 在指定位置生成一枚法球并登记到 Orbs（伤害类型/过滤/障碍检测一并设好），
	 * 但不动它的运动模式——调用方自己决定 InitializeOrbit / InitializeFormation。
	 * 类没配或生成失败时返回 nullptr。
	 */
	AAttackAreaBase_Orbit* CreateOrb(AEnemyBase& Enemy, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	void ClearOrbs();

	/** 把已经销毁的法球从 Orbs 里剔除。 */
	void PruneOrbs();

	/** 收集所有还在场的法球（回收用）。 */
	void GatherAliveOrbs(TArray<AAttackAreaBase_Orbit*>& OutOrbs);

	void ScheduleReturnToChase(AEnemyBase& Enemy);
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

	/** 本次冲撞链还剩几段没冲。 */
	int32 ChargesRemaining = 0;

	FTimerHandle ChargeEndTimerHandle;
	FTimerHandle ChargeRepeatTimerHandle;
	FTimerHandle OrbLaunchTimerHandle;
	FTimerHandle ReturnTimerHandle;
};

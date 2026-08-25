// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GlobalStructs.h"
#include "Common/CombatEffectTypes.h"
#include "Enemy/EnemyBase/EnemyHealthTypes.h"
#include "EnemyBase.generated.h"

class UStaticMeshComponent;
class UMeshComponent;
class UMaterialInstanceDynamic;
class UCapsuleComponent;
class AAttackAreaBase;
class UStateBase;
class APlayerCharacter;
class UEnemyHealthWidget;
class UWidgetComponent;
class UCombatEffectComponent;
class UNiagaraComponent;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDeathSignature, AActor*, DeadEnemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDamageResolvedSignature, const FDamageResult&, DamageResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnEnemyHealthChangedSignature,
	float, PreviousHealth,
	float, CurrentHealth,
	float, MaxHealth,
	EEnemyHealthChangeReason, ChangeReason);

/**
 * Shared enemy base for the first melee state-machine pass.
 *
 * Owns health, damage, state transitions, death notification, and attack
 * configuration. Movement and attack execution remain in state components.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API AEnemyBase : public AActor
{
	GENERATED_BODY()

public:
	AEnemyBase();

	// ── 状态机 ──
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	TObjectPtr<UStateBase> CurrentState;

	UFUNCTION(BlueprintCallable, Category = "State")
	void SwitchState(TSubclassOf<UStateBase> StateClass);

	/** Runtime Buff/Debuff/Proc container shared by all enemy classes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Effects")
	TObjectPtr<UCombatEffectComponent> CombatEffectComponent;

	UFUNCTION(BlueprintPure, Category = "Enemy|Effects")
	UCombatEffectComponent* GetCombatEffectComponent() const { return CombatEffectComponent; }

	/** Niagara presentation for the player-owned Tracking Buff mark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|VFX|Tracking Buff")
	TObjectPtr<UNiagaraSystem> HomingMarkVFX;

	/** Reusable attached component; gameplay owns only its activation lifetime. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|VFX|Tracking Buff")
	TObjectPtr<UNiagaraComponent> HomingMarkVFXComponent;

	/** Ground-relative placement for the ring around this enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|VFX|Tracking Buff")
	FVector HomingMarkVFXRelativeLocation = FVector(0.0f, 0.0f, -45.0f);

	/** Uniform scale for the authored Tracking Buff system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|VFX|Tracking Buff", meta = (ClampMin = "0.01"))
	float HomingMarkVFXScale = 1.0f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 胶囊体（同时也是根组件） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UCapsuleComponent> CapsuleCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Screen-space enemy health widget component; visibility is owned by the enemy lifecycle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|UI")
	TObjectPtr<UWidgetComponent> HealthWidgetComponent;

	/** Gameplay-owned rank consumed by the shared Enemy Health widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Identity")
	EEnemyRank EnemyRank = EEnemyRank::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Stats")
	float CurrentHealth = 100.0f;

	/** Maximum poise/hard value. Set to 0 to disable hard-break reactions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hard Value", meta = (ClampMin = "0.0"))
	float MaxHardValue = 100.0f;

	/** Current poise/hard value. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Hard Value")
	float CurrentHardValue = 100.0f;

	/** Delay after a direct hit before hard value starts regenerating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hard Value", meta = (ClampMin = "0.0", Units = "s"))
	float HardValueRecoveryDelay = 0.75f;

	/** Hard value regenerated per second after the recovery delay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hard Value", meta = (ClampMin = "0.0"))
	float HardValueRecoveryRate = 50.0f;

	/** Prevents repeated hard breaks during the same hit reaction window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hard Value", meta = (ClampMin = "0.0", Units = "s"))
	float HardBreakCooldown = 0.35f;

	/** Fallback hard damage multiplier for legacy attacks with no explicit value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Hard Value", meta = (ClampMin = "0.0"))
	float DefaultHardDamageScale = 1.0f;

	/** Single defense value used by the project-wide damage formula. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "0"))
	int32 Defense = 0;

	/** Legacy physical resistance retained for old Blueprint assets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Legacy", meta = (ClampMin = "0", DeprecatedProperty, DeprecationMessage = "Use Defense."))
	int32 PhysicalResistance = 0;

	/** Legacy magic resistance retained for old Blueprint assets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Legacy", meta = (ClampMin = "0", DeprecatedProperty, DeprecationMessage = "Use Defense."))
	int32 MagicResistance = 0;

	/** Pure Ink awarded when this enemy dies during an active Combat Room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Economy|Pure Ink", meta = (ClampMin = "0"))
	int32 PureInkDropAmount = 1;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Enemy|State")
	bool bIsDead = false;

	/** 延迟销毁前的死亡表现/掉落窗口（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Death", meta = (ClampMin = "0.0", Units = "s"))
	float DeathDestroyDelay = 0.5f;

	/** 最近一次直接性伤害的攻击者（供击退等状态读取） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	TObjectPtr<AActor> LastAttacker;

	/** 最近一次伤害信息（供死亡事件携带） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	FTakeDamageInfo LastDamageInfo;

	/** 最近一次伤害来源的攻击区域（供死亡事件携带） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	TObjectPtr<AAttackAreaBase> LastAttackArea;

	/** 最近一次已结算的伤害与硬值结果。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	FEnemyDamageResult LastDamageResult;

	/** 血条使用的 Widget 类；默认使用 C++ 原生 EnemyHealthWidget。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|UI")
	TSubclassOf<UEnemyHealthWidget> HealthWidgetClass;

	/** 血条锚点相对胶囊体顶部的高度偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|UI", meta = (ClampMin = "0.0"))
	float HealthWidgetHeightOffset = 25.0f;

	/**
	 * 血条锚点的左右偏移，使用敌人组件本地 Y 轴。
	 * 正值向本地右侧偏移，负值向本地左侧偏移，单位为厘米。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|UI", meta = (UIMin = "-100.0", UIMax = "100.0"))
	float HealthWidgetHorizontalOffset = 0.0f;

	/** Screen-space normal enemy widget size in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|UI")
	FVector2D HealthWidgetDrawSize = FVector2D(100.0f, 10.0f);

	/** Screen-space elite enemy widget size in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|UI")
	FVector2D EliteHealthWidgetDrawSize = FVector2D(130.0f, 16.0f);

	/** Optional screen-space scale for both normal and elite widgets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|UI", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float HealthWidgetWorldScale = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Attack")
	FTimerHandle AttackTimerHandle;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyDeathSignature OnEnemyDeath;

	/** 一次性死亡入口；掉落生成和死亡事件均从这里发出。 */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyDeathSignature OnDead;

	/** Final health changes; the widget consumes this event instead of polling or raw hit input. */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyHealthChangedSignature OnEnemyHealthChanged;

	/** 每次有效直接性受击事件（保留给 Buff、音效等通用监听者）。 */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnTakeDirectDamageSignature OnTakeDirectDamage;

	/** Unified damage attempt event, including invulnerability blocks. */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyDamageResolvedSignature OnDamageResolved;

	/** 仅在硬值被击破时广播，状态机用此事件决定是否打断当前行为。 */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyHardBreakSignature OnHardBreak;

	/** 攻击范围蓝图类（在蓝图中赋值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack")
	TSubclassOf<AAttackAreaBase> AttackAreaClass;

	/** 攻击间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.5"))
	float AttackInterval = 5.0f;

	/** 执行攻击的距离阈值（小于此值则进入攻击状态） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 300.0f;

	/** 远程敌人的内圈距离；近战敌人保持 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0"))
	float MinimumAttackRange = 0.0f;

	/** 远程敌人的外圈距离；为 0 时复用 AttackRange。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0"))
	float MaximumAttackRange = 0.0f;

	/** 攻击状态中执行攻击后的移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0"))
	float AttackMoveSpeed = 0.0f;

	/** 攻击状态前摇时间（进入 Attack 到执行攻击的停顿） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0", Units = "s"))
	float AttackWindupTime = 0.2f;

	/** 攻击状态后摇时间（执行攻击后到返回 Chase） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0", Units = "s"))
	float AttackRecoveryTime = 0.3f;

	// ── 冲撞型状态机 Slice 4–5 ──

	/** 开启后，Chase 会在冲撞距离带内进入 EnemyState_Charge。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charge")
	bool bUseChargeAttack = false;

	/** 冲撞可开始的最大距离；超出此距离继续 Chase。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charge", meta = (ClampMin = "0.0"))
	float ChargeStartRange = 1100.0f;

	/** 冲撞可开始的最小距离；太近时改走普通 Attack。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charge", meta = (ClampMin = "0.0"))
	float ChargeMinRange = 450.0f;

	/** 冲撞蓄力时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charge", meta = (ClampMin = "0.0", Units = "s"))
	float ChargeWindupTime = 0.65f;

	/** 冲撞阶段水平速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charge", meta = (ClampMin = "0.0"))
	float ChargeSpeed = 1400.0f;

	/** 冲撞阶段最长持续时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charge", meta = (ClampMin = "0.0", Units = "s"))
	float ChargeDuration = 0.75f;

	/** 冲撞结束后的停顿时间，结束后回到 Chase。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Charge", meta = (ClampMin = "0.0", Units = "s"))
	float ChargeRecoveryTime = 0.8f;

	// ── 攻击区域初始化参数 ──

	/** 攻击区域生命周期 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea", meta = (ClampMin = "0.01"))
	float AttackAreaLifeTime = 3.0f;

	/** 攻击区域飞行速度。ESM-1 的默认近战攻击保持为 0；远程蓝图覆盖该值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea", meta = (ClampMin = "0.0"))
	float AttackAreaSpeed = 0.0f;

	/** 攻击区域是否检测障碍物碰撞 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea")
	bool bAttackAreaDetectObstacle = true;

	/** 攻击区域是否为近战。远程敌人蓝图将其设为 false 并配置非零速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea")
	bool bAttackAreaIsMelee = true;

	/** 攻击区域是否跟随施放者 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea")
	bool bAttackAreaFollowOwner = false;

	/** 攻击区域生成位置偏移（沿前方向） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea", meta = (ClampMin = "0.0"))
	float AttackAreaSpawnOffset = 80.0f;

	/** 检测玩家的最小直线距离阈值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI", meta = (ClampMin = "0.0"))
	float DetectRange = 800.0f;

	/** 停止追击的距离阈值（小于此值则停止靠近） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI", meta = (ClampMin = "0.0"))
	float ChaseStopRange = 200.0f;

	/** 继续追击的距离阈值（大于此值则向玩家移动） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI", meta = (ClampMin = "0.0"))
	float ChaseContinueRange = 400.0f;

	/** 追击移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 400.0f;

	/** 旋转插值速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI", meta = (ClampMin = "0.0"))
	float ChaseRotationSpeed = 360.0f;

	/** 击退位移速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI", meta = (ClampMin = "0.0"))
	float HitBackSpeed = 600.0f;

	/** 击退持续时间（秒），结束后回到 Chase */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AI", meta = (ClampMin = "0.0", Units = "s"))
	float HitBackDuration = 0.2f;

	/** 缓存的玩家引用 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|AI")
	TObjectPtr<APlayerCharacter> CachedPlayer;

public:
	UFUNCTION(BlueprintPure, Category = "Enemy|Economy|Pure Ink")
	int32 GetPureInkDropAmount() const { return PureInkDropAmount; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Stats")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Stats")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Identity")
	EEnemyRank GetEnemyRank() const { return EnemyRank; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Hard Value")
	float GetMaxHardValue() const { return MaxHardValue; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Hard Value")
	float GetCurrentHardValue() const { return CurrentHardValue; }

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void TakeDamage(const FTakeDamageInfo& InInfo, AAttackAreaBase* InAttackArea = nullptr);

	/** New unified damage entry; the legacy TakeDamage function adapts into it. */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void TakeDamageContext(const FDamageContext& InContext, AAttackAreaBase* InAttackArea = nullptr);

	/** Read the current movement speed after runtime Slow effects. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Effects")
	float GetEffectiveMoveSpeed(float BaseSpeed) const;

	/** Read the current control-impact multiplier after resistance effects. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Effects")
	float GetControlResistMultiplier() const;

	/** Minimal gameplay healing entry point used by the Phase 1 health contract. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Health")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void TestDie();

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void Die();

	/** Refresh the cached player when the current Pawn is gone or dead. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
	void RefreshCombatTarget();

	UFUNCTION(BlueprintPure, Category = "Enemy|AI")
	bool HasValidCombatTarget() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|AI")
	APlayerCharacter* GetCombatTarget() const { return CachedPlayer; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Charge")
	bool IsChargeAttackInRange(float Distance) const
	{
		return bUseChargeAttack
			&& Distance >= ChargeMinRange
			&& Distance <= ChargeStartRange;
	}

	/** Dead 状态的唯一进入点；保证掉落、事件和延迟销毁只执行一次。 */
	void HandleDeadState();

private:
	UFUNCTION()
	void HandleHomingMarkAdded(const FActiveCombatEffect& Effect);

	UFUNCTION()
	void HandleHomingMarkChanged(const FActiveCombatEffect& Effect);

	UFUNCTION()
	void HandleHomingMarkRemoved(const FActiveCombatEffect& Effect);

	void ActivateHomingMarkVFX(const FActiveCombatEffect& Effect, bool bRestartSystem);
	void DeactivateHomingMarkVFX(const FActiveCombatEffect& Effect);
	void UpdateHomingMarkVFX(float DeltaTime);

	void NormalizeDefenseFromLegacy();
	void BroadcastHealthChanged(float PreviousHealth, EEnemyHealthChangeReason ChangeReason);
	void UpdateHardValue(float DeltaTime);
	float ResolveHardDamage(const FTakeDamageInfo& InInfo) const;
	void DisableStateComponentTicks();
	UStateBase* EnsureStateComponent(TSubclassOf<UStateBase> StateClass);
	void GenerateDropOnDead();
	void DestroyAfterDeath();
	void SetupDissolveMaterials();
	void StartDissolveAnimation();
	void UpdateDissolve();

	bool bDeadHandled = false;
	FTimerHandle DeathDestroyTimerHandle;
	float HardValueRecoveryDelayRemaining = 0.0f;
	float HardBreakCooldownRemaining = 0.0f;
	FCombatEffectHandle HomingMarkVFXHandle;
	float HomingMarkVFXInitialDuration = 0.0f;

	/** 死亡溶解的动态材质实例（BeginPlay 时创建） */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DissolveMaterialInstances;
	float DissolveStartTime = 0.0f;
	FTimerHandle DissolveTimerHandle;
};

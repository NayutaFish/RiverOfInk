// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/GlobalStructs.h"
#include "EnemyBase.generated.h"

class UStaticMeshComponent;
class UCapsuleComponent;
class AAttackAreaBase;
class UStateBase;
class APlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDeathSignature, AActor*, DeadEnemy);

/**
 * Minimal enemy base for the first room-flow pass.
 *
 * Owns health, damage, death notification, and debug death entry points.
 * AI, movement, attacks, animation, and drops are intentionally left out.
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

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	/** 胶囊体（同时也是根组件） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UCapsuleComponent> CapsuleCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Stats")
	float CurrentHealth = 100.0f;

	/** 物理抗性（物理伤害减免值，最终伤害不低于原伤害的5%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "0"))
	int32 PhysicalResistance = 0;

	/** 魔法抗性（百分比减免，最终伤害不低于原伤害的5%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MagicResistance = 0;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Enemy|State")
	bool bIsDead = false;

	/** 最近一次直接性伤害的攻击者（供击退等状态读取） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	TObjectPtr<AActor> LastAttacker;

	/** 最近一次伤害信息（供死亡事件携带） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	FTakeDamageInfo LastDamageInfo;

	/** 最近一次伤害来源的攻击区域（供死亡事件携带） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|State")
	TObjectPtr<AAttackAreaBase> LastAttackArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Attack")
	FTimerHandle AttackTimerHandle;

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnEnemyDeathSignature OnEnemyDeath;

	/** 直接性受击事件（状态类可订阅，如击退） */
	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FOnTakeDirectDamageSignature OnTakeDirectDamage;

	/** 攻击范围蓝图类（在蓝图中赋值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack")
	TSubclassOf<AAttackAreaBase> AttackAreaClass;

	/** 攻击间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.5"))
	float AttackInterval = 5.0f;

	/** 执行攻击的距离阈值（小于此值则进入攻击状态） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 300.0f;

	/** 攻击状态中执行攻击后的移动速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0"))
	float AttackMoveSpeed = 0.0f;

	/** 攻击状态前摇时间（进入 Attack 到执行攻击的停顿） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0", Units = "s"))
	float AttackWindupTime = 0.2f;

	/** 攻击状态后摇时间（执行攻击后到返回 Chase） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Attack", meta = (ClampMin = "0.0", Units = "s"))
	float AttackRecoveryTime = 0.3f;

	// ── 攻击区域初始化参数 ──

	/** 攻击区域生命周期 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea", meta = (ClampMin = "0.01"))
	float AttackAreaLifeTime = 3.0f;

	/** 攻击区域飞行速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea", meta = (ClampMin = "0.0"))
	float AttackAreaSpeed = 500.0f;

	/** 攻击区域是否检测障碍物碰撞 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea")
	bool bAttackAreaDetectObstacle = true;

	/** 攻击区域是否为近战 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|AttackArea")
	bool bAttackAreaIsMelee = false;

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
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void TakeDamage(const FTakeDamageInfo& InInfo, AAttackAreaBase* InAttackArea = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void TestDie();

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void Die();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Attack1.generated.h"

class AAttackArea_PlayerAttack1;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EPlayerAttackPhase : uint8
{
	Startup  UMETA(DisplayName = "Startup"),
	Active   UMETA(DisplayName = "Active"),
	Recovery UMETA(DisplayName = "Recovery")
};

/**
 * 普通攻击状态：按 Startup → Active → Recovery 执行一次攻击，
 * 并在后段接收左键输入以触发二段普通攻击。
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UPlayerState_Attack1 : public UStateBase
{
	GENERATED_BODY()

public:
	UPlayerState_Attack1();

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);
	void OnLmb();
	void OnSpace();

	void StartAttackStep();
	void BeginActivePhase();
	void BeginRecoveryPhase();
	void FinishAttackStep();
	void ClearActiveAttackArea();
	void FaceAttackDirection();
	void SpawnAttackVFX();
	void SwitchAfterAttack();

	/** 攻击前摇，结束后才生成伤害范围。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float AttackStartupTime = 0.08f;

	/** 攻击有效帧，伤害范围只在此阶段存在。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float AttackActiveTime = 0.10f;

	/** 攻击后摇；二段输入在此阶段可被缓存。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Timing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float AttackRecoveryTime = 0.14f;

	/** 左键输入在攻击结束前的有效缓存窗口。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Combo", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float AttackInputBufferWindow = 0.18f;

	/** 当前版本的普通攻击段数上限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Combo", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "2"))
	int32 MaxComboSteps = 2;

	/** 二段攻击伤害倍率；默认保持与一段攻击一致。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Combo", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ComboSecondDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Hitbox", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float ComboSecondHitboxRadiusMultiplier = 1.5f;

	/** PIE 调试开关：显示普通攻击 CollisionSphere 的真实范围。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugDrawHitbox = true;

	/** 调试球体线宽；不影响实际碰撞。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Debug", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float DebugHitboxLineThickness = 2.0f;

	/** 一段调试颜色；青色便于与二段区分。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Debug", meta = (AllowPrivateAccess = "true"))
	FColor FirstHitboxDebugColor = FColor(60, 220, 255, 220);

	/** 二段调试颜色；橙红色便于观察扩大后的范围。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Debug", meta = (AllowPrivateAccess = "true"))
	FColor SecondHitboxDebugColor = FColor(255, 100, 40, 220);

	/** 攻击前摇期间的移动速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackStartupMoveSpeed = 350.0f;

	/** 攻击有效帧期间的移动速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackActiveMoveSpeed = 180.0f;

	/** 攻击后摇期间的移动速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackRecoveryMoveSpeed = 500.0f;

	/** 后摇阶段允许 Space 取消普通攻击并进入冲刺。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|DashCancel", meta = (AllowPrivateAccess = "true"))
	bool bAllowDashCancelInRecovery = true;

	/** 普通攻击挥砍特效；二段未配置专用特效时复用此资源。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> AttackVFX;

	/** 可选的二段专用特效；当前为空时使用 AttackVFX 占位。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> ComboSecondVFX;

	/** 一段 VFX 相对玩家的前向生成偏移；当前值保持原有表现。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|VFX", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float AttackVFXForwardOffset = 60.0f;

	/** 二段 VFX 相对玩家的前向生成偏移；默认与 Hitbox 中心更接近。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|VFX", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float ComboSecondVFXForwardOffset = 100.0f;

	/** 一段 VFX 缩放。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|VFX", meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float AttackVFXScale = 1.0f;

	/** 二段 VFX 缩放；无专用二段资源时也用于放大第一段占位 VFX。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttackState|VFX", meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float ComboSecondVFXScale = 1.5f;

	/** 当前攻击阶段，便于 PIE 时回读参数和定位输入窗口。 */
	UPROPERTY(VisibleAnywhere, Category = "AttackState|Runtime")
	EPlayerAttackPhase CurrentPhase = EPlayerAttackPhase::Startup;

	UPROPERTY(VisibleAnywhere, Category = "AttackState|Runtime")
	int32 ComboStep = 1;

	bool bHadMoveInput = false;
	bool bAttackQueued = false;
	float AttackInputBufferAge = -1.0f;
	float MoveInputX = 0.0f;
	float MoveInputY = 0.0f;

	TObjectPtr<AAttackArea_PlayerAttack1> ActiveAttackArea;
	FTimerHandle AttackTimerHandle;
};

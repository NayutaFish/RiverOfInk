// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/GlobalStructs.h"
#include "PlayerCharacter.generated.h"

class UAnimMontage;
class AAttackAreaBase;
class UStateBase;
class USkillComponent;
class UPlayerInputComponent;
class UPlayerHealthWidget;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDeathSignature, AActor*, DeadPlayer);

UENUM(BlueprintType)
enum class EHikariActionState : uint8
{
	Normal		UMETA(DisplayName = "Normal"),
	Attacking	UMETA(DisplayName = "Attacking"),
	Dodging		UMETA(DisplayName = "Dodging"),
	HitStun		UMETA(DisplayName = "HitStun"),
	Dead		UMETA(DisplayName = "Dead")
};

UCLASS()
class RIVEROFINK_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

	// 当前是否允许移动
	UFUNCTION(BlueprintPure, Category = "State")
	bool CanMove() const;

	// 当前是否允许开始攻击/闪避等动作
	UFUNCTION(BlueprintPure, Category = "State")
	bool CanStartAction() const;

	// 开始攻击
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void BeginAttack();

	// 结束攻击
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void EndAttack();

	UFUNCTION(BlueprintCallable, Category = "Player|Skill")
	void TryCastSkill1();

	UFUNCTION(BlueprintCallable, Category = "Player|Skill")
	void TryCastSkill2();

	UFUNCTION(BlueprintCallable, Category = "Player|Skill")
	void TryCastSkillSlot1();

	UFUNCTION(BlueprintCallable, Category = "Player|Skill")
	void TryCastSkillSlot2();

	UFUNCTION(BlueprintCallable, Category = "Player|Skill")
	void TryCastSkillSlot3();

	UFUNCTION(BlueprintPure, Category = "State")
	bool IsSprinting() const { return bIsSprinting; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Skill")
	TObjectPtr<USkillComponent> SkillComponent;

	// ── 状态机 ──
	/** 当前活跃状态组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	TObjectPtr<UStateBase> CurrentState;

	/** 切换到指定状态（查找对应组件并切入） */
	UFUNCTION(BlueprintCallable, Category = "State")
	void SwitchState(TSubclassOf<UStateBase> StateClass);

	// 普通移动速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 600.0f;

	// 疾跑速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 900.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsSprinting = false;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Create the first-pass health HUD for the locally controlled player. */
	void CreateHealthWidget();

	/** Optional Blueprint subclass for the health HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPlayerHealthWidget> HealthWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerHealthWidget> HealthWidget;

	void Die();
	void OnAttack();
// 攻击动画蒙太奇
// 之后在 BP_Hikari 类默认值里指定为 AM_Hikari_Attack_01
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
TObjectPtr<UAnimMontage> AttackMontage;

	// 攻击被中断时的动画淡出时间
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	float AttackCancelBlendOutTime = 0.08f;

	// 当前角色动作状态
	// 第一版用于判断：攻击中不能移动，攻击中不能再次攻击
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EHikariActionState CurrentActionState = EHikariActionState::Normal;
private:
	// 攻击 Montage 播放结束时调用
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// 统一切换角色动作状态
	void SetActionState(EHikariActionState NewState);

	// 取消当前攻击
	void CancelAttack();

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Stats")
	float CurrentHealth = 100.0f;

	/** 物理抗性（物理伤害减免值，最终伤害不低于原伤害的5%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Stats", meta = (ClampMin = "0"))
	int32 PhysicalResistance = 0;

	/** 魔法抗性（百分比减免，最终伤害不低于原伤害的5%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Stats", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MagicResistance = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|State")
	bool bIsDead = false;

	UPROPERTY(BlueprintAssignable, Category = "Player|Events")
	FOnPlayerDeathSignature OnPlayerDeath;

	UFUNCTION(BlueprintCallable, Category = "Player")
	void TakeDamage(const FTakeDamageInfo& InInfo);

	UFUNCTION(BlueprintCallable, Category = "Player")
	void TestDie();

	/** 攻击范围蓝图类（在蓝图中赋值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TSubclassOf<AAttackAreaBase> AttackAreaClass;

	/** Attack2 攻击范围蓝图类（在蓝图中赋值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TSubclassOf<AAttackAreaBase> Attack2AreaClass;

public:
	/** 是否正在闪避中 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsDashing = false;

	UFUNCTION(BlueprintPure, Category = "State")
	bool IsDashing() const { return bIsDashing; }

	/** 是否可以冲刺 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bCanDash = true;

	/** 冲刺冷却：设置 bCanDash=false，time 秒后恢复 true */
	UFUNCTION(BlueprintCallable, Category = "State")
	void StartDashCooldown(float Time);

	/** 是否可以进行普通攻击 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bCanAttack1 = true;

	/** Attack1 冷却：设置 bCanAttack1=false，time 秒后恢复 true */
	UFUNCTION(BlueprintCallable, Category = "State")
	void StartAttack1Cooldown(float Time);

	/** 是否可以进行 Attack2 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bCanAttack2 = true;

	/** Attack2 冷却：设置 bCanAttack2=false，time 秒后恢复 true */
	UFUNCTION(BlueprintCallable, Category = "State")
	void StartAttack2Cooldown(float Time);
};

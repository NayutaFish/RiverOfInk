// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "Core/GlobalStructs.h"
#include "RoguelikeSystem/PlayerRuntimeData.h"
#include "PlayerCharacter.generated.h"

class UAnimMontage;
class AAttackAreaBase;
class AAttackArea_PlayerAttack1;
class AAttackArea_PlayerAttack2;
class UStateBase;
class UPlayerState_Attack1;
class UPlayerCharacter_CommonAttackManage;
class USkillComponent;
class UHealthComponent;
class UCombatEffectComponent;
class UProjectileTargetingComponent;
class UPlayerInputComponent;
class UPlayerHealthWidget;
class UPlayerSkillWidget;
class ARoguelikeShopManager;


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
	void BeginAttack(UAnimMontage* InMontage = nullptr, bool bRestartMontage = false);

	// 结束攻击
	UFUNCTION(BlueprintCallable, Category = "Attack")
	void EndAttack();

	/** 普攻请求；由普攻管理组件决定进入哪个 attackStage 的 PlayerState_Attack1。 */
	UFUNCTION(BlueprintCallable, Category = "Player|Attack")
	void RequestNormalAttack();

	/** 切换到指定的具体状态组件（不同于按类名查找的 SwitchState）。 */
	UFUNCTION(BlueprintCallable, Category = "State")
	void SwitchToState(UStateBase* NewState);

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

	/** Route an E press through the TwoStageArc input-buffering rules. */
	void RequestSkill2Input();

	/** Consume a buffered stage-2 request at the earliest legal action point. */
	void TryConsumeBufferedCircularSlashStage2Input();

	UFUNCTION(BlueprintPure, Category = "State")
	bool IsSprinting() const { return bIsSprinting; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Skill")
	TObjectPtr<USkillComponent> SkillComponent;

	/** Owns player health, resistance, damage calculation, and health events. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Health")
	TObjectPtr<UHealthComponent> HealthComponent;

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** Runtime Buff/Debuff/Proc container. Gameplay behavior is added by later effect slices. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Effects")
	TObjectPtr<UCombatEffectComponent> CombatEffectComponent;

	UFUNCTION(BlueprintPure, Category = "Player|Effects")
	UCombatEffectComponent* GetCombatEffectComponent() const { return CombatEffectComponent; }

	/** Selects and consumes player-owned homing marks for moving projectiles. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Projectile")
	TObjectPtr<UProjectileTargetingComponent> ProjectileTargetingComponent;

	UFUNCTION(BlueprintPure, Category = "Player|Projectile")
	UProjectileTargetingComponent* GetProjectileTargetingComponent() const { return ProjectileTargetingComponent; }

	/** Capture all component-owned runtime state into one value snapshot. */
	bool CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const;

	/** Apply one value snapshot to the newly initialized player and its components. */
	bool ApplyRuntimeData(const FPlayerRuntimeData& InRuntimeData);

	UFUNCTION(BlueprintPure, Category = "Player|Health")
	bool IsDead() const;

	/** Register the Shop Manager whose interaction area currently contains this player. */
	void SetNearbyShopManager(ARoguelikeShopManager* InShopManager);

	/** Clear an interaction-area registration without disrupting another nearby Shop. */
	void ClearNearbyShopManager(ARoguelikeShopManager* InShopManager);

	/** Trigger the nearby Shop interaction. Bound to the configurable J key by default. */
	UFUNCTION(BlueprintCallable, Category = "Player|Interaction")
	void TryInteractWithShop();

	FText GetShopInteractionKeyLabel() const;

	// ── 状态机 ──
	/** 当前活跃状态组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	TObjectPtr<UStateBase> CurrentState;

	/** Native attack state component; exposed so inherited player Blueprints can edit its defaults. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Attack")
	TObjectPtr<UPlayerState_Attack1> PlayerState_Attack1;

	/** 第二段普通攻击状态组件（attackStage = 2）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Attack")
	TObjectPtr<UPlayerState_Attack1> PlayerState_Attack1_2;

	/** 第三段普通攻击状态组件（attackStage = 3）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Attack")
	TObjectPtr<UPlayerState_Attack1> PlayerState_Attack1_3;

	/** 普通攻击多段管理组件：负责按 attackStage 路由普攻。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Attack")
	TObjectPtr<UPlayerCharacter_CommonAttackManage> CommonAttackManage;

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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Create the first-pass health HUD for the locally controlled player. */
	void CreateHealthWidget();

	/** Create the fixed Q/E skill HUD for the locally controlled player. */
	void CreateSkillWidget();

	/** Optional Blueprint subclass for the health HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPlayerHealthWidget> HealthWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerHealthWidget> HealthWidget;

	/** Optional Blueprint subclass for the skill HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UPlayerSkillWidget> SkillWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerSkillWidget> SkillWidget;

	/** Default Shop interaction key. Kept local to player input so the first UI slice needs no new input asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Interaction")
	FKey ShopInteractionKey = EKeys::J;

	TWeakObjectPtr<ARoguelikeShopManager> NearbyShopManager;

	void Die();
	void OnAttack();
// 攻击动画蒙太奇
// 之后在 BP_Hikari 类默认值里指定为 AM_Hikari_Attack_01
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
TObjectPtr<UAnimMontage> DefaultAttackMontage;

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

	/** Compatibility mirror; UHealthComponent is the health state source of truth. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|State")
	bool bIsDead = false;

	/** 最近一次直接性伤害的攻击者（供击退等状态读取） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|State")
	TObjectPtr<AActor> LastAttacker;

	UPROPERTY(BlueprintAssignable, Category = "Player|Events")
	FOnPlayerDeathSignature OnPlayerDeath;

	/** 直接性受击事件（状态类可订阅，如击退） */
	UPROPERTY(BlueprintAssignable, Category = "Player|Events")
	FOnTakeDirectDamageSignature OnTakeDirectDamage;

	UFUNCTION(BlueprintCallable, Category = "Player")
	void TakeDamage(const FTakeDamageInfo& InInfo);

	/** 是否处于战斗外无敌；战斗开始时解除，战斗结束时启用，受击时若为真则忽略伤害 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|State")
	bool isInBattleInvincible = true;

	UFUNCTION(BlueprintCallable, Category = "Player")
	void TestDie();

	/** 攻击范围类（近战，普攻；在蓝图中赋值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TSubclassOf<AAttackArea_PlayerAttack1> AttackAreaClass;

	/** Attack2 攻击范围类（玩家中心扇形特攻；在蓝图中赋值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TSubclassOf<AAttackArea_PlayerAttack2> Attack2AreaClass;

public:
	/** 是否正在闪避中 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsDashing = false;

	/** 击退位移速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|State", meta = (ClampMin = "0.0"))
	float HitBackSpeed = 600.0f;

	/** 击退持续时间（秒），结束后回到 Idle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|State", meta = (ClampMin = "0.0", Units = "s"))
	float HitBackDuration = 0.2f;

	UFUNCTION(BlueprintPure, Category = "State")
	bool IsDashing() const { return bIsDashing; }

	/** 是否处于直接性伤害无敌状态 */
	UFUNCTION(BlueprintPure, Category = "State")
	bool IsInvincible() const;

	/** Read a movement speed after runtime Slow effects. */
	UFUNCTION(BlueprintPure, Category = "Player|Effects")
	float GetEffectiveMoveSpeed(float BaseSpeed) const;

	/** 是否可以冲刺 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bCanDash = true;

	/** 冲刺冷却：设置 bCanDash=false，固定 0.3 秒后恢复 true */
	UFUNCTION(BlueprintCallable, Category = "State")
	void StartDashCooldown();

	/** 是否可以进行普通攻击 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bCanAttack1 = true;

	/** Attack1 冷却：设置 bCanAttack1=false，固定 0.3 秒后恢复 true */
	UFUNCTION(BlueprintCallable, Category = "State")
	void StartAttack1Cooldown();

	/** 是否可以进行 Attack2 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bCanAttack2 = true;

	/** Attack2 冷却：设置 bCanAttack2=false，固定 0.3 秒后恢复 true */
	UFUNCTION(BlueprintCallable, Category = "State")
	void StartAttack2Cooldown();

private:
	void ApplyRuntimeBuffEffects(const TArray<FRunBuffData>& InRunBuffs);

	UFUNCTION()
	void HandleHealthChanged(float InCurrentHealth, float InMaxHealth);

	UFUNCTION()
	void HandleHealthDeath(AActor* DeadActor);

	UFUNCTION()
	void HandleHealthDirectDamage(const FTakeDamageInfo& InInfo);

	FDelegateHandle CombatRoomStartedHandle;
	FDelegateHandle CombatRoomClearedHandle;

};

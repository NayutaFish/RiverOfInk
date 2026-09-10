// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "PlayerInputComponent.generated.h"

class UInputAction;
class UInputMappingContext;

// ── 输入事件委托（多播，供角色订阅） ──
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerInputAxis, float);
DECLARE_MULTICAST_DELEGATE(FOnPlayerInputAction);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UPlayerInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerInputComponent();

	/** 注册到 Enhanced Input 子系统并绑定回调（由 SetupPlayerInputComponent 调用） */
	void SetupEnhancedInput(UEnhancedInputComponent* EnhancedInput, APlayerController* PC);

	/** Compatibility entry point for the raw UInputComponent mouse path. */
	void DispatchPrimaryAttackInput();

	/** 当前横向移动输入（A/D）轴值，随输入持续刷新。 */
	float GetMoveX() const { return CurrentMoveX; }

	/** 当前纵向移动输入（W/S）轴值，随输入持续刷新。 */
	float GetMoveY() const { return CurrentMoveY; }

	/** 疾跑键是否处于按住状态（进入移动状态时用它恢复疾跑，避免状态切换期间丢按键）。 */
	bool IsSprintHeld() const { return CurrentShiftValue > 0.5f; }

	/**
	 * 移动轴死区：绝对值小于该值的输入视为 0（主要针对手柄摇杆）。
	 * 键盘输出是 ±1，不受影响。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Move", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float MoveAxisDeadZone = 0.2f;

	/**
	 * 当前移动输入合成的世界方向；无输入时返回零向量。
	 * 采用本作固定的等距映射（与 Move / Attack 状态内的算法一致），
	 * 供“需要立刻知道玩家正按着哪个方向”的逻辑使用（例如冲刺方向）。
	 */
	FVector GetMoveWorldDirection() const;

	// ── 委托实例 ──
	FOnPlayerInputAxis OnMoveXDelegate;
	FOnPlayerInputAxis OnMoveYDelegate;
	FOnPlayerInputAxis OnShiftDelegate;
	FOnPlayerInputAction OnLmbDelegate;
	FOnPlayerInputAction OnRmbDelegate;
	FOnPlayerInputAction OnSpaceDelegate;
	FOnPlayerInputAction OnQDelegate;
	FOnPlayerInputAction OnEDelegate;
	FOnPlayerInputAction OnFDelegate;

protected:
	virtual void BeginPlay() override;
	void LoadInputAssets();
	void ValidateInputAssets() const;
	bool bInputSetup = false;

	/** 缓存的移动输入轴值（OnMoveX/OnMoveY 持续刷新），供 GetMoveWorldDirection 等按需查询。 */
	float CurrentMoveX = 0.0f;
	float CurrentMoveY = 0.0f;

	/** 缓存的疾跑键轴值（OnShift 持续刷新）。 */
	float CurrentShiftValue = 0.0f;

	/** 规范化单轴输入：同轴正反键抵消 + 死区。 */
	float SanitizeMoveAxis(float RawValue) const;

	// ── 轴输入（按住持续触发） ──

	/** 横向移动输入（A/D） */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> MoveXAction;

	/** 纵向移动输入（W/S） */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> MoveYAction;

	/** 左 Shift 按住 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> ShiftAction;

	// ── 动作输入（按下一次触发） ──

	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> LmbAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> RmbAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> SpaceAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> QAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> EAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputAction> FAction;

	/** 默认输入映射上下文 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Assets")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// ── 轴回调 ──
	void OnMoveX(const FInputActionValue& Value);
	void OnMoveY(const FInputActionValue& Value);
	void OnShift(const FInputActionValue& Value);

	// ── 动作回调 ──
	void OnLmb();
	void OnRmb();
	void OnSpace();
	void OnQ();
	void OnE();
	void OnF();
	void DispatchLmb();

	// Enhanced Input and the legacy key path can both observe the same physical
	// click while a PIE viewport is gaining focus. Keep one dispatch per short
	// input window so the compatibility path cannot double-trigger a combo.
	double LastLmbDispatchTime = -1.0;
};

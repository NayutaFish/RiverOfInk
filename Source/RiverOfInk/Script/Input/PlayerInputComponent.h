// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "PlayerInputComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class APlayerController;

// ── 输入事件委托（多播，供角色订阅） ──
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerInputAxis, float);
DECLARE_MULTICAST_DELEGATE(FOnPlayerInputAction);

/** 参与“预输入缓冲”的玩家操作。 */
UENUM(BlueprintType)
enum class EPlayerBufferedInput : uint8
{
	None		UMETA(DisplayName = "None"),
	Dash		UMETA(DisplayName = "Dash (Space)"),
	Attack1		UMETA(DisplayName = "Normal Attack (LMB)"),
	Attack2		UMETA(DisplayName = "Secondary (RMB)"),
	Skill1		UMETA(DisplayName = "Skill 1 (Q)"),
	Skill2		UMETA(DisplayName = "Skill 2 (E)")
};

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

	// ── 输入缓冲（预输入 / input buffering）──

	/** 记录一次操作请求（同类型刷新时间戳）；状态在“允许的最早时刻”消费它，避免按键被吞掉。 */
	void BufferInput(EPlayerBufferedInput Input);

	/** 该操作的缓冲是否仍在窗口内（不消费）。 */
	UFUNCTION(BlueprintPure, Category = "Input|Buffer")
	bool HasBufferedInput(EPlayerBufferedInput Input) const;

	/** 消费该操作的缓冲：仍在窗口内返回 true 并清除；已过期返回 false（同样清除）。 */
	UFUNCTION(BlueprintCallable, Category = "Input|Buffer")
	bool ConsumeBufferedInput(EPlayerBufferedInput Input);

	/** 主动丢弃该操作的缓冲（例如已经即时响应过，避免之后重复触发）。 */
	UFUNCTION(BlueprintCallable, Category = "Input|Buffer")
	void ClearBufferedInput(EPlayerBufferedInput Input);

	/** 丢弃全部缓冲。 */
	void ClearAllBufferedInputs();

	/**
	 * 输入缓冲窗口（秒）：决定“提前按下的键”能被保留多久。
	 * 0.2~0.3 秒是动作游戏的常见区间；设为 0 即关闭缓冲（按键会被吞）。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Buffer", meta = (ClampMin = "0.0", Units = "s"))
	float InputBufferWindow = 0.25f;

	/**
	 * 当前移动输入合成的世界方向；无输入时返回零向量。
	 * 采用本作固定的等距映射（与 Move / Attack 状态内的算法一致），
	 * 供“需要立刻知道玩家正按着哪个方向”的逻辑使用（例如冲刺方向）。
	 */
	FVector GetMoveWorldDirection() const;

	// ── 手柄（右摇杆瞄准）──

	/**
	 * 右摇杆推杆方向换算出的世界方向（与移动同一套等距映射：右=屏幕右）。
	 * 摇杆在死区内（或手柄输入被关闭）时返回 false，调用方应回退到鼠标光标逻辑。
	 */
	bool GetGamepadAimWorldDirection(FVector& OutDirection) const;

	/** 右摇杆当前是否超过瞄准死区（用于区分“按住摇杆瞄准”与“松开摇杆保持朝向”）。 */
	bool HasGamepadAimInput() const;

	/**
	 * 玩家现在是不是在用手柄：任意一个手柄键 / 摇杆 / 扳机处于按下或推动状态。
	 * 攻击瞬间用它决定朝向规则——手柄：右摇杆 > 左摇杆（移动方向）> 保持当前朝向，不读鼠标光标；
	 * 键鼠：仍然读鼠标光标。
	 */
	bool IsGamepadInputActive() const;

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

	// ── 手柄映射（键位可在编辑器覆盖）──
	//
	// 手柄映射在运行时用临时 InputMappingContext 组装（见 BuildGamepadMappingContext），
	// 不往 IMC_Player 内容资产里写数据，所以打包后也一定存在，也不会把编辑器资产改脏。

	/** 关闭后完全不注册手柄映射（保留纯键鼠调试）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	bool bEnableGamepadInput = true;

	/** 左摇杆 → 移动（与 WASD 同一条轴，共用原有移动管线）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadMoveXKey = EKeys::Gamepad_LeftX;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadMoveYKey = EKeys::Gamepad_LeftY;

	/** 右摇杆 → 攻击朝向。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadAimXKey = EKeys::Gamepad_RightX;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadAimYKey = EKeys::Gamepad_RightY;

	/** 右扳机 → 左键普攻（IA_Player_Attack）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadAttackKey = EKeys::Gamepad_RightTrigger;

	/** 西键（Xbox X）→ 右键特攻（IA_Player_Secondary）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadSecondaryKey = EKeys::Gamepad_FaceButton_Left;

	/** 左扳机 → 冲刺（IA_Player_Dash，等同 Space）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadDashKey = EKeys::Gamepad_LeftTrigger;

	/** 左肩键 → Q 法术（IA_Player_Skill1）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadSkill1Key = EKeys::Gamepad_LeftShoulder;

	/** 右肩键 → E 斩击（IA_Player_Skill2）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad")
	FKey GamepadSkill2Key = EKeys::Gamepad_RightShoulder;

	/** 右摇杆瞄准死区（摇杆漂移不会改变朝向）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Gamepad", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float AimAxisDeadZone = 0.3f;

	/** 右摇杆瞄准轴：手柄专用，运行时创建的临时 InputAction。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimXAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimYAction;

	/** 运行时组装的手柄映射上下文（左摇杆/右摇杆/扳机/肩键）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> GamepadMappingContext;

	/** 组装上面那个上下文，并把里面的键位挂到已有动作上。 */
	void BuildGamepadMappingContext();

	/** 给上下文加一条映射；键无效或动作为空时跳过。 */
	void MapGamepadKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key);

	/** 缓存的移动输入轴值（OnMoveX/OnMoveY 持续刷新），供 GetMoveWorldDirection 等按需查询。 */
	float CurrentMoveX = 0.0f;
	float CurrentMoveY = 0.0f;

	/** 缓存的右摇杆轴值（OnAimX/OnAimY 持续刷新）。 */
	float CurrentAimX = 0.0f;
	float CurrentAimY = 0.0f;

	/** 建立输入时缓存的本地 PlayerController，供 IsGamepadInputActive 查按键状态。 */
	TWeakObjectPtr<APlayerController> CachedPlayerController;

	/** 缓存的疾跑键轴值（OnShift 持续刷新）。 */
	float CurrentShiftValue = 0.0f;

	/** 各操作最近一次按下的世界时间（秒）；不存在该键 = 没有缓冲。 */
	TMap<EPlayerBufferedInput, double> BufferedInputTimes;

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
	void OnAimX(const FInputActionValue& Value);
	void OnAimY(const FInputActionValue& Value);

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

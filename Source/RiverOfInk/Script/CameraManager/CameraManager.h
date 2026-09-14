// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CameraManager.generated.h"

class USpringArmComponent;
class UCameraComponent;
class ARoguelikeExitTrigger;

/**
 * 俯视角相机管理器（纯 C++）
 *
 * 逻辑与旧项目 Test_GamePlay 同事实现的 CameraRig（BP_TopDownCameraRig）一致：
 *   - 弹簧臂 + 俯视相机，固定俯视角，不随玩家转向旋转
 *   - Tick 中使用 VLerp + Pow 做帧率无关平滑跟随
 *   - 跟随强度分三档：进场（Intro）→ 正常（Normal），Current 运行时过渡
 *   - 仅跟随 XY 平面
 */
UCLASS()
class RIVEROFINK_API ACameraManager : public AActor
{
	GENERATED_BODY()

public:
	ACameraManager();

	// 是否只跟随目标的 XY 平面（Z 保持不变）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	bool bFollowOnlyXY = true;

	// 进场跟随强度（玩家生成时生效，值越小镜头飘向玩家越慢）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float IntroFollowStrength = 3.0f;

	// 正常跟随强度（接管后逐渐过渡到此值，越大跟随越紧）
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float NormalFollowStrength = 12.0f;

	// 当前跟随强度（运行时从 Intro 过渡到 Normal）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	float CurrentFollowStrength = 0.02f;

	// 跟随强度从 Intro 过渡到 Normal 的速度
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float StrengthBlendSpeed = 1.5f;

	/** 相机震动偏移（由 CameraShakeManager 设置，Tick 应用，非震动时为零） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector CurrentShakeOffset = FVector::ZeroVector;

	/** Start the reactive post-reward camera guide for one exit. */
	void StartExitGuide(ARoguelikeExitTrigger* InExitTrigger);

	/** Stop the exit guide and restore the pre-guide camera state. */
	void StopExitGuide();

	bool IsExitGuideActive() const { return bExitGuideActive; }
protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

protected:
	// 当前跟随目标（玩家）
	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor;

	/** 弹簧臂——让相机固定在目标上方一定距离和角度 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 俯视摄像机 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	/** Height above the player actor origin used as the player focus point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.0", Units = "cm"))
	float ExitGuidePlayerFocusHeight = 80.0f;

	/** 0 keeps the composition on the player; 1 keeps it on the exit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExitGuideExitWeight = 0.40f;

	/** Duration of the first blend from the normal camera into the guide composition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.0", Units = "s"))
	float ExitGuideIntroDuration = 0.80f;

	/** Follow response while the player remains free to move. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.0"))
	float ExitGuideFollowStrength = 16.0f;

	/** Faster response when the camera must zoom out to preserve the safe frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.0"))
	float ExitGuideZoomOutStrength = 12.0f;

	/** Slower response when the camera can zoom back in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.0"))
	float ExitGuideZoomInStrength = 4.0f;

	/** Fraction of the full orthographic width reserved for both targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ExitGuideHorizontalSafeFraction = 0.60f;

	/** Fraction of the full orthographic height reserved for both targets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ExitGuideVerticalSafeFraction = 0.70f;

	/** Extra world-space padding on each side of the fitted target bounds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "0.0", Units = "cm"))
	float ExitGuideOrthoPadding = 120.0f;

	/** Absolute upper bound for auto-zooming in a very long room. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Exit Guide", meta = (ClampMin = "1.0", Units = "cm"))
	float ExitGuideMaxOrthoWidth = 3200.0f;

private:
	void TickExitGuide(float DeltaTime);
	bool ResolveExitGuideTargets(FVector& OutPlayerFocus, FVector& OutExitFocus) const;
	float CalculateExitGuideOrthoWidth(
		const FVector& PlayerFocus,
		const FVector& ExitFocus,
		const FVector& CameraCenter) const;
	float GetCurrentViewAspectRatio() const;

	UPROPERTY(Transient)
	TObjectPtr<ARoguelikeExitTrigger> GuideExitTrigger;

	bool bExitGuideActive = false;
	float ExitGuideElapsed = 0.0f;
	FVector ExitGuideStartLocation = FVector::ZeroVector;
	float ExitGuideStartOrthoWidth = 1550.0f;
	float NormalOrthoWidth = 1550.0f;
	/** Shake offset already baked into the actor location on the previous camera tick. */
	FVector AppliedShakeOffset = FVector::ZeroVector;

};

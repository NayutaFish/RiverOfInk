// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Core/GlobalStructs.h"
#include "AttackAreaBase.generated.h"

UENUM()
enum class EAttackAreaDisappearReason : uint8
{
	Lifetime,
	HitEnemy,
	HitObstacle
};

UCLASS(Blueprintable)
class RIVEROFINK_API AAttackAreaBase : public AActor
{
	GENERATED_BODY()

public:
	AAttackAreaBase();

	/** 可视化根组件（不参与碰撞，检测全部走射线，无需配置任何碰撞通道） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float LifeTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float Speed = 0.0f;

	/** 射线检测最小距离：近战的基础检测范围；远程取每帧位移与它的较大值（避免高速漏检） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "1.0"))
	float MinDetectRange = 100.0f;

	/** 伤害信息（编辑器手动赋值，Attacker 由代码填充为施放者） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FTakeDamageInfo DamageInfo;

	/** 只伤害敌对目标？
	 *  true  → 玩家打敌人，敌人打玩家（不会误伤自己人）
	 *  false → 不分敌我，碰到谁伤谁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bDamageOpponentOnly = true;

	/** 检测障碍物？子弹类开启，碰到墙壁自动销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	bool bDetectObstacle = false;

	/** 是否为近战攻击？true=伤害后不销毁，等 LifeTime 结束 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bIsMeleeAttack = false;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Attack")
	void Initialize(float InLifeTime, float InSpeed, bool InIsMeleeAttack = false, AActor* InFollowTarget = nullptr);

protected:
	/** 统一销毁入口，带原因屏幕输出 */
	void Disappear(EAttackAreaDisappearReason Reason);

	UFUNCTION(BlueprintNativeEvent, Category = "Attack")
	void ApplyDamage(AActor* Target);
	virtual void ApplyDamage_Implementation(AActor* Target);

	UFUNCTION(BlueprintNativeEvent, Category = "Filter")
	bool IsValidTarget(AActor* Target);
	virtual bool IsValidTarget_Implementation(AActor* Target);

private:
	float ElapsedTime = 0.0f;

	/** 每帧沿正方向发射射线，命中玩家/敌人且符合过滤条件时结算伤害 */
	void PerformTargetScan(float DeltaTime);

	/** 跟随的目标（非空则每帧同步位置） */
	UPROPERTY()
	TObjectPtr<AActor> FollowTarget;

	/** 跟随目标时的相对偏移 */
	FVector FollowOffset = FVector::ZeroVector;

	/** 已命中的目标（近战攻击在生命期内持续存在，保证同一目标只结算一次伤害） */
	TSet<TWeakObjectPtr<AActor>> HitActors;
};

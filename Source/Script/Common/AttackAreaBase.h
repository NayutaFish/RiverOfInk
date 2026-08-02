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
class TEST_GAMEPLAY_API AAttackAreaBase : public AActor
{
	GENERATED_BODY()

public:
	AAttackAreaBase();

	/** 碰撞体（同时也是根组件） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float LifeTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float Speed = 0.0f;

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

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

protected:
	void SetupCollision();

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

	/** 跟随的目标（非空则每帧同步位置） */
	UPROPERTY()
	TObjectPtr<AActor> FollowTarget;

	/** 跟随目标时的相对偏移 */
	FVector FollowOffset = FVector::ZeroVector;

	/** 已命中的目标（近战攻击在生命期内持续存在，保证同一目标只结算一次伤害） */
	TSet<TWeakObjectPtr<AActor>> HitActors;
};

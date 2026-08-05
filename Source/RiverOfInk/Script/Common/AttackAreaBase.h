// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "Core/GlobalStructs.h"
#include "AttackAreaBase.generated.h"

class UNiagaraSystem;
class APlayerCharacter;

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

	/** 碰撞根组件：命中检测全部走 Overlap，无需额外配置碰撞通道 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float LifeTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float Speed = 0.0f;

	/** 碰撞半径（近战的范围 / 子弹的体积） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "5.0"))
	float Radius = 50.0f;

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

	/** 是否为近战攻击？true=伤害后不销毁，等 LifeTime 结束（持续与目标重叠不重复结算） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bIsMeleeAttack = false;

	/** 命中特效（在敌人位置生成，朝向=攻击者→受击者方向） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|FX")
	TObjectPtr<UNiagaraSystem> HitSpark;

	/** 命中音效名称（对应 AudioDataAsset 配置表中的键名，默认 AttackHit） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|FX")
	FString HitSoundName = TEXT("AttackHit");

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

	UFUNCTION()
	void OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	float ElapsedTime = 0.0f;

	/** 障碍物检测（射线，只查 WorldStatic，不依赖碰撞通道） */
	void PerformObstacleScan(float DeltaTime);

	/** 跟随的目标（非空则每帧同步位置） */
	UPROPERTY()
	TObjectPtr<AActor> FollowTarget;

	/** 跟随目标时的相对偏移 */
	FVector FollowOffset = FVector::ZeroVector;

	/** 已命中的目标（近战攻击在生命期内持续存在，保证同一目标只结算一次伤害） */
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
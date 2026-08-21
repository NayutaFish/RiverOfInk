// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "Core/GlobalStructs.h"
#include "Common/ProjectileTypes.h"
#include "AttackAreaBase.generated.h"

class UNiagaraSystem;
class APlayerCharacter;
class UStaticMeshComponent;

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

	/** 是否使用以攻击区域中心为圆心的扇形判定；碰撞球仅作为候选筛选范围。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shape")
	bool bUseFanHitbox = false;

	/** 扇形半角，最终总角度为此值的两倍。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shape", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float FanHalfAngleDegrees = 45.0f;
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

	/**
	 * Explicit gameplay tag for moving enemy attacks that E's Null Ring is
	 * allowed to erase. Player projectiles and enemy melee areas stay false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Projectile")
	bool bIsEnemyProjectile = false;

	/** Shared movement/targeting contract for player-owned moving projectiles. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attack|Projectile")
	FProjectileSpec ProjectileSpec;

	/** 跟随目标时是否同步目标朝向；扇形判定通常需要开启。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Follow")
	bool bFollowTargetRotation = false;

	/** 命中特效（在敌人位置生成，朝向=攻击者→受击者方向） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|FX")
	TObjectPtr<UNiagaraSystem> HitSpark;

	/** 命中音效名称（对应 AudioDataAsset 配置表中的键名，默认 AttackHit） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|FX")
	FString HitSoundName = TEXT("AttackHit");

	/**
     * PIE 调试用 Hitbox 描线。显示的是 CollisionSphere 的真实世界半径，
     * 不参与碰撞，也不会改变攻击判定。
     */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Hitbox")
	bool bDrawDebugHitbox = false;

	/** 调试描线颜色；DrawDebugSphere 使用此颜色。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Hitbox")
	FColor DebugHitboxColor = FColor(60, 220, 255, 220);

	/** 球形描线的分段数；只影响调试显示，不影响碰撞。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Hitbox", meta = (ClampMin = "8", ClampMax = "64"))
	int32 DebugHitboxSegments = 24;

	/** DrawDebugSphere 线宽。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Hitbox", meta = (ClampMin = "0.1"))
	float DebugHitboxLineThickness = 2.0f;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Attack")
	void Initialize(float InLifeTime, float InSpeed, bool InIsMeleeAttack = false, AActor* InFollowTarget = nullptr);

	/** Initialize a moving projectile from the shared homing-capable spec. */
	UFUNCTION(BlueprintCallable, Category = "Attack|Projectile")
	void InitializeProjectile(const FProjectileSpec& InProjectileSpec);

	/** Disable collision and destroy this attack area if it is an enemy projectile. */
	UFUNCTION(BlueprintCallable, Category = "Attack|Projectile")
	bool NullifyEnemyProjectile();

protected:
	/** 统一销毁入口，带原因屏幕输出 */
	void Disappear(EAttackAreaDisappearReason Reason);

	/** 同步线框球体的尺寸和可见性；线框球体不会参与碰撞。 */
	void UpdateDebugHitboxVisualization();

	/** 绘制与真实扇形判定一致的边界线。 */
	void DrawDebugFanHitbox() const;

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

	/** Rotate toward the spawn-time marked target without changing its position/scale. */
	void UpdateHoming(float DeltaTime);

	/** 跟随的目标（非空则每帧同步位置；可选同步朝向） */
	UPROPERTY()
	TObjectPtr<AActor> FollowTarget;

	/** 跟随目标时的相对偏移 */
	FVector FollowOffset = FVector::ZeroVector;

	/** 已命中的目标（近战攻击在生命期内持续存在，保证同一目标只结算一次伤害） */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** 扇形角度过滤；CollisionSphere 负责粗筛，最终命中由此函数决定。 */
	bool IsTargetWithinFanHitbox(const AActor* Target) const;

	/** 使用 UE 内置 WireframeMaterial 的可视化球体；DrawDebugSphere 负责颜色和高亮。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|Hitbox", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> DebugHitboxMesh;
};

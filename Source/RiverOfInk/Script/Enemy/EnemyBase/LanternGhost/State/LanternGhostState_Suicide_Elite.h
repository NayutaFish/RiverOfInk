// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "LanternGhostState_Suicide_Elite.generated.h"

class AEnemyBase;
class UNiagaraSystem;

/**
 * 精英自爆灯笼怪召唤攻击状态。
 *
 * 继承自 EnemyState_Attack，但不生成攻击区域：
 * 攻击执行时沿自身朝向取“左前”和“右前”两个落点，
 * 先在这两个落点播放 summonVFX 特效，再生成 summonEnemyBase 小怪。
 * 落点会做地面贴合与占位检测，取不到合适位置时按回退缩放取更近的落点。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API ULanternGhostState_Suicide_Elite : public UEnemyState_Attack
{
	GENERATED_BODY()

public:
	ULanternGhostState_Suicide_Elite();

	/** 召唤出来的小怪类（EnemyBase 子类）；未配置时退回基类普通攻击 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon")
	TSubclassOf<AEnemyBase> summonEnemyBase;

	/** 召唤落点播放的 Niagara 特效（未设置则只生成小怪） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon")
	TObjectPtr<UNiagaraSystem> summonVFX;

	/** 召唤落点沿自身朝向前方的距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float SummonForwardOffset = 250.0f;

	/** 召唤落点相对自身朝向左右两侧的横向偏移（左前、右前各一个） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float SummonSideOffset = 180.0f;

	/** 首选落点不可用时，按该比例缩小前后/左右偏移再尝试一次 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SummonFallbackOffsetScale = 0.5f;

	/** 特效播放后到小怪真正生成的延迟；<= 0 表示与特效同时生成 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (ClampMin = "0.0", Units = "s"))
	float SummonSpawnDelay = 0.5f;

	/** 是否把落点贴合到地面 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon")
	bool bSnapSummonToGround = true;

	/** 地面检测向上探测距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float SummonGroundTraceUp = 300.0f;

	/** 地面检测向下探测距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float SummonGroundTraceDown = 800.0f;

	/**
	 * 贴地后沿 Z 轴的额外留空（叠加在“胶囊底面贴地”之上）。
	 * 留一点正间隙很重要：胶囊底面与地面刚好相切时，带 Sweep 的位移仍可能被判为起点重叠而完全走不动。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (Units = "cm"))
	float SummonGroundZOffset = 10.0f;

	/**
	 * 允许的最大地面高度差；超过则认为地面太远（屋顶/深渊/浮空），
	 * 此时保留自身高度而不是强行贴地。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float SummonMaxGroundHeightDelta = 600.0f;

	/** 是否对落点做占位检测（避免生成在墙体内部） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon")
	bool bCheckSummonClearance = true;

	/** 落点占位检测球半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon", meta = (ClampMin = "0.0", Units = "cm"))
	float SummonClearanceRadius = 50.0f;

	/** 宿主死亡时，是否让仍然存活的召唤物各自调用自己的死亡流程（Die） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide|Summon")
	bool bKillSummonsOnOwnerDeath = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;

	/** 攻击行为：在左右前方生成特效与小怪，不再生成攻击区域 */
	virtual void ExecuteAttack() override;

	/** 宿主死亡：让仍存活的召唤物一起走各自的死亡流程 */
	UFUNCTION()
	void HandleOwnerDeath(AActor* DeadEnemy);

private:
	/** 计算左前/右前两个落点；每侧至少产出一个可生成位置 */
	int32 BuildSummonTransforms(const AEnemyBase& Enemy, TArray<FTransform>& OutTransforms) const;

	/** 把理想落点修正为可用落点（高度处理 + 占位检测）；失败返回 false */
	bool ResolveSummonTransform(
		const FVector& DesiredLocation,
		const FRotator& DesiredRotation,
		FTransform& OutTransform) const;

	/** 落点高度：优先贴合地面，找不到合适地面时保留自身高度 */
	FVector ResolveSummonLocation(const FVector& InLocation) const;

	/** 召唤物胶囊半高：贴地时用它把胶囊中心抬到地面之上 */
	float ResolveSummonHalfHeight() const;

	/**
	 * 生成前用“召唤物实例自己的胶囊”做最后校正：
	 * 只要胶囊与静态物重叠就按 10cm 逐级顶出来。
	 * Chase 用带 Sweep 的 AddActorWorldOffset，起点一旦重叠就永远走不动。
	 */
	FTransform MakeNonBlockingSpawnTransform(const AEnemyBase* Summoned, const FTransform& InTransform) const;

	/** 落点是否没有与静态物体重叠 */
	bool IsSummonLocationClear(const FVector& Location) const;

	/** 在暂存落点上真正生成小怪，并登记进召唤物列表 */
	void SpawnPendingSummons();

	/** 攻击后摇结束后回到追击的计时器 */
	FTimerHandle AttackReturnHandle;

	/** 召唤延迟生成小怪的计时器（不随状态退出清理） */
	FTimerHandle SummonSpawnTimerHandle;

	/** 等待 SummonSpawnDelay 期间暂存的落点 */
	TArray<FTransform> PendingSummonTransforms;

	/** 本状态召唤出来、尚未清理的召唤物（宿主死亡时统一处理） */
	UPROPERTY()
	TArray<TObjectPtr<AEnemyBase>> SummonedEnemies;

	/** 宿主已死亡：不再补生成召唤物 */
	bool bOwnerDead = false;
};

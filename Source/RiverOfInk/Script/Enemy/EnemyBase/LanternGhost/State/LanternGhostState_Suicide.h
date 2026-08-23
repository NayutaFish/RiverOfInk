// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "LanternGhostState_Suicide.generated.h"

/**
 * 灯笼怪自爆攻击状态。
 * 继承自 EnemyState_Attack；进入该状态 0.1 秒后生成爆炸攻击区域并触发死亡。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API ULanternGhostState_Suicide : public UEnemyState_Attack
{
	GENERATED_BODY()

public:
	ULanternGhostState_Suicide();

	/** 爆炸攻击区域半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 250.0f;

	/** 爆炸攻击区域存在时长（近战式，一次性结算范围内目标） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide", meta = (ClampMin = "0.0", Units = "s"))
	float ExplosionLifetime = 0.5f;

	/** 爆炸音效名称（对应 AudioDataAsset 配置表中的键名） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Suicide")
	FString ExplosionSoundName = TEXT("Explosion");

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;

private:
	/** 自爆：进入状态 0.1 秒后生成爆炸攻击区域并触发死亡 */
	void Detonate();

	FTimerHandle DetonateTimerHandle;
};

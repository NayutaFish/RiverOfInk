// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Attack.h"
#include "LanternGhostState_Melee.generated.h"

/**
 * 灯笼怪近战攻击状态。
 * 继承自 EnemyState_Attack；进入该状态时播放冲刺（Dash）音效。
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API ULanternGhostState_Melee : public UEnemyState_Attack
{
	GENERATED_BODY()

public:
	ULanternGhostState_Melee();

	/** 冲刺（Dash）音效名称（对应 AudioDataAsset 配置表中的键名） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
	FString DashSoundName = TEXT("Dash");

protected:
	virtual void OnEnter_Implementation() override;
};

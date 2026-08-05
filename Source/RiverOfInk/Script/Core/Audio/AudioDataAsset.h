// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AudioDataAsset.generated.h"

/**
 * 音频配置资产：字符串名称 → 音频资源 的映射表
 *
 * 在编辑器中创建 DataAsset 后，在 AudioMap 中添加键值对：
 *   键 = 音频名称（如 "AttackHit"、"BGM_Main"）
 *   值 = 音频资源（USoundWave / USoundCue 均可）
 */
UCLASS(BlueprintType)
class RIVEROFINK_API UAudioDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 音频名称 → 音频资源（编辑器可编辑的键值列表） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TMap<FString, TObjectPtr<USoundBase>> AudioMap;
};

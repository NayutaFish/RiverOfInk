// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UAudioDataAsset;
class UWorld;

/**
 * 音频管理器（纯静态类，非 UObject）
 *
 * 用法（一行播放 2D 音效，无空间衰减，全屏可听）：
 *     FAudioManager::Play(TEXT("AttackHit"));
 *
 * 配置资产按约定路径懒加载：
 *    Content/DataAsset/AudioData/ 目录下新建基于 UAudioDataAsset 的 DataAsset
 *    蓝图（默认名称 DA_AudioData），Play 时自动加载，无需手动初始化。
 *    若资产名不同，可调用 SetConfigPath 指定。
 *
 * 未加载到配置 / 名称不存在时静默跳过并输出警告，不影响调用方逻辑。
 */
class RIVEROFINK_API FAudioManager
{
public:
	/** 指定配置资产完整路径（含资产名，如 /Game/DataAsset/AudioData/DA_AudioData） */
	static void SetConfigPath(const FString& InAssetPath);

	/**
	 * 按名称播放 2D 音效（直接播到可听位置，不考虑 3D 左右声道）
	 * @param AudioName      配置表中的音频名称
	 * @param bRandomizePitchVolume 为 true 时音量与音调在 80%~120% 间随机
	 */
	static void Play(const FString& AudioName, bool bRandomizePitchVolume = false);

private:
	/** 懒加载配置资产（未加载过才尝试） */
	static void EnsureConfigLoaded();

	/** 配置资产路径（约定默认值） */
	static FString ConfigPath;

	/** 配置资产（强引用，防止 GC 回收） */
	static TStrongObjectPtr<UAudioDataAsset> Config;

	/** 世界上下文（用于 PlaySound2D） */
	static TWeakObjectPtr<UWorld> WorldContext;
};

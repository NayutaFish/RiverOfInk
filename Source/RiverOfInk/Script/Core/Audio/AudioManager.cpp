// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/Audio/AudioManager.h"
#include "Core/Audio/AudioDataAsset.h"
#include "RiverOfInk.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"

FString FAudioManager::ConfigPath = TEXT("/Game/DataAsset/AudioData/DA_AudioData.DA_AudioData");
TStrongObjectPtr<UAudioDataAsset> FAudioManager::Config = nullptr;
TWeakObjectPtr<UWorld> FAudioManager::WorldContext = nullptr;

void FAudioManager::SetConfigPath(const FString& InAssetPath)
{
	ConfigPath = InAssetPath;
	// 路径变更后强制下次重新加载
	Config.Reset();
}

void FAudioManager::EnsureConfigLoaded()
{
	if (Config || ConfigPath.IsEmpty())
	{
		return;
	}

	UAudioDataAsset* Loaded = LoadObject<UAudioDataAsset>(nullptr, *ConfigPath);
	if (Loaded)
	{
		Config.Reset(Loaded);
		UE_LOG(LogRiverOfInk, Log, TEXT("AudioManager: Config loaded from '%s'."), *ConfigPath);
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("AudioManager: Failed to load config at '%s'. Create a DataAsset (UAudioDataAsset) there."), *ConfigPath);
	}
}

void FAudioManager::Play(const FString& AudioName, bool bRandomizePitchVolume)
{
	// 懒加载配置
	EnsureConfigLoaded();

	// 未加载到配置
	if (!Config)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("AudioManager: Config not loaded. Playing '%s' skipped."), *AudioName);
		return;
	}

	// 名称不存在
	const TObjectPtr<USoundBase>* FoundSound = Config->AudioMap.Find(AudioName);
	if (!FoundSound || !*FoundSound)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("AudioManager: No sound registered for '%s'."), *AudioName);
		return;
	}

	// 世界上下文（兜底 GWorld）
	UWorld* World = WorldContext.Get();
	if (!World)
	{
		World = GWorld;
	}
	if (!World)
	{
		UE_LOG(LogRiverOfInk, Error, TEXT("AudioManager: No world context for playing '%s'."), *AudioName);
		return;
	}

	// 随机化时音调与音量在 80%~120% 间随机，否则为 1.0
	const float VolumeMultiplier = bRandomizePitchVolume ? FMath::FRandRange(0.8f, 1.2f) : 1.0f;
	const float PitchMultiplier = bRandomizePitchVolume ? FMath::FRandRange(0.8f, 1.2f) : 1.0f;

	UGameplayStatics::PlaySound2D(World, *FoundSound, VolumeMultiplier, PitchMultiplier);
	UE_LOG(LogRiverOfInk, Log, TEXT("AudioManager: Play '%s'."), *AudioName);
}

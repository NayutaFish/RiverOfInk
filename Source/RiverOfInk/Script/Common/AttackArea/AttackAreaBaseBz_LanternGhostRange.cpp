// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/AttackArea/AttackAreaBaseBz_LanternGhostRange.h"

#include "NiagaraComponent.h"

AAttackAreaBaseBz_LanternGhostRange::AAttackAreaBaseBz_LanternGhostRange()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAttackAreaBaseBz_LanternGhostRange::BeginPlay()
{
	Super::BeginPlay();

	ScaleElapsedTime = 0.0f;

	// 获取攻击区域蓝图下唯一的 Niagara 子物体，并设置初始缩放值。
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents<UNiagaraComponent>(NiagaraComponents);
	if (NiagaraComponents.Num() > 0)
	{
		ChildNiagara = NiagaraComponents[0];
		ChildNiagara->SetVariableFloat(ScaleSizeParameterName, ScaleSizeStart);
	}
}

void AAttackAreaBaseBz_LanternGhostRange::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 出生后 ScaleSize 在 ScaleSizeDuration 秒内从 ScaleSizeStart 渐变到 ScaleSizeEnd。
	if (ChildNiagara)
	{
		ScaleElapsedTime += DeltaTime;
		const float ScaleAlpha = FMath::Clamp(ScaleElapsedTime / FMath::Max(0.01f, ScaleSizeDuration), 0.0f, 1.0f);
		const float ScaleValue = FMath::Lerp(ScaleSizeStart, ScaleSizeEnd, ScaleAlpha);
		ChildNiagara->SetVariableFloat(ScaleSizeParameterName, ScaleValue);
	}
}

float AAttackAreaBaseBz_LanternGhostRange::GetMovementTimeScale(float RealTime) const
{
	// 先正常飞行 SlowStartDelay 秒，再进入 SlowDuration 秒的减速窗口。
	if (RealTime >= SlowStartDelay
		&& RealTime < SlowStartDelay + SlowDuration)
	{
		return FMath::Max(0.0f, InitialSpeedScale);
	}

	return 1.0f;
}

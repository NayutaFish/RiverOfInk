// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeshFloating.generated.h"

class UMeshComponent;

/**
 * 让 Actor 下所有 Mesh 在 Z 方向做上下往复漂浮的组件。
 *
 * 挂到蓝图后会自动收集自身所有 UMeshComponent（StaticMesh / SkeletalMesh），
 * 并在 Tick 中围绕各自初始位置做正弦漂浮。
 */
UCLASS(ClassGroup = (Common), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UMeshFloating : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeshFloating();

	/** 漂浮最大位移（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshFloating", meta = (ClampMin = "0.0"))
	float FloatAmplitude = 30.0f;

	/** 完成一次上下往复（一个完整正弦周期）的时间（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshFloating", meta = (ClampMin = "0.01", Units = "s"))
	float FloatCycleTime = 2.0f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> Meshes;

	TArray<FVector> OriginalRelativeLocations;

	float ElapsedTime = 0.0f;
};

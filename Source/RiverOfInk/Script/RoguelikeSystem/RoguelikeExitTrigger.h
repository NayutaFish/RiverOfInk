// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeExitTrigger.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class APlayerCharacter;
class ARoguelikeRewardManager;

/**
 * Whitebox exit trigger for the first post-reward flow slice.
 *
 * The trigger listens for a successfully applied reward, then accepts a player
 * overlap and delegates the actual level travel to the GameInstance-level
 * level-flow subsystem. This actor does not own the level sequence.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API ARoguelikeExitTrigger : public AActor
{
	GENERATED_BODY()

public:
	ARoguelikeExitTrigger();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
	TObjectPtr<USphereComponent> TriggerSphere;

	/** 传送门检测半径（球形检测范围） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit", meta = (ClampMin = "10.0"))
	float TriggerRadius = 100.0f;

	/** Whitebox marker shown only after this exit becomes usable. It has no collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit|Visual")
	TObjectPtr<UStaticMeshComponent> ExitMarkerMesh;

	/** 传送门视觉蓝图（编辑器赋值；未设置时回退到代码圆柱） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit|Visual")
	TSubclassOf<AActor> ExitVisualClass;

	/** 已生成的传送门视觉实例（由 ExitVisualClass 生成） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit|Visual")
	TObjectPtr<AActor> ExitVisualActor;

	/** Optional explicit reference; BeginPlay falls back to the first manager in the level. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Exit|Reward")
	TObjectPtr<ARoguelikeRewardManager> RewardManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit|Runtime")
	bool bIsActivated = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit|Runtime")
	bool bHasTriggered = false;

	UFUNCTION(BlueprintCallable, Category = "Exit")
	void ActivateExit();

	UFUNCTION(BlueprintPure, Category = "Exit")
	bool IsExitActivated() const { return bIsActivated; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleRewardApplied(const FRoguelikeRewardOption& Reward);

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

private:
	bool ResolveRewardManager();
	void HandlePlayerEntered(APlayerCharacter* Player);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeExitTrigger.generated.h"

class UBoxComponent;
class ARoguelikeRewardManager;

/**
 * Whitebox exit trigger for the first post-reward flow slice.
 *
 * The trigger listens for a successfully applied reward, then accepts a player
 * overlap and reports it through LogRoguelike. Level travel is intentionally
 * not owned by this actor yet.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API ARoguelikeExitTrigger : public AActor
{
	GENERATED_BODY()

public:
	ARoguelikeExitTrigger();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
	TObjectPtr<UBoxComponent> TriggerBox;

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
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/StateBase.h"
#include "PlayerState_Skill2.generated.h"

/** Skill 2 state: play the attack animation and release CircularSlash. */
UCLASS(meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UPlayerState_Skill2 : public UStateBase
{
	GENERATED_BODY()

protected:
	virtual void OnEnter_Implementation() override;
	virtual void OnExit_Implementation() override;
	virtual void Update_Implementation(float DeltaTime) override;

private:
	void OnMoveX(float Value);
	void OnMoveY(float Value);
	void OnSkillTimer();

	bool bHadMoveInput = false;
	FVector SlideDirection = FVector::ZeroVector;
	FTimerHandle SkillTimerHandle;
};

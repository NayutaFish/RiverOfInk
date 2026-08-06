// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeSystem/RoguelikeRunTypes.h"
#include "RiverOfInkGameMode.generated.h"

class ADemoRoomManager;
class ARoguelikeRewardManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnRoguelikeRoomStateChanged,
	ERoguelikeRoomState,
	PreviousState,
	ERoguelikeRoomState,
	NewState
);

UCLASS()
class RIVEROFINK_API ARiverOfInkGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARiverOfInkGameMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Room Flow")
	ERoguelikeRoomState CurrentRoomState = ERoguelikeRoomState::Initializing;

	UPROPERTY(BlueprintAssignable, Category = "Roguelike|Room Flow|Events")
	FOnRoguelikeRoomStateChanged OnRoomStateChanged;

	UFUNCTION(BlueprintPure, Category = "Roguelike|Room Flow")
	ERoguelikeRoomState GetRoomState() const { return CurrentRoomState; }

	UFUNCTION(BlueprintCallable, Category = "Roguelike|Room Flow")
	bool TransitionRoomState(ERoguelikeRoomState NextState);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleRunStateChanged(
		ERoguelikeRunState PreviousState,
		ERoguelikeRunState NewState,
		ERoguelikeRunTransitionReason Reason
	);

	UFUNCTION()
	void HandleRoomStarted();

	UFUNCTION()
	void HandleRoomCleared();

	UFUNCTION()
	void HandleRewardApplied(const FRoguelikeRewardOption& Reward);

private:
	void BindRoomActors();
	bool IsRoomTransitionAllowed(ERoguelikeRoomState NextState) const;

	UPROPERTY(Transient)
	TObjectPtr<ADemoRoomManager> BoundRoomManager;

	UPROPERTY(Transient)
	TObjectPtr<ARoguelikeRewardManager> BoundRewardManager;
};

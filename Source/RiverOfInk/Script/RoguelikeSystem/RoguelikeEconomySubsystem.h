// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RoguelikeSystem/RoguelikeEconomyTypes.h"
#include "RoguelikeEconomySubsystem.generated.h"

class AEnemyBase;
struct FNonPlayerDiedEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnPureInkChanged,
	int32,
	PreviousBalance,
	int32,
	NewBalance,
	int32,
	Delta,
	EPureInkChangeReason,
	Reason
);

/**
 * Owns the current run's Pure Ink wallet and its balance-changing transactions.
 *
 * The subsystem survives level travel because it belongs to the GameInstance.
 * Pawn components and UI request transactions through this owner instead of
 * mutating a local copy of the balance.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API URoguelikeEconomySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Clear all economy progress at a new-run boundary. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Economy")
	void ResetForNewRun();

	/** Add a positive amount to the wallet. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Economy")
	bool AddPureInk(int32 Amount, EPureInkChangeReason Reason);

	/** Spend a positive amount if the wallet can afford it. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Economy")
	bool TrySpendPureInk(int32 Amount, EPureInkChangeReason Reason);

	/** Grant the fixed result reward for a room, once per MajorStage/Room pair. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Economy")
	bool GrantRoomResult(int32 MajorStageIndex, int32 RoomIndex, int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Roguelike|Economy")
	int32 GetPureInkBalance() const { return Wallet.Balance; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Economy")
	FPureInkWallet GetPureInkWallet() const { return Wallet; }

	UPROPERTY(BlueprintAssignable, Category = "Roguelike|Economy|Events")
	FOnPureInkChanged OnPureInkChanged;

private:
	void HandleEnemyDied(const FNonPlayerDiedEvent& InEvent);
	bool ApplyBalanceDelta(int32 Delta, EPureInkChangeReason Reason);
	bool IsCombatRoomActive(int32& OutMajorStageIndex, int32& OutRoomIndex) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Economy", meta = (AllowPrivateAccess = "true"))
	FPureInkWallet Wallet;

	/** Weak keys prevent the GameInstance subsystem from keeping dead enemies alive. */
	TSet<TWeakObjectPtr<AEnemyBase>> ProcessedEnemyDrops;

	/** Prevent duplicate room-result grants when a clear event is repeated. */
	TSet<uint64> GrantedRoomResults;

	FDelegateHandle EnemyDeathDelegateHandle;
	bool bEnemyDeathSubscribed = false;
};

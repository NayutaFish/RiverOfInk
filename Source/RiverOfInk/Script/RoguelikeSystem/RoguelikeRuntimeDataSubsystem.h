// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RoguelikeSystem/PlayerRuntimeData.h"
#include "RoguelikeRuntimeDataSubsystem.generated.h"

class APlayerCharacter;

DECLARE_LOG_CATEGORY_EXTERN(LogRoguelikeRuntimeData, Log, All);

/**
 * Owns the current run's player snapshot across level travel.
 *
 * The subsystem stores plain value data, not a Pawn or Component reference.
 * A newly spawned PlayerCharacter first initializes its Blueprint defaults,
 * then asks this subsystem to apply an existing snapshot or register its
 * defaults as the first snapshot of the run.
 */
UCLASS(Blueprintable)
class RIVEROFINK_API URoguelikeRuntimeDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Capture live player values and replace the registered run snapshot. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Runtime Data")
	bool CapturePlayerRuntimeData(const APlayerCharacter* Player);

	/** Register an already-built value snapshot. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Runtime Data")
	bool RegisterPlayerRuntimeData(const FPlayerRuntimeData& InRuntimeData);

	/** Apply the registered snapshot to a newly initialized player. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Runtime Data")
	bool ApplyRegisteredPlayerRuntimeData(APlayerCharacter* Player) const;

	/** Clear the snapshot when an external run-restart system starts a new run. */
	UFUNCTION(BlueprintCallable, Category = "Roguelike|Runtime Data")
	void ResetPlayerRuntimeData();

	UFUNCTION(BlueprintPure, Category = "Roguelike|Runtime Data")
	bool HasPlayerRuntimeData() const { return bHasPlayerRuntimeData; }

	UFUNCTION(BlueprintPure, Category = "Roguelike|Runtime Data")
	FPlayerRuntimeData GetPlayerRuntimeData() const { return PlayerRuntimeData; }

	const FPlayerRuntimeData& GetPlayerRuntimeDataRef() const { return PlayerRuntimeData; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Runtime Data", meta = (AllowPrivateAccess = "true"))
	FPlayerRuntimeData PlayerRuntimeData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Roguelike|Runtime Data", meta = (AllowPrivateAccess = "true"))
	bool bHasPlayerRuntimeData = false;
};

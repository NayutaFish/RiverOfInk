// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/GlobalStructs.h"
#include "RoguelikeSystem/PlayerRuntimeData.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChangedSignature, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthDeathSignature, AActor*, DeadActor);

/**
 * Reusable health and damage component.
 *
 * The component owns effective health values and resistance-based damage
 * calculation. The owning actor remains responsible for actor-specific death
 * presentation, destruction, and gameplay events.
 */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	virtual void BeginPlay() override;

	/** Initialize a newly spawned owner from its configured maximum health. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void InitializeHealth();

	/** Apply one damage request after resistance calculation. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeDamage(const FTakeDamageInfo& InInfo);

	/** Mark this health pool dead and notify the owner. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Die();

	/** Apply a cross-level runtime snapshot after the owner has initialized. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetRuntimeHealthData(
		float InMaxHealth,
		float InCurrentHealth,
		int32 InPhysicalResistance,
		int32 InMagicResistance);

	/** Copy the health-owned fields into the aggregate run snapshot. */
	void CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const;

	/** Apply the health-owned fields from the aggregate run snapshot. */
	void ApplyRuntimeData(const FPlayerRuntimeData& InRuntimeData);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetCurrentHealth(float InCurrentHealth);

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	int32 GetPhysicalResistance() const { return PhysicalResistance; }

	UFUNCTION(BlueprintPure, Category = "Health")
	int32 GetMagicResistance() const { return MagicResistance; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health|Stats")
	float CurrentHealth = 100.0f;

	/** Physical damage reduction. Final damage is never lower than 5%. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Stats", meta = (ClampMin = "0"))
	int32 PhysicalResistance = 0;

	/** Magic damage reduction percentage. Final damage is never lower than 5%. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Stats", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MagicResistance = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health|State")
	bool bIsDead = false;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnHealthDeathSignature OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FOnTakeDirectDamageSignature OnTakeDirectDamage;

private:
	void BroadcastHealthChanged();
};

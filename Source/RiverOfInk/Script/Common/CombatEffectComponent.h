// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/CombatEffectTypes.h"
#include "CombatEffectComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatEffectEventSignature, const FActiveCombatEffect&, Effect);

/**
 * Generic runtime container for temporary Buffs, Debuffs, and Proc effects.
 *
 * Slice 0-1 owns identity, lifetime, stacking, events, and query APIs only.
 * Damage, movement, projectile, and AI behavior processors are intentionally
 * kept outside this component until their dedicated migration slices.
 */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API UCombatEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatEffectComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Apply a new effect or resolve it against an existing effect from the same source. */
	UFUNCTION(BlueprintCallable, Category = "Combat Effects")
	FCombatEffectHandle ApplyEffect(const FCombatEffectSpec& Spec);

	/** Remove one active effect by handle. */
	UFUNCTION(BlueprintCallable, Category = "Combat Effects")
	bool RemoveEffect(FCombatEffectHandle Handle);

	/** Replace the remaining duration without changing stacks or charges. */
	UFUNCTION(BlueprintCallable, Category = "Combat Effects")
	bool RefreshEffect(FCombatEffectHandle Handle, float NewDuration);

	/** Consume charges from a charge-based effect; removes it when charges reach zero. */
	UFUNCTION(BlueprintCallable, Category = "Combat Effects")
	bool ConsumeEffectCharge(FCombatEffectHandle Handle, int32 ChargeCount = 1);

	UFUNCTION(BlueprintPure, Category = "Combat Effects")
	bool HasEffect(FGameplayTag EffectTag) const;

	/** Returns the total stack count across active effects carrying this tag. */
	UFUNCTION(BlueprintPure, Category = "Combat Effects")
	int32 GetEffectStackCount(FGameplayTag EffectTag) const;

	/** Returns the first matching active effect, useful for reading its source/spec. */
	UFUNCTION(BlueprintPure, Category = "Combat Effects")
	bool TryGetEffect(FGameplayTag EffectTag, FActiveCombatEffect& OutEffect) const;

	/** Runtime state is visible for UI/debugging; mutation goes through the APIs above. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Effects")
	TArray<FActiveCombatEffect> ActiveEffects;

	UPROPERTY(BlueprintAssignable, Category = "Combat Effects|Events")
	FOnCombatEffectEventSignature OnEffectAdded;

	UPROPERTY(BlueprintAssignable, Category = "Combat Effects|Events")
	FOnCombatEffectEventSignature OnEffectChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat Effects|Events")
	FOnCombatEffectEventSignature OnEffectRemoved;

private:
	int32 NextHandleId = 1;

	int32 FindEffectIndex(FCombatEffectHandle Handle) const;
	int32 FindMatchingEffectIndex(const FCombatEffectSpec& Spec) const;

	static bool UsesDuration(ECombatEffectDurationPolicy Policy);
	static bool UsesCharges(ECombatEffectDurationPolicy Policy);
	static bool IsValidSpec(const FCombatEffectSpec& Spec);
	static FActiveCombatEffect MakeActiveEffect(const FCombatEffectSpec& Spec, FCombatEffectHandle Handle);

	FCombatEffectHandle MakeHandle();
	void UpdateComponentTickState();
};

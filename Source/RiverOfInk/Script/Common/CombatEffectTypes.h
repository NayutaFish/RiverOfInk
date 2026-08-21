// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/GlobalEnums.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "CombatEffectTypes.generated.h"

/** Broad classification used by UI, rules, and future effect processors. */
UENUM(BlueprintType)
enum class ECombatEffectCategory : uint8
{
	Buff,
	Debuff,
	Proc
};

/** A runtime effect can expire by time, charges, or both. */
UENUM(BlueprintType)
enum class ECombatEffectDurationPolicy : uint8
{
	Infinite,
	Timed,
	Charges,
	TimedAndCharges
};

/** Defines what a new application does when the same effect is already active. */
UENUM(BlueprintType)
enum class ECombatEffectStackPolicy : uint8
{
	Ignore,
	Replace,
	RefreshDuration,
	AddStack,
	AddStackAndRefresh,
	KeepStrongest
};

/** Declarative modifier operation; application to gameplay attributes is a later slice. */
UENUM(BlueprintType)
enum class ECombatEffectModifierOperation : uint8
{
	Add,
	Multiply,
	Override
};

/** Stable runtime identity. Handles remain valid while an active effect is refreshed or stacked. */
USTRUCT(BlueprintType)
struct FCombatEffectHandle
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Effect")
	int32 Id = INDEX_NONE;

	bool IsValid() const { return Id != INDEX_NONE; }
	bool operator==(const FCombatEffectHandle& Other) const { return Id == Other.Id; }
	bool operator!=(const FCombatEffectHandle& Other) const { return !(*this == Other); }
};

FORCEINLINE uint32 GetTypeHash(const FCombatEffectHandle& Handle)
{
	return ::GetTypeHash(Handle.Id);
}

/** One declarative attribute change carried by an effect spec. */
USTRUCT(BlueprintType)
struct FCombatEffectModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Modifier")
	FGameplayTag AttributeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Modifier")
	ECombatEffectModifierOperation Operation = ECombatEffectModifierOperation::Add;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Modifier")
	float Magnitude = 0.0f;
};

/** Damage payload carried by proc effects such as NextHitBonusDamage. */
USTRUCT(BlueprintType)
struct FCombatEffectDamagePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Damage")
	float DamageValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Damage", meta = (ClampMin = "0.0"))
	float HardDamageValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Damage")
	EDamageType DamageType = EDamageType::Unified;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Damage")
	bool bCanCauseDeath = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Damage")
	bool bIsDirectDamage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Damage")
	bool bIgnoreInvulnerability = false;
};

/** Immutable application request consumed by UCombatEffectComponent. */
USTRUCT(BlueprintType)
struct FCombatEffectSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect")
	FGameplayTag EffectTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect")
	ECombatEffectCategory Category = ECombatEffectCategory::Buff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Lifetime")
	ECombatEffectDurationPolicy DurationPolicy = ECombatEffectDurationPolicy::Infinite;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Lifetime")
	ECombatEffectStackPolicy StackPolicy = ECombatEffectStackPolicy::RefreshDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Lifetime", meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Lifetime", meta = (ClampMin = "0"))
	int32 Charges = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Lifetime", meta = (ClampMin = "1"))
	int32 StackCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Lifetime", meta = (ClampMin = "1"))
	int32 MaxStacks = 1;

	/** Optional source used to keep independently applied effects separate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Source")
	TObjectPtr<AActor> SourceActor;

	/** Generic scalar for processors such as damage-up, slow strength, or homing strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Magnitude")
	float Magnitude = 0.0f;

	/** Optional one-hit payload used by proc effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Damage")
	FCombatEffectDamagePayload DamagePayload;

	/** Tags that later systems may query when building damage/projectile/AI specs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Tags")
	FGameplayTagContainer AffectsTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Tags")
	FGameplayTagContainer GrantedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Tags")
	FGameplayTagContainer BlockedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Effect|Modifier")
	TArray<FCombatEffectModifier> Modifiers;
};

/** Mutable runtime state corresponding to one applied effect spec. */
USTRUCT(BlueprintType)
struct FActiveCombatEffect
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Effect")
	FCombatEffectHandle Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Effect")
	FCombatEffectSpec Spec;

	/** Negative means this effect has no time limit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Effect|Runtime")
	float RemainingTime = -1.0f;

	/** Negative means this effect has no charge limit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Effect|Runtime")
	int32 RemainingCharges = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Effect|Runtime")
	int32 CurrentStackCount = 1;

	bool HasFiniteDuration() const { return RemainingTime >= 0.0f; }
	bool HasCharges() const { return RemainingCharges >= 0; }
};

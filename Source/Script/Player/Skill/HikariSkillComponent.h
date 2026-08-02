// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Skill/SkillTypes.h"
#include "HikariSkillComponent.generated.h"

class AAttackAreaBase;
class AHikariPlayerCharacter;
class ASkillCircleDamageArea;

DECLARE_LOG_CATEGORY_EXTERN(LogSkill, Log, All);

/**
 * Owns the first-pass active skill cooldowns and spawning logic.
 * Input remains on the player character so input assets can be configured independently.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class TEST_GAMEPLAY_API UHikariSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHikariSkillComponent();

	UFUNCTION(BlueprintCallable, Category = "Player|Skill")
	void TryCastSkill1();

	UFUNCTION(BlueprintCallable, Category = "Player|Skill")
	void TryCastSkill2();

	UFUNCTION(BlueprintCallable, Category = "Skill")
	void TryCastSkillSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Skill")
	bool HasSkill(EPlayerSkillID SkillID) const;

	UFUNCTION(BlueprintPure, Category = "Skill")
	bool HasEmptySkillSlot() const;

	UFUNCTION(BlueprintCallable, Category = "Skill")
	bool AddSkillToFirstEmptySlot(EPlayerSkillID SkillID);

	UFUNCTION(BlueprintPure, Category = "Skill")
	int32 FindSkillSlot(EPlayerSkillID SkillID) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Upgrade")
	bool CanApplyUpgrade(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType) const;

	UFUNCTION(BlueprintCallable, Category = "Skill|Upgrade")
	void ApplySkillUpgrade(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType);

	UFUNCTION(BlueprintPure, Category = "Skill|Upgrade")
	FSkillUpgradeState GetSkillUpgradeState(EPlayerSkillID SkillID) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Parameters")
	int32 GetTripleProjectileCount() const;

	UFUNCTION(BlueprintPure, Category = "Skill|Parameters")
	float GetTripleProjectileCooldown() const;

	UFUNCTION(BlueprintPure, Category = "Skill|Parameters")
	float GetCircularSlashRadius() const;

	UFUNCTION(BlueprintPure, Category = "Skill|Parameters")
	float GetCircularSlashCooldown() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Slots")
	TArray<FPlayerSkillSlot> SkillSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Upgrade")
	TMap<EPlayerSkillID, FSkillUpgradeState> SkillUpgradeStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash")
	TSubclassOf<ASkillCircleDamageArea> CircularSlashAreaClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash", meta = (ClampMin = "0.0"))
	float CircularSlashCooldown = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash", meta = (ClampMin = "0.0"))
	float CircularSlashDamage = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash", meta = (ClampMin = "1.0"))
	float CircularSlashRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash", meta = (ClampMin = "0.01"))
	float CircularSlashLifeTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile")
	TSubclassOf<AAttackAreaBase> ProjectileAttackAreaClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile", meta = (ClampMin = "0.0"))
	float TripleProjectileCooldown = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile", meta = (ClampMin = "0.0"))
	float TripleProjectileSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile", meta = (ClampMin = "0.01"))
	float TripleProjectileLifeTime = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float TripleProjectileSpreadAngle = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile")
	float ProjectileSpawnForwardOffset = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile", meta = (ClampMin = "0.0"))
	float ProjectileSpawnSideOffset = 35.0f;

protected:
	virtual void BeginPlay() override;

public:
	bool IsOnCooldown(EPlayerSkillID SkillID, float Cooldown) const;

private:
	bool CanCastSkill() const;
	bool CastCircularSlash();
	bool CastTripleProjectile();
	bool SpawnProjectile(const FVector& SpawnLocation, const FVector& Direction, const TCHAR* ProjectileLabel);
	void InitializeSkillSlots();
	int32 GetMaxUpgradeLevel(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType) const;

	UPROPERTY(Transient)
	TObjectPtr<AHikariPlayerCharacter> OwnerCharacter;

	TMap<EPlayerSkillID, double> LastCastTimes;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/ProjectileTypes.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "RoguelikeSystem/PlayerRuntimeData.h"
#include "TimerManager.h"
#include "SkillComponent.generated.h"

class AAttackAreaBase;
class APlayerCharacter;
class APlayerSkill_CircleDamageArea;
class APlayerSkill_ThrownGrenade;
class UNiagaraSystem;

DECLARE_LOG_CATEGORY_EXTERN(LogSkill, Log, All);
DECLARE_MULTICAST_DELEGATE(FOnSkillStateChanged);

/**
 * Owns the first-pass active skill cooldowns and spawning logic.
 * Input remains on the player character so input assets can be configured independently.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class RIVEROFINK_API USkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillComponent();

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

	UFUNCTION(BlueprintPure, Category = "Skill|Build")
	int32 GetModifierStack(EPlayerSkillID SkillID, ESkillModifierID ModifierID) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Build")
	bool CanApplyModifier(EPlayerSkillID SkillID, ESkillModifierID ModifierID, int32 StackDelta = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Skill|Build")
	bool ApplyModifier(EPlayerSkillID SkillID, ESkillModifierID ModifierID, int32 StackDelta = 1);

	/** Copy skill slots, modifiers, and upgrade levels into the aggregate run snapshot. */
	void CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const;

	/** Apply skill slots, modifiers, and upgrade levels from the aggregate run snapshot. */
	void ApplyRuntimeData(const FPlayerRuntimeData& InRuntimeData);

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

	/** Stage-2 input window after a valid TwoStageArc stage-1 hit. */
	UFUNCTION(BlueprintPure, Category = "Skill|Parameters")
	float GetTwoStageArcStage2InputWindow() const;

	UFUNCTION(BlueprintPure, Category = "Skill|Form")
	EPlayerSkillForm GetSkillForm(EPlayerSkillID SkillID) const;

	/** True while the first TwoStageArc release is waiting for a hit result. */
	UFUNCTION(BlueprintPure, Category = "Skill|Form")
	bool IsCircularSlashStage1Active() const;

	/** True after stage 1 hit and before the stage 2 release is confirmed. */
	UFUNCTION(BlueprintPure, Category = "Skill|Form")
	bool IsCircularSlashStage2Ready() const;

	/** Resolve the current runtime state used by gameplay HUDs. */
	UFUNCTION(BlueprintPure, Category = "Skill|State")
	EPlayerSkillRuntimeState GetSkillRuntimeState(EPlayerSkillID SkillID) const;

	/**
	 * Returns whether the E input may enter the skill state right now.
	 * TwoStageArc keeps stage 1 input-locked while its hit window resolves,
	 * while an unlocked stage 2 bypasses the normal cooldown gate.
	 */
	UFUNCTION(BlueprintPure, Category = "Skill|Form")
	bool CanTriggerCircularSlashInput() const;

	/** True when a target form belongs to this skill and differs from its current form. */
	UFUNCTION(BlueprintPure, Category = "Skill|Form")
	bool CanApplySkillForm(EPlayerSkillID SkillID, EPlayerSkillForm NewForm) const;

	/** Set a persistent skill form. Reward systems call this instead of editing slots directly. */
	UFUNCTION(BlueprintCallable, Category = "Skill|Form")
	bool ApplySkillForm(EPlayerSkillID SkillID, EPlayerSkillForm NewForm);

	/** Effective cooldown after current roguelike upgrades. */
	UFUNCTION(BlueprintPure, Category = "Skill|Cooldown")
	float GetSkillCooldown(EPlayerSkillID SkillID) const;

	/** Remaining cooldown in seconds; zero means the skill is ready. */
	UFUNCTION(BlueprintPure, Category = "Skill|Cooldown")
	float GetRemainingSkillCooldown(EPlayerSkillID SkillID) const;

	/** Resolve the current build into deterministic, one-cast parameters. */
	UFUNCTION(BlueprintPure, Category = "Skill|Resolved")
	FResolvedSkillSpec ResolveSkillSpec(EPlayerSkillID SkillID) const;

	/** Presentation-safe summaries derived from the same build state used by casts. */
	UFUNCTION(BlueprintPure, Category = "Skill|Resolved")
	FText GetSkillBuildSummary(EPlayerSkillID SkillID) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Resolved")
	FText GetResolvedSkillSummary(EPlayerSkillID SkillID) const;

	/** Native notification for HUDs and other runtime observers. */
	FOnSkillStateChanged OnSkillStateChanged;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Slots")
	TArray<FPlayerSkillSlot> SkillSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Upgrade")
	TMap<EPlayerSkillID, FSkillUpgradeState> SkillUpgradeStates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash")
	TSubclassOf<APlayerSkill_CircleDamageArea> CircularSlashAreaClass;

	/** Dedicated close-range diagonal ink slash for E. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|VFX")
	TObjectPtr<UNiagaraSystem> CircularSlashVFX;

	/** Core tint passed to the dedicated E Niagara system. The sheet and ink layers keep their authored user-variable palette. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|VFX")
	FLinearColor CircularSlashVFXColor = FLinearColor(0.0f, 0.004f, 0.12f, 0.90f);

	/** Primary Niagara user parameter for the sharp inner slash core. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|VFX")
	FName CircularSlashVFXColorParameter = TEXT("User.Color_Spiral1");

	/** Keeps the inner edge close to the player while the authored arc extends outward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|VFX", meta = (ClampMin = "0.0", Units = "cm"))
	float CircularSlashVFXForwardOffset = 12.0f;

	/** Uniform scale for the authored E arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|VFX", meta = (ClampMin = "0.01"))
	float CircularSlashVFXScale = 1.15f;

	/** Fixed diagonal bias relative to the current attack direction until an animation socket trail is available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|VFX", meta = (ClampMin = "-180.0", ClampMax = "180.0", Units = "deg"))
	float CircularSlashVFXYawOffset = -35.0f;

	/** E 技能施放音效名称（对应 AudioDataAsset 配置表中的键名，留空则跳过） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|Audio")
	FString ECastSoundName = TEXT("CircleSlash");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash", meta = (ClampMin = "0.0"))
	float CircularSlashCooldown = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash", meta = (ClampMin = "0.0"))
	float CircularSlashDamage = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash", meta = (ClampMin = "1.0"))
	float CircularSlashRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash", meta = (ClampMin = "0.01"))
	float CircularSlashLifeTime = 1.0f;

	/** Legacy timing value kept for serialized/editor compatibility; the current TwinSlash is simultaneous extra judgment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|TwinSlash", meta = (ClampMin = "0.0", Units = "s"))
	float TwinSlashDelay = 0.18f;

	/** Damage multiplier applied to every judgment when TwinSlash is present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|TwinSlash", meta = (ClampMin = "0.0"))
	float TwinSlashSecondDamageMultiplier = 0.65f;

	/** The delayed hit is placed in this yaw direction relative to the first cast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|TwinSlash", meta = (ClampMin = "-180.0", ClampMax = "180.0", Units = "deg"))
	float TwinSlashSecondYawOffset = 35.0f;

	/** Offset the delayed circular hit so the yaw angle has gameplay impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|TwinSlash", meta = (ClampMin = "0.0", Units = "cm"))
	float TwinSlashSecondForwardOffset = 110.0f;

	/** Base damage multiplier for each stage of the TwoStageArc form. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|TwoStageArc", meta = (ClampMin = "0.0"))
	float TwoStageArcStageDamageMultiplier = 0.8f;

	/** Smaller radial reach used by each TwoStageArc judgment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|TwoStageArc", meta = (ClampMin = "1.0", Units = "cm"))
	float TwoStageArcRadius = 200.0f;

	/** Horizontal half-angle of each TwoStageArc judgment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|TwoStageArc", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float TwoStageArcHalfAngle = 65.0f;

	/** Time available to press E again after stage 1 confirms a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|CircularSlash|TwoStageArc", meta = (ClampMin = "0.8", ClampMax = "3.0", UIMin = "0.8", UIMax = "3.0", Units = "s"))
	float TwoStageArcStage2InputWindow = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile")
	TSubclassOf<AAttackAreaBase> ProjectileAttackAreaClass;

	/** Q 技能施放音效名称（对应 AudioDataAsset 配置表中的键名，留空则跳过） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|Audio")
	FString QCastSoundName = TEXT("CircleMagic");

	/** Legacy Q form actor. New builds select it through the InkGrenade modifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade")
	TSubclassOf<APlayerSkill_ThrownGrenade> ThrownGrenadeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade", meta = (ClampMin = "0.0"))
	float ThrownGrenadeSpeed = 850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade")
	float ThrownGrenadeGravityZ = -980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade", meta = (ClampMin = "0.05"))
	float ThrownGrenadeFuseTime = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade", meta = (ClampMin = "1.0"))
	float ThrownGrenadeExplosionRadius = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade", meta = (ClampMin = "0.0"))
	float ThrownGrenadeDamage = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade", meta = (ClampMin = "0.0", Units = "s"))
	float ThrownGrenadeExplosionDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade", meta = (ClampMin = "1.0"))
	float ThrownGrenadeCollisionRadius = 32.0f;

	/** 雷电球飞行特效；在编辑器里赋值后，会传给每个抛出的雷电球。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade|Visual")
	TObjectPtr<UNiagaraSystem> ThrownGrenadeNiagaraSystem;

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

	/** Constant turn rate for marked-target homing projectiles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|Homing", meta = (ClampMin = "0.0", Units = "deg/s"))
	float ProjectileHomingTurnRate = 360.0f;

	/** Delay before Q projectiles begin steering toward the spawn-time marked target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|Homing", meta = (ClampMin = "0.0", Units = "s"))
	float ProjectileHomingStartDelay = 0.06f;

	/** Maximum target distance at which Q projectiles may keep steering. Zero disables this limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|Homing", meta = (ClampMin = "0.0"))
	float ProjectileHomingMaxDistance = 2500.0f;

	/** Stop steering once a Q projectile enters this radius around its locked target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|Homing", meta = (ClampMin = "0.0"))
	float ProjectileHomingAcceptanceRadius = 80.0f;

	/** Horizontal turn rate for the Ink Grenade targeted-arc guidance mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade|Guidance", meta = (ClampMin = "0.0", Units = "deg/s"))
	float ThrownGrenadeGuidanceTurnRate = 150.0f;

	/** Delay before a targeted Ink Grenade begins correcting its landing direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade|Guidance", meta = (ClampMin = "0.0", Units = "s"))
	float ThrownGrenadeGuidanceStartDelay = 0.06f;

	/** Maximum target distance for targeted Ink Grenade guidance. Zero disables this limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade|Guidance", meta = (ClampMin = "0.0"))
	float ThrownGrenadeGuidanceMaxDistance = 2500.0f;

	/** Horizontal acceptance radius at which a targeted Ink Grenade stops steering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade|Guidance", meta = (ClampMin = "0.0"))
	float ThrownGrenadeGuidanceAcceptanceRadius = 120.0f;

	/** Half-width of the three targeted Ink Grenade landing points around the mark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|TripleProjectile|ThrownGrenade|Guidance", meta = (ClampMin = "0.0"))
	float ThrownGrenadeGuidanceSpread = 80.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	bool IsOnCooldown(EPlayerSkillID SkillID, float Cooldown) const;

private:
	bool CanCastSkill() const;
	bool CastCircularSlash();
	bool CastCircularSlashStage2();
	bool SpawnCircularSlashSet(const FResolvedSkillSpec& Spec, int32 StageIndex, bool bListenForStage1Hit);
	bool SpawnCircularSlash(
		const FTransform& SpawnTransform,
		float Radius,
		float Damage,
		bool bNullifyEnemyProjectiles,
		bool bListenForStage1Hit,
		bool bUseArcHitbox,
		float ArcHalfAngle);
	void SpawnCircularSlashVFX(const FTransform& SpawnTransform);
	void HandleCircularSlashStage1Hit(AActor* HitActor);
	void ResolveCircularSlashStage1Miss();
	void ResolveCircularSlashStage2Timeout();
	bool CastTripleProjectile();
	bool CastThrownGrenade(const FResolvedSkillSpec& Spec);
	bool SpawnProjectile(
		const FVector& SpawnLocation,
		const FVector& Direction,
		const FProjectileSpec& ProjectileSpec,
		const TCHAR* ProjectileLabel);
	bool HasProjectileHoming(EPlayerSkillID SkillID) const;
	void InitializeSkillSlots();
	int32 GetMaxUpgradeLevel(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType) const;
	int32 GetModifierStackForSlot(const FPlayerSkillSlot& Slot, ESkillModifierID ModifierID) const;
	int32 GetMaxModifierStack(EPlayerSkillID SkillID, ESkillModifierID ModifierID) const;
	void NormalizeSkillModifiers();
	void MigrateLegacySkillForms();
	void AddModifierIfMissing(FPlayerSkillSlot& Slot, ESkillModifierID ModifierID, int32 StackCount);
	FString BuildModifierSummary(EPlayerSkillID SkillID) const;

	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> OwnerCharacter;

	TMap<EPlayerSkillID, double> LastCastTimes;

	FTimerHandle CircularSlashStage1ResolutionTimerHandle;
	bool bCircularSlashStage1Active = false;
	bool bCircularSlashStage2Ready = false;
};

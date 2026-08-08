// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Skill/SkillComponent.h"

#include "Common/AttackAreaBase.h"
#include "Engine/World.h"
#include "Player/PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Skill/PlayerSkill_CircleDamageArea.h"
#include "Player/Skill/PlayerSkill_ThrownGrenade.h"

DEFINE_LOG_CATEGORY(LogSkill);

USkillComponent::USkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CircularSlashAreaClass = APlayerSkill_CircleDamageArea::StaticClass();
	ProjectileAttackAreaClass = AAttackAreaBase::StaticClass();
	ThrownGrenadeClass = APlayerSkill_ThrownGrenade::StaticClass();
	InitializeSkillSlots();
}

void USkillComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<APlayerCharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogSkill, Error, TEXT("SkillComponent requires an APlayerCharacter owner."));
	}
}

void USkillComponent::InitializeSkillSlots()
{
	// The current run has exactly two active skills. Slot order is the input
	// contract: slot 0 is Q and slot 1 is E.
	SkillSlots.SetNum(2);
	SkillSlots[0].SkillID = EPlayerSkillID::TripleProjectile;
	SkillSlots[0].SkillLevel = 1;
	SkillSlots[0].SkillForm = EPlayerSkillForm::ThrownGrenade;
	SkillSlots[1].SkillID = EPlayerSkillID::CircularSlash;
	SkillSlots[1].SkillLevel = 1;
	SkillSlots[1].SkillForm = EPlayerSkillForm::Default;
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::TripleProjectile);
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::CircularSlash);
	UE_LOG(LogSkill, Log,
		TEXT("Skill slots initialized: Slot 0 (Q) = TripleProjectile [ThrownGrenade], Slot 1 (E) = CircularSlash."));
}

void USkillComponent::CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const
{
	OutRuntimeData.SkillSlots = SkillSlots;
	OutRuntimeData.SkillUpgradeStates = SkillUpgradeStates;

	UE_LOG(LogSkill, Log,
		TEXT("Skill runtime data captured: Owner=%s Slots=%d Upgrades=%d."),
		*GetNameSafe(GetOwner()),
		OutRuntimeData.SkillSlots.Num(),
		OutRuntimeData.SkillUpgradeStates.Num());
}

void USkillComponent::ApplyRuntimeData(const FPlayerRuntimeData& InRuntimeData)
{
	SkillSlots = InRuntimeData.SkillSlots;
	SkillUpgradeStates = InRuntimeData.SkillUpgradeStates;

	// Older snapshots may contain an empty third slot or no CircularSlash slot.
	// Normalize them to the current fixed two-skill contract before gameplay uses
	// the data. Upgrade levels remain owned by the runtime snapshot.
	SkillSlots.SetNum(2);
	SkillSlots[0].SkillID = EPlayerSkillID::TripleProjectile;
	SkillSlots[0].SkillLevel = FMath::Max(1, SkillSlots[0].SkillLevel);
	SkillSlots[1].SkillID = EPlayerSkillID::CircularSlash;
	SkillSlots[1].SkillLevel = FMath::Max(1, SkillSlots[1].SkillLevel);
	// SkillForm is intentionally preserved from the snapshot. A missing field
	// in an older snapshot deserializes as Default and keeps the old spread
	// projectile behavior as a backwards-compatible fallback.
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::TripleProjectile);
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::CircularSlash);
	LastCastTimes.Reset();

	UE_LOG(LogSkill, Log,
		TEXT("Skill runtime data applied: Owner=%s Slots=%d Upgrades=%d."),
		*GetNameSafe(GetOwner()),
		SkillSlots.Num(),
		SkillUpgradeStates.Num());
	OnSkillStateChanged.Broadcast();
}

void USkillComponent::TryCastSkill1()
{
	TryCastSkillSlot(0);
}

void USkillComponent::TryCastSkill2()
{
	TryCastSkillSlot(1);
}

void USkillComponent::TryCastSkillSlot(int32 SlotIndex)
{
	if (!SkillSlots.IsValidIndex(SlotIndex) || !CanCastSkill())
	{
		return;
	}

	const EPlayerSkillID SkillID = SkillSlots[SlotIndex].SkillID;
	if (SkillID == EPlayerSkillID::None)
	{
		return;
	}

	const float Cooldown = SkillID == EPlayerSkillID::TripleProjectile
		? GetTripleProjectileCooldown()
		: GetCircularSlashCooldown();
	if (IsOnCooldown(SkillID, Cooldown))
	{
		UE_LOG(LogSkill, Display, TEXT("%s is on cooldown."), *UEnum::GetValueAsString(SkillID));
		return;
	}

	const bool bCastSucceeded = SkillID == EPlayerSkillID::TripleProjectile
		? CastTripleProjectile()
		: CastCircularSlash();
	if (bCastSucceeded)
	{
		LastCastTimes.Add(SkillID, GetWorld()->GetTimeSeconds());
		OnSkillStateChanged.Broadcast();
	}
}

bool USkillComponent::HasSkill(EPlayerSkillID SkillID) const
{
	return FindSkillSlot(SkillID) != INDEX_NONE;
}

bool USkillComponent::HasEmptySkillSlot() const
{
	return FindSkillSlot(EPlayerSkillID::None) != INDEX_NONE;
}

bool USkillComponent::AddSkillToFirstEmptySlot(EPlayerSkillID SkillID)
{
	if (SkillID == EPlayerSkillID::None || HasSkill(SkillID))
	{
		return false;
	}

	const int32 EmptySlotIndex = FindSkillSlot(EPlayerSkillID::None);
	if (EmptySlotIndex == INDEX_NONE)
	{
		return false;
	}

	SkillSlots[EmptySlotIndex].SkillID = SkillID;
	SkillSlots[EmptySlotIndex].SkillLevel = 1;
	SkillUpgradeStates.FindOrAdd(SkillID);
	UE_LOG(LogSkill, Log, TEXT("Added %s to skill slot %d."), *UEnum::GetValueAsString(SkillID), EmptySlotIndex + 1);
	OnSkillStateChanged.Broadcast();
	return true;
}

int32 USkillComponent::FindSkillSlot(EPlayerSkillID SkillID) const
{
	return SkillSlots.IndexOfByPredicate([SkillID](const FPlayerSkillSlot& Slot)
	{
		return Slot.SkillID == SkillID;
	});
}

bool USkillComponent::CanApplyUpgrade(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType) const
{
	if (!HasSkill(SkillID) || UpgradeType == ESkillUpgradeType::None || UpgradeType == ESkillUpgradeType::Damage)
	{
		return false;
	}

	const FSkillUpgradeState State = GetSkillUpgradeState(SkillID);
	const int32 CurrentLevel = UpgradeType == ESkillUpgradeType::Mechanic ? State.MechanicLevel : State.CooldownLevel;
	return CurrentLevel < GetMaxUpgradeLevel(SkillID, UpgradeType);
}

void USkillComponent::ApplySkillUpgrade(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType)
{
	if (!CanApplyUpgrade(SkillID, UpgradeType))
	{
		return;
	}

	FSkillUpgradeState& State = SkillUpgradeStates.FindOrAdd(SkillID);
	if (UpgradeType == ESkillUpgradeType::Mechanic)
	{
		++State.MechanicLevel;
	}
	else if (UpgradeType == ESkillUpgradeType::Cooldown)
	{
		++State.CooldownLevel;
	}

	const int32 SlotIndex = FindSkillSlot(SkillID);
	if (SkillSlots.IsValidIndex(SlotIndex))
	{
		++SkillSlots[SlotIndex].SkillLevel;
	}
	UE_LOG(LogSkill, Log, TEXT("Upgraded %s: Mechanic=%d Cooldown=%d."), *UEnum::GetValueAsString(SkillID), State.MechanicLevel, State.CooldownLevel);
	OnSkillStateChanged.Broadcast();
}

FSkillUpgradeState USkillComponent::GetSkillUpgradeState(EPlayerSkillID SkillID) const
{
	if (const FSkillUpgradeState* State = SkillUpgradeStates.Find(SkillID))
	{
		return *State;
	}
	return FSkillUpgradeState();
}

int32 USkillComponent::GetTripleProjectileCount() const
{
	return FMath::Min(7, 3 + GetSkillUpgradeState(EPlayerSkillID::TripleProjectile).MechanicLevel * 2);
}

float USkillComponent::GetTripleProjectileCooldown() const
{
	return FMath::Max(2.0f, TripleProjectileCooldown - GetSkillUpgradeState(EPlayerSkillID::TripleProjectile).CooldownLevel * 0.5f);
}

float USkillComponent::GetCircularSlashRadius() const
{
	return FMath::Min(440.0f, CircularSlashRadius + GetSkillUpgradeState(EPlayerSkillID::CircularSlash).MechanicLevel * 60.0f);
}

float USkillComponent::GetCircularSlashCooldown() const
{
	return FMath::Max(1.6f, CircularSlashCooldown - GetSkillUpgradeState(EPlayerSkillID::CircularSlash).CooldownLevel * 0.4f);
}

EPlayerSkillForm USkillComponent::GetSkillForm(EPlayerSkillID SkillID) const
{
	const int32 SlotIndex = FindSkillSlot(SkillID);
	return SkillSlots.IsValidIndex(SlotIndex)
		? SkillSlots[SlotIndex].SkillForm
		: EPlayerSkillForm::Default;
}

float USkillComponent::GetSkillCooldown(EPlayerSkillID SkillID) const
{
	switch (SkillID)
	{
	case EPlayerSkillID::TripleProjectile:
		return GetTripleProjectileCooldown();
	case EPlayerSkillID::CircularSlash:
		return GetCircularSlashCooldown();
	default:
		return 0.0f;
	}
}

float USkillComponent::GetRemainingSkillCooldown(EPlayerSkillID SkillID) const
{
	const UWorld* World = GetWorld();
	const double* LastCastTime = LastCastTimes.Find(SkillID);
	if (!World || !LastCastTime)
	{
		return 0.0f;
	}

	const float Remaining = GetSkillCooldown(SkillID)
		- static_cast<float>(World->GetTimeSeconds() - *LastCastTime);
	return FMath::Max(0.0f, Remaining);
}

bool USkillComponent::CanCastSkill() const
{
	return IsValid(OwnerCharacter)
		&& !OwnerCharacter->IsDead()
		&& OwnerCharacter->CanStartAction()
		&& !OwnerCharacter->IsSprinting();
}

bool USkillComponent::IsOnCooldown(EPlayerSkillID SkillID, float Cooldown) const
{
	const UWorld* World = GetWorld();
	const double* LastCastTime = LastCastTimes.Find(SkillID);
	return World && LastCastTime && World->GetTimeSeconds() - *LastCastTime < Cooldown;
}

bool USkillComponent::CastCircularSlash()
{
	UWorld* World = GetWorld();
	if (!World || !CircularSlashAreaClass)
	{
		UE_LOG(LogSkill, Error, TEXT("CircularSlashAreaClass is not configured."));
		return false;
	}

	const float Radius = GetCircularSlashRadius();
	const FTransform SpawnTransform(OwnerCharacter->GetActorRotation(), OwnerCharacter->GetActorLocation());
	APlayerSkill_CircleDamageArea* DamageArea = World->SpawnActorDeferred<APlayerSkill_CircleDamageArea>(CircularSlashAreaClass, SpawnTransform, OwnerCharacter, OwnerCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!DamageArea)
	{
		return false;
	}

	DamageArea->Initialize(Radius, CircularSlashDamage, CircularSlashLifeTime, OwnerCharacter);
	UGameplayStatics::FinishSpawningActor(DamageArea, SpawnTransform);
	UE_LOG(LogSkill, Display, TEXT("CircularSlash cast: Radius=%.0f."), Radius);
	return true;
}

bool USkillComponent::CastTripleProjectile()
{
	if (GetSkillForm(EPlayerSkillID::TripleProjectile) == EPlayerSkillForm::ThrownGrenade)
	{
		return CastThrownGrenade();
	}

	if (!ProjectileAttackAreaClass)
	{
		UE_LOG(LogSkill, Error, TEXT("ProjectileAttackAreaClass is not configured."));
		return false;
	}

	FVector Forward = OwnerCharacter->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		return false;
	}

	FVector Right = OwnerCharacter->GetActorRightVector();
	Right.Z = 0.0f;
	Right.Normalize();

	const int32 ProjectileCount = GetTripleProjectileCount();
	const float AngleStep = 12.0f;
	const float StartAngle = -AngleStep * (ProjectileCount - 1) * 0.5f;
	const FVector SpawnCenter = OwnerCharacter->GetActorLocation() + Forward * ProjectileSpawnForwardOffset;
	bool bSpawnedAny = false;
	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		const FVector Direction = Forward.RotateAngleAxis(StartAngle + AngleStep * Index, FVector::UpVector);
		const float SideOffset = (Index - (ProjectileCount - 1) * 0.5f) * ProjectileSpawnSideOffset;
		bSpawnedAny |= SpawnProjectile(SpawnCenter + Right * SideOffset, Direction, *FString::FromInt(Index + 1));
	}

	if (bSpawnedAny)
	{
		UE_LOG(LogSkill, Display, TEXT("TripleProjectile cast: ProjectileCount=%d."), ProjectileCount);
	}
	return bSpawnedAny;
}

bool USkillComponent::CastThrownGrenade()
{
	UWorld* World = GetWorld();
	if (!World || !OwnerCharacter || !ThrownGrenadeClass)
	{
		UE_LOG(LogSkill, Error, TEXT("ThrownGrenade cast failed: missing World, OwnerCharacter, or ThrownGrenadeClass."));
		return false;
	}

	FVector Direction = OwnerCharacter->GetActorForwardVector();
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		return false;
	}

	const FVector SpawnLocation = OwnerCharacter->GetActorLocation()
		+ Direction * ProjectileSpawnForwardOffset
		+ FVector(0.0f, 0.0f, 60.0f);
	const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);

	APlayerSkill_ThrownGrenade* Grenade = World->SpawnActorDeferred<APlayerSkill_ThrownGrenade>(
		ThrownGrenadeClass,
		SpawnTransform,
		OwnerCharacter,
		OwnerCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Grenade)
	{
		UE_LOG(LogSkill, Error, TEXT("ThrownGrenade cast failed: actor spawn returned null."));
		return false;
	}

	Grenade->Initialize(
		ThrownGrenadeFuseTime,
		ThrownGrenadeExplosionRadius,
		ThrownGrenadeDamage,
		ThrownGrenadeGravityZ,
		ThrownGrenadeCollisionRadius,
		Direction * ThrownGrenadeSpeed,
		OwnerCharacter);
	UGameplayStatics::FinishSpawningActor(Grenade, SpawnTransform);

	UE_LOG(LogSkill, Display,
		TEXT("TripleProjectile thrown grenade cast: Fuse=%.2f Radius=%.0f Damage=%.1f."),
		ThrownGrenadeFuseTime,
		ThrownGrenadeExplosionRadius,
		ThrownGrenadeDamage);
	return true;
}

bool USkillComponent::SpawnProjectile(const FVector& SpawnLocation, const FVector& Direction, const TCHAR* ProjectileLabel)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
	AAttackAreaBase* Projectile = World->SpawnActorDeferred<AAttackAreaBase>(ProjectileAttackAreaClass, SpawnTransform, OwnerCharacter, OwnerCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(LogSkill, Error, TEXT("Projectile failed to spawn: %s"), ProjectileLabel);
		return false;
	}

	Projectile->Initialize(TripleProjectileLifeTime, TripleProjectileSpeed);
	Projectile->bDetectObstacle = true;
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
	return true;
}

int32 USkillComponent::GetMaxUpgradeLevel(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType) const
{
	if (UpgradeType == ESkillUpgradeType::Mechanic)
	{
		return SkillID == EPlayerSkillID::TripleProjectile ? 2 : 3;
	}
	if (UpgradeType == ESkillUpgradeType::Cooldown)
	{
		return 4;
	}
	return 0;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Skill/HikariSkillComponent.h"

#include "Common/AttackAreaBase.h"
#include "Engine/World.h"
#include "HikariPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Skill/SkillCircleDamageArea.h"

DEFINE_LOG_CATEGORY(LogSkill);

UHikariSkillComponent::UHikariSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CircularSlashAreaClass = ASkillCircleDamageArea::StaticClass();
	ProjectileAttackAreaClass = AAttackAreaBase::StaticClass();
	InitializeSkillSlots();
}

void UHikariSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AHikariPlayerCharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogSkill, Error, TEXT("HikariSkillComponent requires an AHikariPlayerCharacter owner."));
	}
}

void UHikariSkillComponent::InitializeSkillSlots()
{
	SkillSlots.SetNum(3);
	SkillSlots[0].SkillID = EPlayerSkillID::TripleProjectile;
	SkillSlots[0].SkillLevel = 1;
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::TripleProjectile);
	UE_LOG(LogSkill, Log, TEXT("Skill slots initialized: Slot 1 = TripleProjectile."));
}

void UHikariSkillComponent::TryCastSkill1()
{
	TryCastSkillSlot(0);
}

void UHikariSkillComponent::TryCastSkill2()
{
	TryCastSkillSlot(1);
}

void UHikariSkillComponent::TryCastSkillSlot(int32 SlotIndex)
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
	}
}

bool UHikariSkillComponent::HasSkill(EPlayerSkillID SkillID) const
{
	return FindSkillSlot(SkillID) != INDEX_NONE;
}

bool UHikariSkillComponent::HasEmptySkillSlot() const
{
	return FindSkillSlot(EPlayerSkillID::None) != INDEX_NONE;
}

bool UHikariSkillComponent::AddSkillToFirstEmptySlot(EPlayerSkillID SkillID)
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
	return true;
}

int32 UHikariSkillComponent::FindSkillSlot(EPlayerSkillID SkillID) const
{
	return SkillSlots.IndexOfByPredicate([SkillID](const FPlayerSkillSlot& Slot)
	{
		return Slot.SkillID == SkillID;
	});
}

bool UHikariSkillComponent::CanApplyUpgrade(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType) const
{
	if (!HasSkill(SkillID) || UpgradeType == ESkillUpgradeType::None || UpgradeType == ESkillUpgradeType::Damage)
	{
		return false;
	}

	const FSkillUpgradeState State = GetSkillUpgradeState(SkillID);
	const int32 CurrentLevel = UpgradeType == ESkillUpgradeType::Mechanic ? State.MechanicLevel : State.CooldownLevel;
	return CurrentLevel < GetMaxUpgradeLevel(SkillID, UpgradeType);
}

void UHikariSkillComponent::ApplySkillUpgrade(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType)
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
}

FSkillUpgradeState UHikariSkillComponent::GetSkillUpgradeState(EPlayerSkillID SkillID) const
{
	if (const FSkillUpgradeState* State = SkillUpgradeStates.Find(SkillID))
	{
		return *State;
	}
	return FSkillUpgradeState();
}

int32 UHikariSkillComponent::GetTripleProjectileCount() const
{
	return FMath::Min(7, 3 + GetSkillUpgradeState(EPlayerSkillID::TripleProjectile).MechanicLevel * 2);
}

float UHikariSkillComponent::GetTripleProjectileCooldown() const
{
	return FMath::Max(2.0f, TripleProjectileCooldown - GetSkillUpgradeState(EPlayerSkillID::TripleProjectile).CooldownLevel * 0.5f);
}

float UHikariSkillComponent::GetCircularSlashRadius() const
{
	return FMath::Min(440.0f, CircularSlashRadius + GetSkillUpgradeState(EPlayerSkillID::CircularSlash).MechanicLevel * 60.0f);
}

float UHikariSkillComponent::GetCircularSlashCooldown() const
{
	return FMath::Max(1.6f, CircularSlashCooldown - GetSkillUpgradeState(EPlayerSkillID::CircularSlash).CooldownLevel * 0.4f);
}

bool UHikariSkillComponent::CanCastSkill() const
{
	return IsValid(OwnerCharacter)
		&& !OwnerCharacter->bIsDead
		&& OwnerCharacter->CanStartAction()
		&& !OwnerCharacter->IsSprinting();
}

bool UHikariSkillComponent::IsOnCooldown(EPlayerSkillID SkillID, float Cooldown) const
{
	const UWorld* World = GetWorld();
	const double* LastCastTime = LastCastTimes.Find(SkillID);
	return World && LastCastTime && World->GetTimeSeconds() - *LastCastTime < Cooldown;
}

bool UHikariSkillComponent::CastCircularSlash()
{
	UWorld* World = GetWorld();
	if (!World || !CircularSlashAreaClass)
	{
		UE_LOG(LogSkill, Error, TEXT("CircularSlashAreaClass is not configured."));
		return false;
	}

	const float Radius = GetCircularSlashRadius();
	const FTransform SpawnTransform(OwnerCharacter->GetActorRotation(), OwnerCharacter->GetActorLocation());
	ASkillCircleDamageArea* DamageArea = World->SpawnActorDeferred<ASkillCircleDamageArea>(CircularSlashAreaClass, SpawnTransform, OwnerCharacter, OwnerCharacter, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!DamageArea)
	{
		return false;
	}

	DamageArea->Initialize(Radius, CircularSlashDamage, CircularSlashLifeTime, OwnerCharacter);
	UGameplayStatics::FinishSpawningActor(DamageArea, SpawnTransform);
	UE_LOG(LogSkill, Display, TEXT("CircularSlash cast: Radius=%.0f."), Radius);
	return true;
}

bool UHikariSkillComponent::CastTripleProjectile()
{
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

bool UHikariSkillComponent::SpawnProjectile(const FVector& SpawnLocation, const FVector& Direction, const TCHAR* ProjectileLabel)
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

int32 UHikariSkillComponent::GetMaxUpgradeLevel(EPlayerSkillID SkillID, ESkillUpgradeType UpgradeType) const
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

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
	SkillSlots[0].SkillForm = EPlayerSkillForm::Default;
	SkillSlots[1].SkillID = EPlayerSkillID::CircularSlash;
	SkillSlots[1].SkillLevel = 1;
	SkillSlots[1].SkillForm = EPlayerSkillForm::Default;
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::TripleProjectile);
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::CircularSlash);
	UE_LOG(LogSkill, Log,
		TEXT("Skill slots initialized: Slot 0 (Q) = TripleProjectile [Default], Slot 1 (E) = CircularSlash."));
}

void USkillComponent::CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const
{
	OutRuntimeData.SkillSlots = SkillSlots;
	OutRuntimeData.SkillUpgradeStates = SkillUpgradeStates;

	UE_LOG(LogSkill, Log,
		TEXT("Skill runtime data captured: Owner=%s Slots=%d Upgrades=%d Modifiers(Q=%s E=%s)."),
		*GetNameSafe(GetOwner()),
		OutRuntimeData.SkillSlots.Num(),
		OutRuntimeData.SkillUpgradeStates.Num(),
		*BuildModifierSummary(EPlayerSkillID::TripleProjectile),
		*BuildModifierSummary(EPlayerSkillID::CircularSlash));
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
	// SkillForm is intentionally preserved for serialized compatibility. The
	// migration below mirrors legacy forms into modifiers, and the resolver
	// reads both representations while old snapshots are still in circulation.
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::TripleProjectile);
	SkillUpgradeStates.FindOrAdd(EPlayerSkillID::CircularSlash);
	MigrateLegacySkillForms();
	NormalizeSkillModifiers();
	LastCastTimes.Reset();

	UE_LOG(LogSkill, Log,
		TEXT("Skill runtime data applied: Owner=%s Slots=%d Upgrades=%d Modifiers(Q=%s E=%s)."),
		*GetNameSafe(GetOwner()),
		SkillSlots.Num(),
		SkillUpgradeStates.Num(),
		*BuildModifierSummary(EPlayerSkillID::TripleProjectile),
		*BuildModifierSummary(EPlayerSkillID::CircularSlash));
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

int32 USkillComponent::GetModifierStackForSlot(
	const FPlayerSkillSlot& Slot,
	ESkillModifierID ModifierID) const
{
	if (ModifierID == ESkillModifierID::None)
	{
		return 0;
	}

	int32 StackCount = 0;
	for (const FSkillModifierState& Modifier : Slot.Modifiers)
	{
		if (Modifier.ModifierID == ModifierID)
		{
			StackCount += FMath::Max(0, Modifier.StackCount);
		}
	}
	return StackCount;
}

int32 USkillComponent::GetModifierStack(EPlayerSkillID SkillID, ESkillModifierID ModifierID) const
{
	const int32 SlotIndex = FindSkillSlot(SkillID);
	return SkillSlots.IsValidIndex(SlotIndex)
		? GetModifierStackForSlot(SkillSlots[SlotIndex], ModifierID)
		: 0;
}

int32 USkillComponent::GetMaxModifierStack(
	EPlayerSkillID SkillID,
	ESkillModifierID ModifierID) const
{
	switch (SkillID)
	{
	case EPlayerSkillID::TripleProjectile:
		switch (ModifierID)
		{
		case ESkillModifierID::AddProjectile:
			return 3;
		case ESkillModifierID::InkGrenade:
		case ESkillModifierID::ExtraExplosion:
			return 1;
		case ESkillModifierID::CooldownDown:
			// Q starts at 4.0s and bottoms out at 2.0s in
			// GetTripleProjectileCooldown(), so four 0.5s steps are legal.
			return 4;
		default:
			return 0;
		}
	case EPlayerSkillID::CircularSlash:
		switch (ModifierID)
		{
		case ESkillModifierID::TwinSlash:
		case ESkillModifierID::NullRing:
			return 1;
		case ESkillModifierID::RadiusUp:
		case ESkillModifierID::CooldownDown:
			return 3;
		default:
			return 0;
		}
	default:
		return 0;
	}
}

bool USkillComponent::CanApplyModifier(
	EPlayerSkillID SkillID,
	ESkillModifierID ModifierID,
	int32 StackDelta) const
{
	if (StackDelta <= 0 || ModifierID == ESkillModifierID::None || !HasSkill(SkillID))
	{
		return false;
	}

	const int32 MaxStack = GetMaxModifierStack(SkillID, ModifierID);
	if (MaxStack <= 0)
	{
		return false;
	}

	if (ModifierID == ESkillModifierID::ExtraExplosion
		&& GetModifierStack(SkillID, ESkillModifierID::InkGrenade) <= 0)
	{
		return false;
	}

	return GetModifierStack(SkillID, ModifierID) + StackDelta <= MaxStack;
}

bool USkillComponent::ApplyModifier(
	EPlayerSkillID SkillID,
	ESkillModifierID ModifierID,
	int32 StackDelta)
{
	if (!CanApplyModifier(SkillID, ModifierID, StackDelta))
	{
		UE_LOG(LogSkill, Warning,
			TEXT("Modifier rejected: Skill=%s Modifier=%s StackDelta=%d Current=%d."),
			*UEnum::GetValueAsString(SkillID),
			*UEnum::GetValueAsString(ModifierID),
			StackDelta,
			GetModifierStack(SkillID, ModifierID));
		return false;
	}

	const int32 SlotIndex = FindSkillSlot(SkillID);
	if (!SkillSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FPlayerSkillSlot& Slot = SkillSlots[SlotIndex];
	FSkillModifierState* ExistingModifier = Slot.Modifiers.FindByPredicate(
		[ModifierID](const FSkillModifierState& Modifier)
		{
			return Modifier.ModifierID == ModifierID;
		});
	if (ExistingModifier)
	{
		ExistingModifier->StackCount += StackDelta;
	}
	else
	{
		FSkillModifierState NewModifier;
		NewModifier.ModifierID = ModifierID;
		NewModifier.StackCount = StackDelta;
		Slot.Modifiers.Add(NewModifier);
	}
	Slot.SkillLevel = FMath::Max(1, Slot.SkillLevel + StackDelta);

	UE_LOG(LogSkill, Log,
		TEXT("Modifier applied: Skill=%s Modifier=%s Stack=%d Build=%s."),
		*UEnum::GetValueAsString(SkillID),
		*UEnum::GetValueAsString(ModifierID),
		GetModifierStack(SkillID, ModifierID),
		*BuildModifierSummary(SkillID));
	OnSkillStateChanged.Broadcast();
	return true;
}

void USkillComponent::AddModifierIfMissing(
	FPlayerSkillSlot& Slot,
	ESkillModifierID ModifierID,
	int32 StackCount)
{
	if (ModifierID == ESkillModifierID::None || StackCount <= 0)
	{
		return;
	}

	FSkillModifierState* ExistingModifier = Slot.Modifiers.FindByPredicate(
		[ModifierID](const FSkillModifierState& Modifier)
		{
			return Modifier.ModifierID == ModifierID;
		});
	if (ExistingModifier)
	{
		ExistingModifier->StackCount = FMath::Max(ExistingModifier->StackCount, StackCount);
		return;
	}

	FSkillModifierState MigratedModifier;
	MigratedModifier.ModifierID = ModifierID;
	MigratedModifier.StackCount = StackCount;
	Slot.Modifiers.Add(MigratedModifier);
}

void USkillComponent::NormalizeSkillModifiers()
{
	for (FPlayerSkillSlot& Slot : SkillSlots)
	{
		TArray<FSkillModifierState> NormalizedModifiers;
		for (const FSkillModifierState& Modifier : Slot.Modifiers)
		{
			const int32 MaxStack = GetMaxModifierStack(Slot.SkillID, Modifier.ModifierID);
			if (MaxStack <= 0 || Modifier.StackCount <= 0)
			{
				continue;
			}

			FSkillModifierState* ExistingModifier = NormalizedModifiers.FindByPredicate(
				[&Modifier](const FSkillModifierState& Existing)
				{
					return Existing.ModifierID == Modifier.ModifierID;
				});
			if (ExistingModifier)
			{
				ExistingModifier->StackCount = FMath::Min(
					MaxStack,
					ExistingModifier->StackCount + Modifier.StackCount);
			}
			else
			{
				FSkillModifierState NormalizedModifier = Modifier;
				NormalizedModifier.StackCount = FMath::Min(MaxStack, Modifier.StackCount);
				NormalizedModifiers.Add(NormalizedModifier);
			}
		}

		if (GetModifierStackForSlot(Slot, ESkillModifierID::InkGrenade) <= 0)
		{
			NormalizedModifiers.RemoveAll(
				[](const FSkillModifierState& Modifier)
				{
					return Modifier.ModifierID == ESkillModifierID::ExtraExplosion;
				});
		}
		Slot.Modifiers = MoveTemp(NormalizedModifiers);
	}
}

void USkillComponent::MigrateLegacySkillForms()
{
	for (FPlayerSkillSlot& Slot : SkillSlots)
	{
		const EPlayerSkillForm LegacyForm = Slot.SkillForm;
		switch (Slot.SkillForm)
		{
		case EPlayerSkillForm::ThrownGrenade:
			if (Slot.SkillID == EPlayerSkillID::TripleProjectile)
			{
				AddModifierIfMissing(Slot, ESkillModifierID::InkGrenade, 1);
			}
			break;
		case EPlayerSkillForm::NullRing:
			if (Slot.SkillID == EPlayerSkillID::CircularSlash)
			{
				AddModifierIfMissing(Slot, ESkillModifierID::NullRing, 1);
			}
			break;
		case EPlayerSkillForm::TwinSlash:
			if (Slot.SkillID == EPlayerSkillID::CircularSlash)
			{
				AddModifierIfMissing(Slot, ESkillModifierID::TwinSlash, 1);
			}
			break;
		default:
			break;
		}

		if (LegacyForm != EPlayerSkillForm::Default)
		{
			UE_LOG(LogSkill, Log,
				TEXT("Legacy skill form migrated: Skill=%s Form=%s Build=%s."),
				*UEnum::GetValueAsString(Slot.SkillID),
				*UEnum::GetValueAsString(LegacyForm),
				*BuildModifierSummary(Slot.SkillID));
		}
	}
}

FString USkillComponent::BuildModifierSummary(EPlayerSkillID SkillID) const
{
	static const ESkillModifierID OrderedModifiers[] =
	{
		ESkillModifierID::AddProjectile,
		ESkillModifierID::InkGrenade,
		ESkillModifierID::ExtraExplosion,
		ESkillModifierID::TwinSlash,
		ESkillModifierID::NullRing,
		ESkillModifierID::RadiusUp,
		ESkillModifierID::CooldownDown
	};

	FString Summary;
	for (const ESkillModifierID ModifierID : OrderedModifiers)
	{
		const int32 StackCount = GetModifierStack(SkillID, ModifierID);
		if (StackCount <= 0)
		{
			continue;
		}

		if (!Summary.IsEmpty())
		{
			Summary += TEXT(", ");
		}
		Summary += FString::Printf(
			TEXT("%s x%d"),
			*UEnum::GetValueAsString(ModifierID),
			StackCount);
	}

	return Summary.IsEmpty() ? TEXT("None") : Summary;
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
	const int32 MechanicCount = GetSkillUpgradeState(EPlayerSkillID::TripleProjectile).MechanicLevel * 2;
	const int32 ModifierCount = GetModifierStack(EPlayerSkillID::TripleProjectile, ESkillModifierID::AddProjectile);
	return FMath::Min(7, 3 + MechanicCount + ModifierCount);
}

float USkillComponent::GetTripleProjectileCooldown() const
{
	const int32 UpgradeCount = GetSkillUpgradeState(EPlayerSkillID::TripleProjectile).CooldownLevel;
	const int32 ModifierCount = GetModifierStack(EPlayerSkillID::TripleProjectile, ESkillModifierID::CooldownDown);
	return FMath::Max(2.0f, TripleProjectileCooldown - (UpgradeCount + ModifierCount) * 0.5f);
}

float USkillComponent::GetCircularSlashRadius() const
{
	const int32 UpgradeCount = GetSkillUpgradeState(EPlayerSkillID::CircularSlash).MechanicLevel;
	const int32 ModifierCount = GetModifierStack(EPlayerSkillID::CircularSlash, ESkillModifierID::RadiusUp);
	return FMath::Min(440.0f, CircularSlashRadius + (UpgradeCount + ModifierCount) * 60.0f);
}

float USkillComponent::GetCircularSlashCooldown() const
{
	const int32 UpgradeCount = GetSkillUpgradeState(EPlayerSkillID::CircularSlash).CooldownLevel;
	const int32 ModifierCount = GetModifierStack(EPlayerSkillID::CircularSlash, ESkillModifierID::CooldownDown);
	return FMath::Max(1.6f, CircularSlashCooldown - (UpgradeCount + ModifierCount) * 0.4f);
}

EPlayerSkillForm USkillComponent::GetSkillForm(EPlayerSkillID SkillID) const
{
	const int32 SlotIndex = FindSkillSlot(SkillID);
	if (!SkillSlots.IsValidIndex(SlotIndex))
	{
		return EPlayerSkillForm::Default;
	}

	// Prefer the new modifier representation while retaining the legacy form
	// as a fallback for snapshots that have not gone through migration yet.
	if (SkillID == EPlayerSkillID::TripleProjectile
		&& GetModifierStack(SkillID, ESkillModifierID::InkGrenade) > 0)
	{
		return EPlayerSkillForm::ThrownGrenade;
	}
	if (SkillID == EPlayerSkillID::CircularSlash)
	{
		if (GetModifierStack(SkillID, ESkillModifierID::TwinSlash) > 0)
		{
			return EPlayerSkillForm::TwinSlash;
		}
		if (GetModifierStack(SkillID, ESkillModifierID::NullRing) > 0)
		{
			return EPlayerSkillForm::NullRing;
		}
	}

	return SkillSlots[SlotIndex].SkillForm;
}

FResolvedSkillSpec USkillComponent::ResolveSkillSpec(EPlayerSkillID SkillID) const
{
	FResolvedSkillSpec Spec;
	Spec.SkillID = SkillID;

	switch (SkillID)
	{
	case EPlayerSkillID::TripleProjectile:
	{
		const EPlayerSkillForm LegacyForm = GetSkillForm(SkillID);
		const bool bHasGrenadePayload = GetModifierStack(SkillID, ESkillModifierID::InkGrenade) > 0
			|| LegacyForm == EPlayerSkillForm::ThrownGrenade;
		Spec.PayloadType = bHasGrenadePayload
			? ESkillPayloadType::InkGrenade
			: ESkillPayloadType::NormalProjectile;
		Spec.ProjectileCount = GetTripleProjectileCount();
		Spec.ProjectileSpeed = bHasGrenadePayload ? ThrownGrenadeSpeed : TripleProjectileSpeed;
		Spec.ProjectileLifeTime = TripleProjectileLifeTime;
		Spec.FuseTime = ThrownGrenadeFuseTime;
		Spec.ExplosionRadius = ThrownGrenadeExplosionRadius;
		Spec.ExplosionDamage = ThrownGrenadeDamage;
		Spec.CollisionRadius = ThrownGrenadeCollisionRadius;
		Spec.ExplosionCount = bHasGrenadePayload
			? 1 + FMath::Min(1, GetModifierStack(SkillID, ESkillModifierID::ExtraExplosion))
			: 1;
		Spec.ExplosionDelay = ThrownGrenadeExplosionDelay;
		Spec.Cooldown = GetTripleProjectileCooldown();
		break;
	}
	case EPlayerSkillID::CircularSlash:
	{
		const EPlayerSkillForm LegacyForm = GetSkillForm(SkillID);
		const bool bHasTwinSlash = GetModifierStack(SkillID, ESkillModifierID::TwinSlash) > 0
			|| LegacyForm == EPlayerSkillForm::TwinSlash;
		Spec.HitCount = bHasTwinSlash ? 2 : 1;
		Spec.Radius = GetCircularSlashRadius();
		Spec.Damage = CircularSlashDamage;
		Spec.SecondHitDelay = TwinSlashDelay;
		Spec.SecondHitAngle = TwinSlashSecondYawOffset;
		Spec.SecondHitForwardOffset = TwinSlashSecondForwardOffset;
		Spec.SecondHitDamageMultiplier = TwinSlashSecondDamageMultiplier;
		Spec.bNullifyEnemyProjectiles = GetModifierStack(SkillID, ESkillModifierID::NullRing) > 0
			|| LegacyForm == EPlayerSkillForm::NullRing;
		Spec.Cooldown = GetCircularSlashCooldown();
		break;
	}
	default:
		UE_LOG(LogSkill, Warning, TEXT("Skill spec resolution rejected: Skill=%s."), *UEnum::GetValueAsString(SkillID));
		break;
	}

	UE_LOG(LogSkill, Log,
		TEXT("Skill spec resolved: Skill=%s Build=%s ProjectileCount=%d Payload=%s ExplosionCount=%d HitCount=%d Radius=%.0f Cooldown=%.2f."),
		*UEnum::GetValueAsString(SkillID),
		*BuildModifierSummary(SkillID),
		Spec.ProjectileCount,
		*UEnum::GetValueAsString(Spec.PayloadType),
		Spec.ExplosionCount,
		Spec.HitCount,
		Spec.Radius,
		Spec.Cooldown);
	return Spec;
}

FText USkillComponent::GetSkillBuildSummary(EPlayerSkillID SkillID) const
{
	if (!HasSkill(SkillID))
	{
		return FText::FromString(TEXT("Build: Empty"));
	}

	TArray<FString> Parts;
	if (SkillID == EPlayerSkillID::TripleProjectile)
	{
		const bool bHasGrenadePayload = GetModifierStack(SkillID, ESkillModifierID::InkGrenade) > 0
			|| GetSkillForm(SkillID) == EPlayerSkillForm::ThrownGrenade;
		Parts.Add(bHasGrenadePayload
			? TEXT("Payload: Ink Grenade")
			: TEXT("Payload: Normal Projectile"));
	}

	static const ESkillModifierID OrderedModifiers[] =
	{
		ESkillModifierID::AddProjectile,
		ESkillModifierID::InkGrenade,
		ESkillModifierID::ExtraExplosion,
		ESkillModifierID::TwinSlash,
		ESkillModifierID::NullRing,
		ESkillModifierID::RadiusUp,
		ESkillModifierID::CooldownDown
	};
	const UEnum* ModifierEnum = StaticEnum<ESkillModifierID>();
	for (const ESkillModifierID ModifierID : OrderedModifiers)
	{
		const int32 StackCount = GetModifierStack(SkillID, ModifierID);
		if (StackCount <= 0)
		{
			continue;
		}

		const FString ModifierName = ModifierEnum
			? ModifierEnum->GetDisplayNameTextByValue(static_cast<int64>(ModifierID)).ToString()
			: UEnum::GetValueAsString(ModifierID);
		Parts.Add(FString::Printf(TEXT("%s x%d"), *ModifierName, StackCount));
	}

	if (SkillID == EPlayerSkillID::CircularSlash && Parts.Num() == 0)
	{
		switch (GetSkillForm(SkillID))
		{
		case EPlayerSkillForm::TwinSlash:
			Parts.Add(TEXT("Form: Twin Slash"));
			break;
		case EPlayerSkillForm::NullRing:
			Parts.Add(TEXT("Form: Null Ring"));
			break;
		default:
			Parts.Add(TEXT("Base Circular Slash"));
			break;
		}
	}

	return FText::FromString(FString::Printf(TEXT("Build: %s"), *FString::Join(Parts, TEXT(" · "))));
}

FText USkillComponent::GetResolvedSkillSummary(EPlayerSkillID SkillID) const
{
	const FResolvedSkillSpec Spec = ResolveSkillSpec(SkillID);
	if (SkillID == EPlayerSkillID::TripleProjectile)
	{
		if (Spec.PayloadType == ESkillPayloadType::InkGrenade)
		{
			return FText::FromString(FString::Printf(
				TEXT("Effect: %d grenades · %d explosion(s) · CD %.1fs"),
				Spec.ProjectileCount,
				Spec.ExplosionCount,
				Spec.Cooldown));
		}

		return FText::FromString(FString::Printf(
			TEXT("Effect: %d projectiles · Life %.1fs · CD %.1fs"),
			Spec.ProjectileCount,
			Spec.ProjectileLifeTime,
			Spec.Cooldown));
	}
	if (SkillID == EPlayerSkillID::CircularSlash)
	{
		return FText::FromString(FString::Printf(
			TEXT("Effect: %d hit(s) · Radius %.0f · CD %.1fs%s"),
			Spec.HitCount,
			Spec.Radius,
			Spec.Cooldown,
			Spec.bNullifyEnemyProjectiles ? TEXT(" · Null Ring") : TEXT("")));
	}

	return FText::FromString(TEXT("Effect: Unresolved"));
}

bool USkillComponent::CanApplySkillForm(EPlayerSkillID SkillID, EPlayerSkillForm NewForm) const
{
	const int32 SlotIndex = FindSkillSlot(SkillID);
	if (!SkillSlots.IsValidIndex(SlotIndex) || GetSkillForm(SkillID) == NewForm)
	{
		return false;
	}

	switch (SkillID)
	{
	case EPlayerSkillID::TripleProjectile:
		return NewForm == EPlayerSkillForm::Default || NewForm == EPlayerSkillForm::ThrownGrenade;
	case EPlayerSkillID::CircularSlash:
		return NewForm == EPlayerSkillForm::Default
			|| NewForm == EPlayerSkillForm::NullRing
			|| NewForm == EPlayerSkillForm::TwinSlash;
	default:
		return false;
	}
}

void USkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TwinSlashTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

bool USkillComponent::ApplySkillForm(EPlayerSkillID SkillID, EPlayerSkillForm NewForm)
{
	if (!CanApplySkillForm(SkillID, NewForm))
	{
		return false;
	}

	const int32 SlotIndex = FindSkillSlot(SkillID);
	if (!SkillSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	// Keep old form callers source-compatible while immediately mirroring the
	// choice into the new persistent modifier representation.
	switch (NewForm)
	{
	case EPlayerSkillForm::ThrownGrenade:
		AddModifierIfMissing(SkillSlots[SlotIndex], ESkillModifierID::InkGrenade, 1);
		break;
	case EPlayerSkillForm::NullRing:
		AddModifierIfMissing(SkillSlots[SlotIndex], ESkillModifierID::NullRing, 1);
		break;
	case EPlayerSkillForm::TwinSlash:
		AddModifierIfMissing(SkillSlots[SlotIndex], ESkillModifierID::TwinSlash, 1);
		break;
	default:
		break;
	}
	SkillSlots[SlotIndex].SkillForm = NewForm;
	UE_LOG(LogSkill, Log, TEXT("Skill form applied: Skill=%s Form=%s."),
		*UEnum::GetValueAsString(SkillID),
		*UEnum::GetValueAsString(NewForm));
	OnSkillStateChanged.Broadcast();
	return true;
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

	const FResolvedSkillSpec Spec = ResolveSkillSpec(EPlayerSkillID::CircularSlash);
	const FTransform SpawnTransform(OwnerCharacter->GetActorRotation(), OwnerCharacter->GetActorLocation());
	if (!SpawnCircularSlash(SpawnTransform, Spec.Radius, Spec.Damage, Spec.bNullifyEnemyProjectiles))
	{
		return false;
	}

	if (Spec.HitCount > 1)
	{
		PendingTwinSlashOrigin = SpawnTransform.GetLocation();
		PendingTwinSlashRotation = SpawnTransform.Rotator();
		PendingTwinSlashSpec = Spec;
		World->GetTimerManager().ClearTimer(TwinSlashTimerHandle);
		World->GetTimerManager().SetTimer(
			TwinSlashTimerHandle,
			this,
			&USkillComponent::CastTwinSlashSecondHit,
			Spec.SecondHitDelay,
			false);
		UE_LOG(LogSkill, Log,
			TEXT("Twin Slash queued: Delay=%.2f Damage=%.1f Angle=%.1f Offset=%.0f."),
			Spec.SecondHitDelay,
			Spec.Damage * Spec.SecondHitDamageMultiplier,
			Spec.SecondHitAngle,
			Spec.SecondHitForwardOffset);
	}

	UE_LOG(LogSkill, Display, TEXT("CircularSlash cast: Radius=%.0f NullRing=%s HitCount=%d."),
		Spec.Radius,
		Spec.bNullifyEnemyProjectiles ? TEXT("true") : TEXT("false"),
		Spec.HitCount);
	return true;
}

bool USkillComponent::SpawnCircularSlash(
	const FTransform& SpawnTransform,
	float Radius,
	float Damage,
	bool bNullifyEnemyProjectiles)
{
	UWorld* World = GetWorld();
	if (!World || !CircularSlashAreaClass || !OwnerCharacter)
	{
		return false;
	}

	APlayerSkill_CircleDamageArea* DamageArea = World->SpawnActorDeferred<APlayerSkill_CircleDamageArea>(
		CircularSlashAreaClass,
		SpawnTransform,
		OwnerCharacter,
		OwnerCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!DamageArea)
	{
		return false;
	}

	DamageArea->Initialize(
		Radius,
		Damage,
		CircularSlashLifeTime,
		OwnerCharacter,
		bNullifyEnemyProjectiles);
	UGameplayStatics::FinishSpawningActor(DamageArea, SpawnTransform);
	return true;
}

void USkillComponent::CastTwinSlashSecondHit()
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(OwnerCharacter) || OwnerCharacter->IsDead())
	{
		return;
	}

	const FRotator SecondRotation = PendingTwinSlashRotation + FRotator(0.0f, PendingTwinSlashSpec.SecondHitAngle, 0.0f);
	FVector SecondDirection = SecondRotation.Vector();
	SecondDirection.Z = 0.0f;
	if (!SecondDirection.Normalize())
	{
		return;
	}

	const FVector SecondLocation = PendingTwinSlashOrigin + SecondDirection * PendingTwinSlashSpec.SecondHitForwardOffset;
	const FTransform SecondTransform(SecondRotation, SecondLocation);
	const float SecondDamage = PendingTwinSlashSpec.Damage * PendingTwinSlashSpec.SecondHitDamageMultiplier;
	if (SpawnCircularSlash(
		SecondTransform,
		PendingTwinSlashSpec.Radius,
		SecondDamage,
		PendingTwinSlashSpec.bNullifyEnemyProjectiles))
	{
		UE_LOG(LogSkill, Log,
			TEXT("Twin Slash second cast: Damage=%.1f Angle=%.1f Location=(%.0f, %.0f, %.0f)."),
			SecondDamage,
			PendingTwinSlashSpec.SecondHitAngle,
			SecondLocation.X,
			SecondLocation.Y,
			SecondLocation.Z);
	}
}

bool USkillComponent::CastTripleProjectile()
{
	const FResolvedSkillSpec Spec = ResolveSkillSpec(EPlayerSkillID::TripleProjectile);
	if (Spec.PayloadType == ESkillPayloadType::InkGrenade)
	{
		return CastThrownGrenade(Spec);
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

	const int32 ProjectileCount = Spec.ProjectileCount;
	const float AngleStep = ProjectileCount > 1
		? TripleProjectileSpreadAngle / (ProjectileCount - 1)
		: 0.0f;
	const float StartAngle = -AngleStep * (ProjectileCount - 1) * 0.5f;
	const FVector SpawnCenter = OwnerCharacter->GetActorLocation() + Forward * ProjectileSpawnForwardOffset;
	bool bSpawnedAny = false;
	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		const FVector Direction = Forward.RotateAngleAxis(StartAngle + AngleStep * Index, FVector::UpVector);
		const float SideOffset = (Index - (ProjectileCount - 1) * 0.5f) * ProjectileSpawnSideOffset;
		bSpawnedAny |= SpawnProjectile(
			SpawnCenter + Right * SideOffset,
			Direction,
			Spec.ProjectileLifeTime,
			Spec.ProjectileSpeed,
			*FString::FromInt(Index + 1));
	}

	if (bSpawnedAny)
	{
		UE_LOG(LogSkill, Display,
			TEXT("TripleProjectile cast: ProjectileCount=%d Payload=%s ExplosionCount=%d."),
			ProjectileCount,
			*UEnum::GetValueAsString(Spec.PayloadType),
			Spec.ExplosionCount);
	}
	return bSpawnedAny;
}

bool USkillComponent::CastThrownGrenade(const FResolvedSkillSpec& Spec)
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

	FVector Right = OwnerCharacter->GetActorRightVector();
	Right.Z = 0.0f;
	if (!Right.Normalize())
	{
		return false;
	}

	const int32 GrenadeCount = FMath::Max(1, Spec.ProjectileCount);
	const float AngleStep = GrenadeCount > 1
		? TripleProjectileSpreadAngle / (GrenadeCount - 1)
		: 0.0f;
	const float StartAngle = -AngleStep * (GrenadeCount - 1) * 0.5f;
	const FVector SpawnOrigin = OwnerCharacter->GetActorLocation();
	int32 SpawnedCount = 0;

	for (int32 Index = 0; Index < GrenadeCount; ++Index)
	{
		const FVector GrenadeDirection = Direction.RotateAngleAxis(
			StartAngle + AngleStep * Index,
			FVector::UpVector);
		const float SideOffset = (Index - (GrenadeCount - 1) * 0.5f) * ProjectileSpawnSideOffset;
		const FVector SpawnLocation = SpawnOrigin
			+ GrenadeDirection * ProjectileSpawnForwardOffset
			+ Right * SideOffset
			+ FVector(0.0f, 0.0f, 60.0f);
		const FTransform SpawnTransform(GrenadeDirection.Rotation(), SpawnLocation);

		APlayerSkill_ThrownGrenade* Grenade = World->SpawnActorDeferred<APlayerSkill_ThrownGrenade>(
			ThrownGrenadeClass,
			SpawnTransform,
			OwnerCharacter,
			OwnerCharacter,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Grenade)
		{
			UE_LOG(LogSkill, Warning,
				TEXT("ThrownGrenade cast skipped projectile %d/%d: actor spawn returned null."),
				Index + 1,
				GrenadeCount);
			continue;
		}

		Grenade->Initialize(
			Spec.FuseTime,
			Spec.ExplosionRadius,
			Spec.ExplosionDamage,
			ThrownGrenadeGravityZ,
			Spec.CollisionRadius,
			GrenadeDirection * Spec.ProjectileSpeed,
			OwnerCharacter,
			Spec.ExplosionCount,
			Spec.ExplosionDelay);
		UGameplayStatics::FinishSpawningActor(Grenade, SpawnTransform);
		++SpawnedCount;
	}

	UE_LOG(LogSkill, Display,
		TEXT("TripleProjectile grenade cast: Spawned=%d/%d Fuse=%.2f Radius=%.0f Damage=%.1f Explosions=%d."),
		SpawnedCount,
		GrenadeCount,
		Spec.FuseTime,
		Spec.ExplosionRadius,
		Spec.ExplosionDamage,
		Spec.ExplosionCount);
	return SpawnedCount > 0;
}

bool USkillComponent::SpawnProjectile(
	const FVector& SpawnLocation,
	const FVector& Direction,
	float ProjectileLifeTime,
	float ProjectileSpeed,
	const TCHAR* ProjectileLabel)
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

	Projectile->Initialize(ProjectileLifeTime, ProjectileSpeed);
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

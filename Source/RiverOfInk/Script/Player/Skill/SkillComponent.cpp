// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Skill/SkillComponent.h"

#include "Common/AttackAreaBase.h"
#include "Common/CombatEffectTags.h"
#include "Core/Audio/AudioManager.h"
#include "Engine/World.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Player/PlayerCharacter.h"
#include "Player/ProjectileTargetingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/Skill/PlayerSkill_CircleDamageArea.h"
#include "Player/Skill/PlayerSkill_ThrownGrenade.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogSkill);

USkillComponent::USkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CircularSlashAreaClass = APlayerSkill_CircleDamageArea::StaticClass();
	ProjectileAttackAreaClass = AAttackAreaBase::StaticClass();
	ThrownGrenadeClass = APlayerSkill_ThrownGrenade::StaticClass();

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TwoStageArcVFXAsset(
		TEXT("/Game/RawContent/VFX/NiagaraSystem/NS/PlayerTwoStageArc/NS/NS_TwoStageArc_Animated.NS_TwoStageArc_Animated"));
	if (TwoStageArcVFXAsset.Succeeded())
	{
		TwoStageArcVFX = TwoStageArcVFXAsset.Object;
	}
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TwoStageArcMirrorVFXAsset(
		TEXT("/Game/RawContent/VFX/NiagaraSystem/NS/PlayerTwoStageArc/NS/NS_TwoStageArc_Animated_Mirror.NS_TwoStageArc_Animated_Mirror"));
	if (TwoStageArcMirrorVFXAsset.Succeeded())
	{
		TwoStageArcMirrorVFX = TwoStageArcMirrorVFXAsset.Object;
	}
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
	bCircularSlashStage1Active = false;
	bCircularSlashStage2Ready = false;
	bCircularSlashStage2InputBuffered = false;

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
	TryCastSkillSlotInternal(SlotIndex, false);
}

bool USkillComponent::TryCastCircularSlashStage2Immediately()
{
	return TryCastSkillSlotInternal(1, true);
}

bool USkillComponent::TryCastSkillSlotInternal(int32 SlotIndex, bool bIgnoreActionState)
{
	if (!SkillSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	const EPlayerSkillID SkillID = SkillSlots[SlotIndex].SkillID;
	if (SkillID == EPlayerSkillID::None)
	{
		return false;
	}

	const bool bIsTwoStageCircularSlash = SkillID == EPlayerSkillID::CircularSlash
		&& GetSkillForm(SkillID) == EPlayerSkillForm::TwoStageArc;
	if (bIsTwoStageCircularSlash && bCircularSlashStage1Active)
	{
		UE_LOG(LogSkill, Display, TEXT("TwoStageArc stage 1 is still resolving; stage 2 remains locked."));
		return false;
	}

	if (bIsTwoStageCircularSlash && bCircularSlashStage2Ready)
	{
		if (!CanCastSkill(bIgnoreActionState))
		{
			return false;
		}

		if (CastCircularSlashStage2())
		{
			LastCastTimes.Add(SkillID, GetWorld()->GetTimeSeconds());
			OnSkillStateChanged.Broadcast();
			return true;
		}
		return false;
	}

	if (!CanCastSkill())
	{
		return false;
	}

	const float Cooldown = SkillID == EPlayerSkillID::TripleProjectile
		? GetTripleProjectileCooldown()
		: GetCircularSlashCooldown();
	if (IsOnCooldown(SkillID, Cooldown))
	{
		UE_LOG(LogSkill, Display, TEXT("%s is on cooldown."), *UEnum::GetValueAsString(SkillID));
		return false;
	}

	const bool bCastSucceeded = SkillID == EPlayerSkillID::TripleProjectile
		? CastTripleProjectile()
		: CastCircularSlash();
	if (bCastSucceeded)
	{
		// TwoStageArc starts its cooldown only after stage 1 misses or stage 2 is
		// released. The first cast itself must leave the slot in an actionable
		// stage-1/stage-2 state instead of starting the normal cooldown.
		if (!bIsTwoStageCircularSlash)
		{
			LastCastTimes.Add(SkillID, GetWorld()->GetTimeSeconds());
		}
		OnSkillStateChanged.Broadcast();
	}

	return bCastSucceeded;
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

bool USkillComponent::HasProjectileHoming(EPlayerSkillID SkillID) const
{
	return GetModifierStack(SkillID, ESkillModifierID::ProjectileHoming) > 0;
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
		case ESkillModifierID::ProjectileHoming:
			return 1;
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
		ESkillModifierID::CooldownDown,
		ESkillModifierID::ProjectileHoming
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

float USkillComponent::GetTwoStageArcStage2InputWindow() const
{
	return FMath::Clamp(TwoStageArcStage2InputWindow, 0.8f, 3.0f);
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
		// TwoStageArc is a true form and may coexist with additive modifiers such
		// as TwinSlash. Preserve the form identity before legacy modifier fallbacks.
		if (SkillSlots[SlotIndex].SkillForm == EPlayerSkillForm::TwoStageArc)
		{
			return EPlayerSkillForm::TwoStageArc;
		}
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

bool USkillComponent::IsCircularSlashStage1Active() const
{
	return bCircularSlashStage1Active;
}

bool USkillComponent::IsCircularSlashStage2Ready() const
{
	return bCircularSlashStage2Ready;
}

void USkillComponent::BufferCircularSlashStage2Input()
{
	if (GetSkillForm(EPlayerSkillID::CircularSlash) != EPlayerSkillForm::TwoStageArc
		|| (!bCircularSlashStage1Active && !bCircularSlashStage2Ready))
	{
		return;
	}

	if (!bCircularSlashStage2InputBuffered)
	{
		bCircularSlashStage2InputBuffered = true;
		UE_LOG(LogSkill, Display,
			TEXT("TwoStageArc stage 2 input buffered: Stage1Active=%s Stage2Ready=%s."),
			bCircularSlashStage1Active ? TEXT("true") : TEXT("false"),
			bCircularSlashStage2Ready ? TEXT("true") : TEXT("false"));
	}
}

bool USkillComponent::HasBufferedCircularSlashStage2Input() const
{
	return bCircularSlashStage2InputBuffered;
}

EPlayerSkillRuntimeState USkillComponent::GetSkillRuntimeState(EPlayerSkillID SkillID) const
{
	if (SkillID == EPlayerSkillID::CircularSlash
		&& GetSkillForm(SkillID) == EPlayerSkillForm::TwoStageArc)
	{
		if (bCircularSlashStage1Active)
		{
			return EPlayerSkillRuntimeState::Stage1Active;
		}

		if (bCircularSlashStage2Ready)
		{
			return EPlayerSkillRuntimeState::Stage2Ready;
		}
	}

	return IsOnCooldown(SkillID, GetSkillCooldown(SkillID))
		? EPlayerSkillRuntimeState::Cooldown
		: EPlayerSkillRuntimeState::Ready;
}

bool USkillComponent::CanTriggerCircularSlashInput() const
{
	if (GetSkillForm(EPlayerSkillID::CircularSlash) == EPlayerSkillForm::TwoStageArc)
	{
		// Do not enter UPlayerState_Skill2 for a second time while stage 1 is
		// still resolving. That would replay the skill animation even though
		// TryCastSkillSlot correctly rejects the cast.
		if (bCircularSlashStage1Active)
		{
			return false;
		}

		// A confirmed stage-1 hit unlocks stage 2 immediately, without waiting
		// for the normal E cooldown gate.
		if (bCircularSlashStage2Ready)
		{
			return true;
		}
	}

	return !IsOnCooldown(EPlayerSkillID::CircularSlash, GetCircularSlashCooldown());
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
		Spec.bEnableHoming = HasProjectileHoming(SkillID);
		Spec.GuidanceMode = Spec.bEnableHoming
			? (bHasGrenadePayload
				? EProjectileGuidanceMode::TargetedArcLanding
				: EProjectileGuidanceMode::SoftProjectileHoming)
			: EProjectileGuidanceMode::None;
		Spec.HomingTurnRate = bHasGrenadePayload
			? ThrownGrenadeGuidanceTurnRate
			: ProjectileHomingTurnRate;
		Spec.HomingStartDelay = bHasGrenadePayload
			? ThrownGrenadeGuidanceStartDelay
			: ProjectileHomingStartDelay;
		Spec.HomingMaxDistance = bHasGrenadePayload
			? ThrownGrenadeGuidanceMaxDistance
			: ProjectileHomingMaxDistance;
		Spec.HomingAcceptanceRadius = bHasGrenadePayload
			? ThrownGrenadeGuidanceAcceptanceRadius
			: ProjectileHomingAcceptanceRadius;
		Spec.GuidanceTargetSpread = bHasGrenadePayload
			? FMath::Max(0.0f, ThrownGrenadeGuidanceSpread)
			: 0.0f;
		Spec.Cooldown = GetTripleProjectileCooldown();
		break;
	}
	case EPlayerSkillID::CircularSlash:
	{
		const EPlayerSkillForm LegacyForm = GetSkillForm(SkillID);
		const bool bHasTwinSlash = GetModifierStack(SkillID, ESkillModifierID::TwinSlash) > 0
			|| LegacyForm == EPlayerSkillForm::TwinSlash;
		const bool bTwoStageArc = LegacyForm == EPlayerSkillForm::TwoStageArc;
		Spec.StageCount = bTwoStageArc ? 2 : 1;
		Spec.JudgmentsPerStage = bHasTwinSlash ? 2 : 1;
		Spec.bHasTwinSlash = bHasTwinSlash;
		Spec.TwinSlashDamageMultiplier = bHasTwinSlash
			? FMath::Max(0.0f, TwinSlashSecondDamageMultiplier)
			: 1.0f;
		Spec.HitCount = Spec.StageCount * Spec.JudgmentsPerStage;
		Spec.Radius = GetCircularSlashRadius();
		Spec.Damage = CircularSlashDamage;
		Spec.StageDamageMultiplier = bTwoStageArc
			? FMath::Max(0.0f, TwoStageArcStageDamageMultiplier)
			: 1.0f;
		Spec.Stage2InputWindow = bTwoStageArc
			? GetTwoStageArcStage2InputWindow()
			: 0.0f;
		Spec.bUseArcHitbox = bTwoStageArc;
		Spec.ArcHalfAngle = bTwoStageArc
			? FMath::Clamp(TwoStageArcHalfAngle, 0.0f, 180.0f)
			: 180.0f;
		if (bTwoStageArc)
		{
			Spec.Radius = FMath::Max(1.0f, TwoStageArcRadius);
		}
		Spec.SecondHitDelay = TwinSlashDelay;
		Spec.SecondHitAngle = TwinSlashSecondYawOffset;
		Spec.SecondHitForwardOffset = TwinSlashSecondForwardOffset;
		// Keep the legacy field populated for serialized/editor callers. Runtime
		// damage resolution uses the explicit per-judgment field above.
		Spec.SecondHitDamageMultiplier = Spec.TwinSlashDamageMultiplier;
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
		TEXT("Skill spec resolved: Skill=%s Build=%s ProjectileCount=%d Payload=%s ExplosionCount=%d HitCount=%d Stages=%d JudgmentsPerStage=%d TwinSlash=%s TwinSlashMultiplier=%.2f StageDamageMultiplier=%.2f Stage2Window=%.2f Arc=%s HalfAngle=%.1f Radius=%.0f Cooldown=%.2f."),
		*UEnum::GetValueAsString(SkillID),
		*BuildModifierSummary(SkillID),
		Spec.ProjectileCount,
		*UEnum::GetValueAsString(Spec.PayloadType),
		Spec.ExplosionCount,
		Spec.HitCount,
		Spec.StageCount,
		Spec.JudgmentsPerStage,
		Spec.bHasTwinSlash ? TEXT("true") : TEXT("false"),
		Spec.TwinSlashDamageMultiplier,
		Spec.StageDamageMultiplier,
		Spec.Stage2InputWindow,
		Spec.bUseArcHitbox ? TEXT("true") : TEXT("false"),
		Spec.ArcHalfAngle,
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
		ESkillModifierID::CooldownDown,
		ESkillModifierID::ProjectileHoming
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
		case EPlayerSkillForm::TwoStageArc:
			Parts.Add(TEXT("Form: Two Stage Arc"));
			break;
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
			TEXT("Effect: %d stage(s) · %d judgment(s)/stage · Radius %.0f · CD %.1fs%s"),
			Spec.StageCount,
			Spec.JudgmentsPerStage,
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
			|| NewForm == EPlayerSkillForm::TwinSlash
			|| NewForm == EPlayerSkillForm::TwoStageArc;
	default:
		return false;
	}
}

void USkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CircularSlashStage1ResolutionTimerHandle);
	}
	bCircularSlashStage2InputBuffered = false;

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

	if (SkillID == EPlayerSkillID::CircularSlash)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CircularSlashStage1ResolutionTimerHandle);
		}
		bCircularSlashStage1Active = false;
		bCircularSlashStage2Ready = false;
		bCircularSlashStage2InputBuffered = false;
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

bool USkillComponent::CanCastSkill(bool bIgnoreActionState) const
{
	return IsValid(OwnerCharacter)
		&& !OwnerCharacter->IsDead()
		&& (bIgnoreActionState || OwnerCharacter->CanStartAction())
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
	if (!ECastSoundName.IsEmpty())
	{
		FAudioManager::Play(ECastSoundName, true);
	}

	UWorld* World = GetWorld();
	if (!World || !CircularSlashAreaClass)
	{
		UE_LOG(LogSkill, Error, TEXT("CircularSlashAreaClass is not configured."));
		return false;
	}

	const FResolvedSkillSpec Spec = ResolveSkillSpec(EPlayerSkillID::CircularSlash);
	const bool bIsTwoStage = Spec.StageCount > 1;
	if (bIsTwoStage)
	{
		bCircularSlashStage1Active = true;
		bCircularSlashStage2Ready = false;
		bCircularSlashStage2InputBuffered = false;
		LastCastTimes.Remove(EPlayerSkillID::CircularSlash);
		World->GetTimerManager().ClearTimer(CircularSlashStage1ResolutionTimerHandle);
	}

	if (!SpawnCircularSlashSet(Spec, 0, bIsTwoStage))
	{
		if (bIsTwoStage)
		{
			bCircularSlashStage1Active = false;
			bCircularSlashStage2Ready = false;
		}
		return false;
	}

	if (bIsTwoStage && bCircularSlashStage1Active)
	{
		World->GetTimerManager().SetTimer(
			CircularSlashStage1ResolutionTimerHandle,
			this,
			&USkillComponent::ResolveCircularSlashStage1Miss,
			CircularSlashLifeTime,
			false);
		UE_LOG(LogSkill, Log,
			TEXT("TwoStageArc stage 1 active: Hit any enemy within %.2fs to unlock stage 2."),
			CircularSlashLifeTime);
	}

	UE_LOG(LogSkill, Display, TEXT("CircularSlash cast: Radius=%.0f NullRing=%s Stages=%d JudgmentsPerStage=%d Arc=%s."),
		Spec.Radius,
		Spec.bNullifyEnemyProjectiles ? TEXT("true") : TEXT("false"),
		Spec.StageCount,
		Spec.JudgmentsPerStage,
		Spec.bUseArcHitbox ? TEXT("true") : TEXT("false"));
	return true;
}

bool USkillComponent::CastCircularSlashStage2()
{
	if (!bCircularSlashStage2Ready)
	{
		return false;
	}

	if (!ECastSoundName.IsEmpty())
	{
		FAudioManager::Play(ECastSoundName, true);
	}

	const FResolvedSkillSpec Spec = ResolveSkillSpec(EPlayerSkillID::CircularSlash);
	if (Spec.StageCount < 2 || !SpawnCircularSlashSet(Spec, 1, false))
	{
		UE_LOG(LogSkill, Warning, TEXT("TwoStageArc stage 2 spawn failed; stage 2 remains available."));
		return false;
	}

	bCircularSlashStage2Ready = false;
	bCircularSlashStage1Active = false;
	bCircularSlashStage2InputBuffered = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CircularSlashStage1ResolutionTimerHandle);
	}
	UE_LOG(LogSkill, Display,
		TEXT("TwoStageArc stage 2 released: DamageMultiplier=%.2f JudgmentsPerStage=%d."),
		Spec.StageDamageMultiplier,
		Spec.JudgmentsPerStage);
	return true;
}

bool USkillComponent::SpawnCircularSlashSet(
	const FResolvedSkillSpec& Spec,
	int32 StageIndex,
	bool bListenForStage1Hit)
{
	if (!OwnerCharacter || StageIndex < 0 || StageIndex >= Spec.StageCount)
	{
		return false;
	}

	const FVector Origin = OwnerCharacter->GetActorLocation();
	const FRotator BaseRotation = OwnerCharacter->GetActorRotation();
	const float TwinSlashMultiplier = Spec.bHasTwinSlash
		? Spec.TwinSlashDamageMultiplier
		: 1.0f;
	const float JudgmentDamage = Spec.Damage * Spec.StageDamageMultiplier * TwinSlashMultiplier;

	for (int32 JudgmentIndex = 0; JudgmentIndex < Spec.JudgmentsPerStage; ++JudgmentIndex)
	{
		FRotator JudgmentRotation = BaseRotation;
		FVector JudgmentLocation = Origin;
		if (JudgmentIndex > 0)
		{
			JudgmentRotation += FRotator(0.0f, Spec.SecondHitAngle, 0.0f);
			FVector JudgmentDirection = JudgmentRotation.Vector();
			JudgmentDirection.Z = 0.0f;
			if (!JudgmentDirection.Normalize())
			{
				return false;
			}
			JudgmentLocation += JudgmentDirection * Spec.SecondHitForwardOffset;
		}

		const FTransform JudgmentTransform(JudgmentRotation, JudgmentLocation);
		const FTransform VisualTransform =
			Spec.bUseArcHitbox && Spec.bHasTwinSlash
			? FTransform(BaseRotation, Origin)
			: JudgmentTransform;
		if (!SpawnCircularSlash(
			JudgmentTransform,
			VisualTransform,
			Spec.Radius,
			JudgmentDamage,
			Spec.bNullifyEnemyProjectiles,
			bListenForStage1Hit,
			Spec.bUseArcHitbox,
			Spec.ArcHalfAngle,
			StageIndex,
			JudgmentIndex,
			Spec.bHasTwinSlash))
		{
			return false;
		}
	}

	UE_LOG(LogSkill, Log,
		TEXT("CircularSlash stage spawned: Stage=%d/%d Judgments=%d DamagePerJudgment=%.1f StageTotal=%.1f TwinSlash=%s Arc=%s."),
		StageIndex + 1,
		Spec.StageCount,
		Spec.JudgmentsPerStage,
		JudgmentDamage,
		JudgmentDamage * static_cast<float>(Spec.JudgmentsPerStage),
		Spec.bHasTwinSlash ? TEXT("true") : TEXT("false"),
		Spec.bUseArcHitbox ? TEXT("true") : TEXT("false"));
	return true;
}

bool USkillComponent::SpawnCircularSlash(
	const FTransform& SpawnTransform,
	const FTransform& VFXSpawnTransform,
	float Radius,
	float Damage,
	bool bNullifyEnemyProjectiles,
	bool bListenForStage1Hit,
	bool bUseArcHitbox,
	float ArcHalfAngle,
	int32 StageIndex,
	int32 JudgmentIndex,
	bool bHasTwinSlash)
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
		bNullifyEnemyProjectiles,
		bUseArcHitbox,
		ArcHalfAngle);
	// The normal E keeps the original BP circular slash VFX. TwoStageArc owns
	// its presentation and therefore hides only the legacy component on this
	// arc damage area before the deferred actor begins play.
	DamageArea->SetUseLegacyCircularSlashVFX(!bUseArcHitbox);
	if (bListenForStage1Hit)
	{
		DamageArea->OnHitConfirmed.AddUObject(this, &USkillComponent::HandleCircularSlashStage1Hit);
	}
	if (bUseArcHitbox)
	{
		UE_LOG(LogSkill, Verbose,
			TEXT("CircularSlash arc hitbox configured for %s: HalfAngle=%.1f Radius=%.0f."),
			*GetNameSafe(DamageArea),
			ArcHalfAngle,
			Radius);
	}
	UGameplayStatics::FinishSpawningActor(DamageArea, SpawnTransform);
	if (bUseArcHitbox)
	{
		SpawnTwoStageArcVFX(VFXSpawnTransform, StageIndex, JudgmentIndex, bHasTwinSlash);
	}
	return true;
}

void USkillComponent::SpawnTwoStageArcVFX(
	const FTransform& SpawnTransform,
	int32 StageIndex,
	int32 JudgmentIndex,
	bool bHasTwinSlash)
{
	UWorld* World = GetWorld();
	const bool bMirrorVFX = bHasTwinSlash ? JudgmentIndex > 0 : StageIndex > 0;
	UNiagaraSystem* SelectedVFX = bMirrorVFX && TwoStageArcMirrorVFX
		? TwoStageArcMirrorVFX
		: TwoStageArcVFX;
	if (!World || !SelectedVFX || !OwnerCharacter)
	{
		UE_LOG(LogSkill, Warning,
			TEXT("TwoStageArc VFX skipped: World=%s Asset=%s Mirror=%s Owner=%s."),
			World ? TEXT("valid") : TEXT("invalid"),
			*GetNameSafe(SelectedVFX),
			bMirrorVFX ? TEXT("true") : TEXT("false"),
			*GetNameSafe(OwnerCharacter));
		return;
	}

	FRotator SpawnRotation = SpawnTransform.Rotator();
	const float SweepYaw = (bMirrorVFX ? -1.0f : 1.0f)
		* FMath::Clamp(TwoStageArcVFXYawOffset, -180.0f, 180.0f);
	SpawnRotation.Yaw += SweepYaw;
	const float GroundAngle = FMath::Clamp(TwoStageArcVFXGroundAngle, -90.0f, 90.0f);
	SpawnRotation.Pitch += GroundAngle;

	FVector Forward = OwnerCharacter->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	FVector Right = OwnerCharacter->GetActorRightVector();
	Right.Z = 0.0f;
	if (!Right.Normalize())
	{
		Right = FVector::RightVector;
	}

	const FVector SpawnLocation = SpawnTransform.GetLocation()
		+ Forward * TwoStageArcVFXForwardOffset
		+ Right * TwoStageArcVFXRightOffset
		+ FVector::UpVector * TwoStageArcVFXHeightOffset;
	const float Scale = FMath::Max(0.01f, TwoStageArcVFXScale);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		SelectedVFX,
		SpawnLocation,
		SpawnRotation,
		FVector(Scale));

	UE_LOG(LogSkill, Log,
		TEXT("TwoStageArc VFX spawned: Asset=%s Stage=%d Judgment=%d Twin=%s Mirror=%s Scale=%.2f Yaw=%.1f Pitch=%.1f Offset=(Forward=%.1f Right=%.1f Height=%.1f)."),
		*GetNameSafe(SelectedVFX),
		StageIndex + 1,
		JudgmentIndex + 1,
		bHasTwinSlash ? TEXT("true") : TEXT("false"),
		bMirrorVFX ? TEXT("true") : TEXT("false"),
		Scale,
		SweepYaw,
		GroundAngle,
		TwoStageArcVFXForwardOffset,
		TwoStageArcVFXRightOffset,
		TwoStageArcVFXHeightOffset);
}

void USkillComponent::HandleCircularSlashStage1Hit(AActor* HitActor)
{
	if (!bCircularSlashStage1Active)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CircularSlashStage1ResolutionTimerHandle);
	}
	bCircularSlashStage1Active = false;
	bCircularSlashStage2Ready = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CircularSlashStage1ResolutionTimerHandle,
			this,
			&USkillComponent::ResolveCircularSlashStage2Timeout,
			GetTwoStageArcStage2InputWindow(),
			false);
	}
	UE_LOG(LogSkill, Display,
		TEXT("TwoStageArc stage 1 hit %s; stage 2 unlocked for %.2fs."),
		*GetNameSafe(HitActor),
		GetTwoStageArcStage2InputWindow());
	OnSkillStateChanged.Broadcast();

	if (bCircularSlashStage2InputBuffered && OwnerCharacter)
	{
		OwnerCharacter->TryConsumeBufferedCircularSlashStage2Input();
	}
}


void USkillComponent::ResolveCircularSlashStage1Miss()
{
	if (!bCircularSlashStage1Active)
	{
		return;
	}

	bCircularSlashStage1Active = false;
	bCircularSlashStage2Ready = false;
	bCircularSlashStage2InputBuffered = false;
	if (UWorld* World = GetWorld())
	{
		LastCastTimes.Add(EPlayerSkillID::CircularSlash, World->GetTimeSeconds());
	}
	UE_LOG(LogSkill, Display, TEXT("TwoStageArc stage 1 missed; E cooldown started without stage 2."));
	OnSkillStateChanged.Broadcast();
}

void USkillComponent::ResolveCircularSlashStage2Timeout()
{
	if (!bCircularSlashStage2Ready)
	{
		return;
	}

	bCircularSlashStage2Ready = false;
	bCircularSlashStage2InputBuffered = false;
	if (UWorld* World = GetWorld())
	{
		LastCastTimes.Add(EPlayerSkillID::CircularSlash, World->GetTimeSeconds());
	}
	UE_LOG(LogSkill, Display,
		TEXT("TwoStageArc stage 2 input window expired; E cooldown started."));
	OnSkillStateChanged.Broadcast();
}

bool USkillComponent::CastTripleProjectile()
{
	if (!QCastSoundName.IsEmpty())
	{
		FAudioManager::Play(QCastSoundName, true);
	}

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
	AEnemyBase* HomingTarget = nullptr;
	FCombatEffectHandle HomingMarkHandle;
	if (Spec.bEnableHoming && OwnerCharacter->GetProjectileTargetingComponent())
	{
		OwnerCharacter->GetProjectileTargetingComponent()->GetCurrentMarkedTargetSnapshot(
			HomingTarget,
			HomingMarkHandle);
	}

	FProjectileSpec ProjectileSpec;
	ProjectileSpec.LifeTime = Spec.ProjectileLifeTime;
	ProjectileSpec.ProjectileSpeed = Spec.ProjectileSpeed;
	ProjectileSpec.GuidanceMode = Spec.GuidanceMode;
	ProjectileSpec.HomingTurnRate = Spec.HomingTurnRate;
	ProjectileSpec.HomingStartDelay = Spec.HomingStartDelay;
	ProjectileSpec.HomingMaxDistance = Spec.HomingMaxDistance;
	ProjectileSpec.HomingAcceptanceRadius = Spec.HomingAcceptanceRadius;
	ProjectileSpec.HomingTarget = HomingTarget;
	ProjectileSpec.HomingMarkHandle = HomingMarkHandle;
	ProjectileSpec.bEnableHoming = Spec.bEnableHoming
		&& Spec.GuidanceMode != EProjectileGuidanceMode::None
		&& IsValid(HomingTarget)
		&& HomingMarkHandle.IsValid();
	if (ProjectileSpec.bEnableHoming)
	{
		ProjectileSpec.ProjectileTags.AddTag(RiverOfInkCombatEffectTags::Build_Projectile_Homing);
	}

	bool bSpawnedAny = false;
	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		const FVector Direction = Forward.RotateAngleAxis(StartAngle + AngleStep * Index, FVector::UpVector);
		const float SideOffset = (Index - (ProjectileCount - 1) * 0.5f) * ProjectileSpawnSideOffset;
		bSpawnedAny |= SpawnProjectile(
			SpawnCenter + Right * SideOffset,
			Direction,
			ProjectileSpec,
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
	AEnemyBase* HomingTarget = nullptr;
	FCombatEffectHandle HomingMarkHandle;
	if (Spec.bEnableHoming && OwnerCharacter->GetProjectileTargetingComponent())
	{
		OwnerCharacter->GetProjectileTargetingComponent()->GetCurrentMarkedTargetSnapshot(
			HomingTarget,
			HomingMarkHandle);
	}
	FVector TargetRight = Right;
	if (IsValid(HomingTarget))
	{
		FVector ToTarget = HomingTarget->GetActorLocation() - SpawnOrigin;
		ToTarget.Z = 0.0f;
		if (ToTarget.Normalize())
		{
			TargetRight = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal();
		}
	}
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
		const float TargetOffsetAmount = (Index - (GrenadeCount - 1) * 0.5f) * Spec.GuidanceTargetSpread;
		const FVector GuidanceTargetOffset = Spec.GuidanceMode == EProjectileGuidanceMode::TargetedArcLanding
			? TargetRight * TargetOffsetAmount
			: FVector::ZeroVector;
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
			Spec.ExplosionDelay,
			HomingTarget,
			Spec.HomingTurnRate,
			HomingMarkHandle,
			Spec.HomingStartDelay,
			Spec.HomingMaxDistance,
			Spec.HomingAcceptanceRadius,
			Spec.GuidanceMode,
			GuidanceTargetOffset,
			ThrownGrenadeNiagaraSystem);
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
	const FProjectileSpec& ProjectileSpec,
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

	Projectile->InitializeProjectile(ProjectileSpec);
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

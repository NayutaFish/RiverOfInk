// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/ProjectileTargetingComponent.h"

#include "Common/CombatEffectComponent.h"
#include "Common/CombatEffectTags.h"
#include "Common/CombatEffectTypes.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/PlayerCharacter.h"
#include "Player/Skill/PlayerSkillTypes.h"
#include "Player/Skill/SkillComponent.h"

UProjectileTargetingComponent::UProjectileTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UProjectileTargetingComponent::HasHomingBuild() const
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	const USkillComponent* Skills = Player ? Player->SkillComponent : nullptr;
	if (!Skills)
	{
		const UCombatEffectComponent* Effects = Player ? Player->GetCombatEffectComponent() : nullptr;
		return Effects
			&& Effects->HasEffect(RiverOfInkCombatEffectTags::Build_Projectile_Homing);
	}

	const bool bHasSkillModifier = Skills->GetModifierStack(
		EPlayerSkillID::TripleProjectile,
		ESkillModifierID::ProjectileHoming) > 0;
	const UCombatEffectComponent* Effects = Player->GetCombatEffectComponent();
	return bHasSkillModifier
		|| (Effects && Effects->HasEffect(RiverOfInkCombatEffectTags::Build_Projectile_Homing));
}

bool UProjectileTargetingComponent::IsHomingMarkActive(const AEnemyBase* Target) const
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player
		|| !IsValid(Target)
		|| Target->bIsDead
		|| CurrentMarkedTarget.Get() != Target)
	{
		return false;
	}

	const UCombatEffectComponent* Effects = Target->GetCombatEffectComponent();
	if (!Effects)
	{
		return false;
	}

	FActiveCombatEffect Mark;
	if (!Effects->TryGetEffectFromSource(
		RiverOfInkCombatEffectTags::Effect_Debuff_HomingMark,
		const_cast<APlayerCharacter*>(Player),
		Mark))
	{
		return false;
	}

	if (CurrentMarkedEffectHandle.IsValid() && Mark.Handle != CurrentMarkedEffectHandle)
	{
		return false;
	}

	return Mark.HasFiniteDuration()
		&& Mark.RemainingTime > KINDA_SMALL_NUMBER;
}

AEnemyBase* UProjectileTargetingComponent::FindBestHomingTarget() const
{
	return GetCurrentMarkedTarget();
}

FCombatEffectHandle UProjectileTargetingComponent::ApplyOrTransferHomingMark(
	AEnemyBase* NewTarget,
	float Duration)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !IsValid(NewTarget) || NewTarget->bIsDead || Duration <= 0.0f)
	{
		return FCombatEffectHandle();
	}

	if (CurrentMarkedTarget.Get() != NewTarget)
	{
		ClearCurrentMarkedTarget();
	}

	UCombatEffectComponent* Effects = NewTarget->GetCombatEffectComponent();
	if (!Effects)
	{
		CurrentMarkedTarget = nullptr;
		CurrentMarkedEffectHandle = FCombatEffectHandle();
		return FCombatEffectHandle();
	}

	FCombatEffectSpec HomingMarkSpec;
	HomingMarkSpec.EffectTag = RiverOfInkCombatEffectTags::Effect_Debuff_HomingMark;
	HomingMarkSpec.Category = ECombatEffectCategory::Debuff;
	HomingMarkSpec.DurationPolicy = ECombatEffectDurationPolicy::Timed;
	HomingMarkSpec.StackPolicy = ECombatEffectStackPolicy::RefreshDuration;
	HomingMarkSpec.Duration = FMath::Max(0.01f, Duration);
	HomingMarkSpec.StackCount = 1;
	HomingMarkSpec.MaxStacks = 1;
	HomingMarkSpec.SourceActor = Player;
	HomingMarkSpec.AffectsTags.AddTag(RiverOfInkCombatEffectTags::Build_Projectile_Homing);

	const FCombatEffectHandle MarkHandle = Effects->ApplyEffect(HomingMarkSpec);
	if (!MarkHandle.IsValid())
	{
		CurrentMarkedTarget = nullptr;
		CurrentMarkedEffectHandle = FCombatEffectHandle();
		return MarkHandle;
	}

	CurrentMarkedTarget = NewTarget;
	CurrentMarkedEffectHandle = MarkHandle;
	UE_LOG(LogTemp, Log,
		TEXT("Projectile homing mark applied: Player=%s Target=%s Duration=%.2f Handle=%d."),
		*GetNameSafe(Player),
		*GetNameSafe(NewTarget),
		HomingMarkSpec.Duration,
		MarkHandle.Id);
	return MarkHandle;
}

AEnemyBase* UProjectileTargetingComponent::GetCurrentMarkedTarget() const
{
	AEnemyBase* Target = CurrentMarkedTarget.Get();
	return IsHomingMarkActive(Target) ? Target : nullptr;
}

bool UProjectileTargetingComponent::GetCurrentMarkedTargetSnapshot(
	AEnemyBase*& OutTarget,
	FCombatEffectHandle& OutMarkHandle) const
{
	OutTarget = GetCurrentMarkedTarget();
	OutMarkHandle = CurrentMarkedEffectHandle;
	return IsValid(OutTarget) && OutMarkHandle.IsValid();
}

bool UProjectileTargetingComponent::IsHomingMarkActive(
	const AEnemyBase* Target,
	const FCombatEffectHandle& ExpectedMarkHandle) const
{
	return ExpectedMarkHandle.IsValid()
		&& CurrentMarkedEffectHandle == ExpectedMarkHandle
		&& IsHomingMarkActive(Target);
}

bool UProjectileTargetingComponent::IsCurrentMarkedTargetValid() const
{
	return GetCurrentMarkedTarget() != nullptr;
}

void UProjectileTargetingComponent::ClearCurrentMarkedTarget()
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	AEnemyBase* Target = CurrentMarkedTarget.Get();
	if (Player && IsValid(Target))
	{
		if (UCombatEffectComponent* Effects = Target->GetCombatEffectComponent())
		{
			if (CurrentMarkedEffectHandle.IsValid())
			{
				Effects->RemoveEffect(CurrentMarkedEffectHandle);
			}
			else
			{
				FActiveCombatEffect Mark;
				if (Effects->TryGetEffectFromSource(
					RiverOfInkCombatEffectTags::Effect_Debuff_HomingMark,
					Player,
					Mark))
				{
					Effects->RemoveEffect(Mark.Handle);
				}
			}
		}
	}

	CurrentMarkedTarget = nullptr;
	CurrentMarkedEffectHandle = FCombatEffectHandle();
}

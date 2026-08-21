// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/ProjectileTargetingComponent.h"

#include "Common/CombatEffectComponent.h"
#include "Common/CombatEffectTags.h"
#include "Common/CombatEffectTypes.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "EngineUtils.h"
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
	if (!Player || !IsValid(Target) || Target->bIsDead)
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

	return (!Mark.HasFiniteDuration() || Mark.RemainingTime > KINDA_SMALL_NUMBER)
		&& (!Mark.HasCharges() || Mark.RemainingCharges > 0);
}

AEnemyBase* UProjectileTargetingComponent::FindBestHomingTarget() const
{
	UWorld* World = GetWorld();
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!World || !Player)
	{
		return nullptr;
	}

	const FVector Origin = Player->GetActorLocation();
	const float MaxDistanceSquared = HomingSearchRadius > 0.0f
		? FMath::Square(HomingSearchRadius)
		: TNumericLimits<float>::Max();

	AEnemyBase* BestTarget = nullptr;
	float BestDistanceSquared = MaxDistanceSquared;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* Candidate = *It;
		if (!IsHomingMarkActive(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool UProjectileTargetingComponent::ConsumeHomingMark(AEnemyBase* Target)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player || !IsHomingMarkActive(Target))
	{
		return false;
	}

	UCombatEffectComponent* Effects = Target->GetCombatEffectComponent();
	const bool bConsumed = Effects
		&& Effects->ConsumeEffectChargeFromSource(
			RiverOfInkCombatEffectTags::Effect_Debuff_HomingMark,
			Player,
			1);
	if (bConsumed)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("Projectile homing mark charge consumed: Player=%s Enemy=%s."),
			*GetNameSafe(Player),
			*GetNameSafe(Target));
	}
	return bConsumed;
}

void UProjectileTargetingComponent::NotifyProjectileHit(AActor* Target)
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(Target))
	{
		ConsumeHomingMark(Enemy);
	}
}

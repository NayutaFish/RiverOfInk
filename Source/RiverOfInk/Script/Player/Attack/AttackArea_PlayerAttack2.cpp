// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Attack/AttackArea_PlayerAttack2.h"
#include "Common/CombatEffectComponent.h"
#include "Common/CombatEffectTags.h"
#include "Core/EventBus.h"
#include "Core/GameEvents.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Player/Attack/SpecialBuff_PlayerAttack2.h"
#include "RiverOfInk.h"

AAttackArea_PlayerAttack2::AAttackArea_PlayerAttack2()
{
	// Attack2 is a moving projectile. Its hitbox is the inherited sphere, not a player-centered fan.
	bUseFanHitbox = false;
	bIsMeleeAttack = false;
	bFollowTargetRotation = false;
	bDetectObstacle = true;
	LifeTime = 1.5f;
	Speed = 900.0f;
	Radius = 50.0f;
	bDrawDebugHitbox = false;
}

void AAttackArea_PlayerAttack2::ApplyDamage_Implementation(AActor* Target)
{
	// 先记录目标是否为敌人（伤害结算可能直接击杀敌人，之后无法再 Cast）
	AEnemyBase* Enemy = Cast<AEnemyBase>(Target);

	Super::ApplyDamage_Implementation(Target);

	// 命中敌人时通报特攻命中事件（供音效/特效/计数器等订阅）
	if (Enemy)
	{
		FEventBus::Publish<FPlayerSpecialAttackHitEvent>(FPlayerSpecialAttackHitEvent(Enemy));

		// 若敌人已被本次伤害击杀（已销毁），不生成 Buff，避免幽灵 Buff 留在原地
		if (!IsValid(Enemy))
		{
			return;
		}

		if (UCombatEffectComponent* EnemyEffects = Enemy->GetCombatEffectComponent())
		{
			FCombatEffectSpec HomingMarkSpec;
			HomingMarkSpec.EffectTag = RiverOfInkCombatEffectTags::Effect_Debuff_HomingMark;
			HomingMarkSpec.Category = ECombatEffectCategory::Debuff;
			HomingMarkSpec.DurationPolicy = ECombatEffectDurationPolicy::TimedAndCharges;
			HomingMarkSpec.StackPolicy = ECombatEffectStackPolicy::AddStackAndRefresh;
			HomingMarkSpec.Duration = FMath::Max(0.01f, HomingMarkDuration);
			HomingMarkSpec.Charges = FMath::Max(1, HomingMarkCharges);
			HomingMarkSpec.MaxCharges = HomingMarkSpec.Charges;
			HomingMarkSpec.StackCount = 1;
			HomingMarkSpec.MaxStacks = 1;
			HomingMarkSpec.SourceActor = GetOwner();
			HomingMarkSpec.AffectsTags.AddTag(RiverOfInkCombatEffectTags::Build_Projectile_Homing);
			const FCombatEffectHandle MarkHandle = EnemyEffects->ApplyEffect(HomingMarkSpec);
			UE_LOG(LogRiverOfInk, Log,
				TEXT("Attack2 applied HomingMark: Enemy=%s Duration=%.2f Charges=%d Handle=%d."),
				*Enemy->GetName(),
				HomingMarkSpec.Duration,
				HomingMarkSpec.Charges,
				MarkHandle.Id);

			FTakeDamageInfo BonusInfo = NextHitBonusDamageInfo;
			// Preserve existing Blueprint tuning while assets migrate to the
			// direct payload exposed on this attack area.
			if (BonusInfo.DamageValue <= KINDA_SMALL_NUMBER && SpecialBuffClass)
			{
				if (const ASpecialBuff_PlayerAttack2* LegacyBuff =
					SpecialBuffClass->GetDefaultObject<ASpecialBuff_PlayerAttack2>())
				{
					BonusInfo = LegacyBuff->BonusDamageInfo;
				}
			}

			if (BonusInfo.DamageValue > KINDA_SMALL_NUMBER)
			{
				FCombatEffectSpec NextHitSpec;
				NextHitSpec.EffectTag = RiverOfInkCombatEffectTags::Effect_Proc_NextHitBonusDamage;
				NextHitSpec.Category = ECombatEffectCategory::Proc;
				NextHitSpec.DurationPolicy = ECombatEffectDurationPolicy::TimedAndCharges;
				NextHitSpec.StackPolicy = ECombatEffectStackPolicy::AddStackAndRefresh;
				NextHitSpec.Duration = FMath::Max(0.01f, NextHitBonusDuration);
				NextHitSpec.Charges = FMath::Max(1, NextHitBonusCharges);
				NextHitSpec.StackCount = 1;
				NextHitSpec.MaxStacks = FMath::Max(1, NextHitBonusMaxStacks);
				NextHitSpec.SourceActor = GetOwner();
				NextHitSpec.Magnitude = BonusInfo.DamageValue;
				NextHitSpec.DamagePayload.DamageValue = BonusInfo.DamageValue;
				NextHitSpec.DamagePayload.HardDamageValue = BonusInfo.HardDamageValue;
				NextHitSpec.DamagePayload.DamageType = BonusInfo.DamageType;
				NextHitSpec.DamagePayload.bCanCauseDeath = BonusInfo.bCanCauseDeath;
				NextHitSpec.DamagePayload.bIsDirectDamage = BonusInfo.bIsDirectDamage;
				NextHitSpec.DamagePayload.bIgnoreInvulnerability = BonusInfo.bIgnoreInvincible;

				const FCombatEffectHandle Handle = EnemyEffects->ApplyEffect(NextHitSpec);
				UE_LOG(LogRiverOfInk, Log,
					TEXT("Attack2 applied NextHitBonusDamage: Enemy=%s Damage=%.1f Duration=%.2f Charges=%d Handle=%d."),
					*Enemy->GetName(),
					BonusInfo.DamageValue,
					NextHitSpec.Duration,
					NextHitSpec.Charges,
					Handle.Id);
			}
		}
	}
}

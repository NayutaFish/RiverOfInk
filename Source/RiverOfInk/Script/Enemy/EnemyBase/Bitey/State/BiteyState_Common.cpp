// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/Bitey/State/BiteyState_Common.h"

#include "Common/AttackAreaBase.h"
#include "Common/AttackArea/AttackAreaBase_Orbit.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"
#include "TimerManager.h"

namespace
{
	/** 攻击方式数量：召唤法球 / 冲撞后发射。 */
	constexpr int32 BiteyAttackModeCount = 2;

	/** 阿咬的攻击方式（与 CurrentAttackMode 的取值一一对应）。 */
	enum class EBiteyAttackMode : int32
	{
		SummonOrbs = 0,
		ChargeAndLaunch = 1
	};

	const TCHAR* GetBiteyModeName(int32 Mode)
	{
		switch (Mode)
		{
		case static_cast<int32>(EBiteyAttackMode::ChargeAndLaunch): return TEXT("ChargeAndLaunch");
		case static_cast<int32>(EBiteyAttackMode::SummonOrbs):
		default: return TEXT("SummonOrbs");
		}
	}
}

UBiteyState_Common::UBiteyState_Common()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBiteyState_Common::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy)
	{
		return;
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey %s attack state entered: NextMode=%d(%s)."),
		*Enemy->GetName(),
		CurrentAttackMode,
		GetBiteyModeName(CurrentAttackMode));

	// 冲撞由本状态自己驱动：敌人如果同时开了 bUseChargeAttack，Chase 会抢先进 EnemyState_Charge，
	// 那样就绕过了“冲撞后发射法球”的收尾，这里提前告警。
	if (Enemy->bUseChargeAttack)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Bitey %s has bUseChargeAttack enabled: Chase may enter EnemyState_Charge and skip this state's orb launch. Set it to false."),
			*Enemy->GetName());
	}
}

void UBiteyState_Common::OnExit_Implementation()
{
	// 冲撞/发射/回追的排程全部清理：被打断（硬值击破、死亡、切状态）时不再补动作。
	// 法球是独立 Actor，不在这里销毁——它们要留在场上等下一次冲撞发射。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeEndTimerHandle);
		World->GetTimerManager().ClearTimer(OrbLaunchTimerHandle);
		World->GetTimerManager().ClearTimer(ReturnTimerHandle);
	}

	bCharging = false;
	ClearChargeAttackArea();

	Super::OnExit_Implementation();
}

void UBiteyState_Common::Update_Implementation(float DeltaTime)
{
	Super::Update_Implementation(DeltaTime);

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || !bCharging)
	{
		return;
	}

	// 冲撞位移：与近战灯笼怪的冲撞一致，撞到障碍/目标就提前收束
	FHitResult Hit;
	Enemy->AddActorWorldOffset(
		ChargeDirection * Enemy->GetEffectiveMoveSpeed(Enemy->ChargeSpeed) * DeltaTime,
		true,
		&Hit);

	if (Hit.bBlockingHit)
	{
		FinishCharge(*Enemy, TEXT("Blocked"));
	}
}

void UBiteyState_Common::ExecuteAttack()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead)
	{
		return;
	}

	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	// 每次进入只执行一种攻击方式，并推进到下一种：两种方式一轮循环
	ActiveAttackMode = CurrentAttackMode % BiteyAttackModeCount;
	CurrentAttackMode = (ActiveAttackMode + 1) % BiteyAttackModeCount;

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey %s attack mode %d(%s) started."),
		*Enemy->GetName(),
		ActiveAttackMode,
		GetBiteyModeName(ActiveAttackMode));

	if (ActiveAttackMode == static_cast<int32>(EBiteyAttackMode::ChargeAndLaunch))
	{
		EnterChargeMode(*Enemy);
	}
	else
	{
		EnterSummonMode(*Enemy);
	}
}

// ──────────────────────────────
// 模式 0：召唤法球
// ──────────────────────────────

void UBiteyState_Common::EnterSummonMode(AEnemyBase& Enemy)
{
	if (!OrbAttackAreaClass)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Bitey %s summon mode skipped: OrbAttackAreaClass is missing (assign an AAttackAreaBase_Orbit subclass on this state)."),
			*Enemy.GetName());
		ScheduleReturnToChase(Enemy);
		return;
	}

	SummonOrbs(Enemy);
	ScheduleReturnToChase(Enemy);
}

void UBiteyState_Common::SummonOrbs(AEnemyBase& Enemy)
{
	// 先清掉上一轮残留（例如上一轮冲撞被打断、法球还没发射出去）：
	// 保证每次召唤都是“左右各一”的固定造型，而不是越攒越多。
	ClearOrbs();

	const int32 Count = FMath::Max(1, OrbCount);
	const float StepDegrees = 360.0f / static_cast<float>(Count);
	const float EnemyYaw = Enemy.GetActorRotation().Yaw;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		SpawnOrb(Enemy, EnemyYaw + OrbFirstAngleOffset + StepDegrees * static_cast<float>(Index));
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey %s summoned %d orb(s): Radius=%.0f AngularSpeed=%.1f LifeTime=%.2fs."),
		*Enemy.GetName(),
		Count,
		OrbOrbitRadius,
		OrbOrbitSpeed,
		OrbLifeTime);
}

void UBiteyState_Common::SpawnOrb(AEnemyBase& Enemy, float StartAngleDegrees)
{
	UWorld* World = GetWorld();
	if (!World || !OrbAttackAreaClass)
	{
		return;
	}

	const float AngleRadians = FMath::DegreesToRadians(StartAngleDegrees);
	const FVector Radial(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
	const FVector SpawnLocation = Enemy.GetActorLocation()
		+ Radial * FMath::Max(0.0f, OrbOrbitRadius)
		+ FVector(0.0f, 0.0f, OrbHeightOffset);
	const FTransform SpawnTransform(FRotator(0.0f, StartAngleDegrees, 0.0f), SpawnLocation);

	AAttackAreaBase_Orbit* Orb = World->SpawnActorDeferred<AAttackAreaBase_Orbit>(
		OrbAttackAreaClass,
		SpawnTransform,
		&Enemy,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Orb)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("Bitey %s orb spawn failed."), *Enemy.GetName());
		return;
	}

	// 伤害类型与过滤：Owner 是敌人，bDamageOpponentOnly 会只打玩家
	Orb->AttackDamageProfile.AttackType = EAttackType::EnemyRanged;
	Orb->bDamageOpponentOnly = true;
	Orb->bDetectObstacle = Enemy.bAttackAreaDetectObstacle;

	UGameplayStatics::FinishSpawningActor(Orb, SpawnTransform);

	// FinishSpawning 之后再开始绕行：BeginPlay 会重置基类的寿命计时
	Orb->InitializeOrbit(
		&Enemy,
		OrbOrbitRadius,
		OrbOrbitSpeed,
		StartAngleDegrees,
		OrbHeightOffset,
		OrbLifeTime);

	Orbs.Add(Orb);
}

void UBiteyState_Common::ClearOrbs()
{
	for (const TWeakObjectPtr<AAttackAreaBase_Orbit>& Orb : Orbs)
	{
		AAttackAreaBase_Orbit* ValidOrb = Orb.Get();
		if (IsValid(ValidOrb))
		{
			ValidOrb->Destroy();
		}
	}

	Orbs.Reset();
}

void UBiteyState_Common::GatherOrbitingOrbs(TArray<AAttackAreaBase_Orbit*>& OutOrbs)
{
	OutOrbs.Reset();

	for (int32 Index = Orbs.Num() - 1; Index >= 0; --Index)
	{
		AAttackAreaBase_Orbit* Orb = Orbs[Index].Get();

		// 已经被销毁（寿命/净墨环/撞墙）或已经发射过的法球从列表剔除，避免重复发射
		if (!IsValid(Orb) || !Orb->IsOrbiting())
		{
			Orbs.RemoveAtSwap(Index);
			continue;
		}

		OutOrbs.Add(Orb);
	}
}

// ──────────────────────────────
// 模式 1：冲撞 → 延迟发射法球
// ──────────────────────────────

void UBiteyState_Common::EnterChargeMode(AEnemyBase& Enemy)
{
	// 冲撞方向在冲撞开始时锁定（与近战灯笼怪一致），冲撞期间不再追着玩家转向
	FVector ToTarget = Enemy.GetCombatTarget()->GetActorLocation() - Enemy.GetActorLocation();
	ToTarget.Z = 0.0f;
	ChargeDirection = ToTarget.IsNearlyZero()
		? Enemy.GetActorForwardVector().GetSafeNormal2D()
		: ToTarget.GetSafeNormal();
	if (ChargeDirection.IsNearlyZero())
	{
		ChargeDirection = FVector::ForwardVector;
	}

	Enemy.SetActorRotation(ChargeDirection.Rotation());

	// 冲撞前摇用的是基类攻击状态的 AttackWindupTime：ExecuteAttack 已经在前摇结束后才被调用
	BeginCharge(Enemy);
}

void UBiteyState_Common::BeginCharge(AEnemyBase& Enemy)
{
	bCharging = true;

	const TSubclassOf<AAttackAreaBase> ChargeClass = ResolveChargeClass(Enemy);
	if (ChargeClass)
	{
		const FVector SpawnLocation = Enemy.GetActorLocation() + ChargeDirection * Enemy.AttackAreaSpawnOffset;
		const FTransform SpawnTransform(ChargeDirection.Rotation(), SpawnLocation);

		if (UWorld* World = GetWorld())
		{
			ChargeAttackArea = World->SpawnActorDeferred<AAttackAreaBase>(
				ChargeClass,
				SpawnTransform,
				&Enemy,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			if (ChargeAttackArea)
			{
				ChargeAttackArea->AttackDamageProfile.AttackType = EAttackType::EnemyCharge;
				// 近战 + 跟随施放者：冲撞期间一直贴在敌人身上，每个目标只结算一次
				ChargeAttackArea->Initialize(
					FMath::Max(0.01f, Enemy.ChargeDuration),
					0.0f,
					true,
					&Enemy);
				ChargeAttackArea->bDamageOpponentOnly = true;
				ChargeAttackArea->bDetectObstacle = false;
				ChargeAttackArea->bIsEnemyProjectile = false;
				UGameplayStatics::FinishSpawningActor(ChargeAttackArea, SpawnTransform);
			}
		}
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Bitey %s charge has no attack area class (set ChargeAttackAreaClass on this state or AttackAreaClass on the enemy); the dash will deal no damage."),
			*Enemy.GetName());
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey %s charge started: Speed=%.1f Duration=%.2fs Direction=(%.2f,%.2f) Hitbox=%s."),
		*Enemy.GetName(),
		Enemy.GetEffectiveMoveSpeed(Enemy.ChargeSpeed),
		Enemy.ChargeDuration,
		ChargeDirection.X,
		ChargeDirection.Y,
		*GetNameSafe(ChargeAttackArea.Get()));

	if (UWorld* World = GetWorld())
	{
		// rate <= 0 会被 SetTimer 直接丢弃，所以冲撞时长要抬到极小正数
		World->GetTimerManager().SetTimer(
			ChargeEndTimerHandle,
			this,
			&UBiteyState_Common::FinishChargeByDuration,
			FMath::Max(Enemy.ChargeDuration, KINDA_SMALL_NUMBER),
			false);
	}
}

void UBiteyState_Common::FinishCharge(AEnemyBase& Enemy, const TCHAR* EndReason)
{
	if (!bCharging)
	{
		return;
	}

	bCharging = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeEndTimerHandle);
	}

	ClearChargeAttackArea();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey %s charge ended: Reason=%s; launching orbs in %.2fs."),
		*Enemy.GetName(),
		EndReason ? EndReason : TEXT("Unknown"),
		OrbLaunchDelay);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			OrbLaunchTimerHandle,
			this,
			&UBiteyState_Common::LaunchOrbsAndRecover,
			FMath::Max(OrbLaunchDelay, KINDA_SMALL_NUMBER),
			false);
	}
}

void UBiteyState_Common::FinishChargeByDuration()
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		FinishCharge(*Enemy, TEXT("Duration"));
	}
}

void UBiteyState_Common::LaunchOrbsAndRecover()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead || !IsStillCurrentState())
	{
		return;
	}

	LaunchOrbs(*Enemy);
	ScheduleReturnToChase(*Enemy);
}

void UBiteyState_Common::LaunchOrbs(AEnemyBase& Enemy)
{
	TArray<AAttackAreaBase_Orbit*> OrbitingOrbs;
	GatherOrbitingOrbs(OrbitingOrbs);

	// “如果法球还在的话”：法球已经被寿命/净墨环/撞墙清掉时就只保留冲撞，不发射
	if (OrbitingOrbs.Num() == 0)
	{
		UE_LOG(LogRiverOfInk, Log,
			TEXT("Bitey %s has no orbiting orb left; charge only, no launch."),
			*Enemy.GetName());
		return;
	}

	// 朝目标当前位置发射（与其它敌人射弹一致：只取水平方向，飞行高度保持不变）
	FVector AimPoint = Enemy.GetActorLocation();
	if (const APlayerCharacter* Target = Enemy.GetCombatTarget())
	{
		AimPoint = Target->GetActorLocation();
	}

	int32 LaunchedCount = 0;
	for (AAttackAreaBase_Orbit* Orb : OrbitingOrbs)
	{
		FVector Direction = AimPoint - Orb->GetActorLocation();
		Direction.Z = 0.0f;
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		Orb->LaunchAsProjectile(Direction, OrbLaunchSpeed, OrbFlightLifeTime);
		++LaunchedCount;
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey %s launched %d/%d orb(s): Speed=%.1f FlightLifeTime=%.2fs."),
		*Enemy.GetName(),
		LaunchedCount,
		OrbitingOrbs.Num(),
		OrbLaunchSpeed,
		OrbFlightLifeTime);
}

// ──────────────────────────────
// 收尾与工具
// ──────────────────────────────

void UBiteyState_Common::ScheduleReturnToChase(AEnemyBase& Enemy)
{
	if (UWorld* World = GetWorld())
	{
		// 同上：后摇为 0 时也要真的创建计时器，否则永远回不到 Chase
		World->GetTimerManager().SetTimer(
			ReturnTimerHandle,
			this,
			&UBiteyState_Common::ReturnToChase,
			FMath::Max(Enemy.AttackRecoveryTime, KINDA_SMALL_NUMBER),
			false);
	}
}

void UBiteyState_Common::ClearChargeAttackArea()
{
	if (IsValid(ChargeAttackArea.Get()))
	{
		ChargeAttackArea->Destroy();
	}

	ChargeAttackArea = nullptr;
}

TSubclassOf<AAttackAreaBase> UBiteyState_Common::ResolveChargeClass(const AEnemyBase& Enemy) const
{
	return ChargeAttackAreaClass ? ChargeAttackAreaClass : Enemy.AttackAreaClass;
}

bool UBiteyState_Common::IsStillCurrentState() const
{
	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	const UStateBase* Current = Enemy ? Enemy->CurrentState.Get() : nullptr;
	return Current == this;
}

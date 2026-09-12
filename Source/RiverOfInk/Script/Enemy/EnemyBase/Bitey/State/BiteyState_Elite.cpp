// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/EnemyBase/Bitey/State/BiteyState_Elite.h"

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
	/** 攻击方式数量：四个斜向法球 / 三段冲撞后发射 / 横向排开后冲撞回收。 */
	constexpr int32 BiteyEliteAttackModeCount = 3;

	/** 精英阿咬的攻击方式（与 CurrentAttackMode 的取值一一对应）。 */
	enum class EBiteyEliteAttackMode : int32
	{
		OrbitFour = 0,
		TripleCharge = 1,
		LineUpCharge = 2
	};

	const TCHAR* GetBiteyEliteModeName(int32 Mode)
	{
		switch (Mode)
		{
		case static_cast<int32>(EBiteyEliteAttackMode::TripleCharge): return TEXT("TripleCharge");
		case static_cast<int32>(EBiteyEliteAttackMode::LineUpCharge): return TEXT("LineUpCharge");
		case static_cast<int32>(EBiteyEliteAttackMode::OrbitFour):
		default: return TEXT("OrbitFour");
		}
	}
}

UBiteyState_Elite::UBiteyState_Elite()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBiteyState_Elite::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy)
	{
		return;
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey elite %s attack state entered: NextMode=%d(%s)."),
		*Enemy->GetName(),
		CurrentAttackMode,
		GetBiteyEliteModeName(CurrentAttackMode));

	// 冲撞由本状态自己驱动：敌人如果同时开了 bUseChargeAttack，Chase 会抢先进 EnemyState_Charge，
	// 那样就绕过了本状态的收尾（发射 / 回收），这里提前告警。
	if (Enemy->bUseChargeAttack)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Bitey elite %s has bUseChargeAttack enabled: Chase may enter EnemyState_Charge and skip this state's launch/recall. Set it to false."),
			*Enemy->GetName());
	}
}

void UBiteyState_Elite::OnExit_Implementation()
{
	// 冲撞/发射/回收/回追的排程全部清理：被打断（硬值击破、死亡、切状态）时不再补动作。
	// 法球是独立 Actor，不在这里销毁——绕行中的法球要留着等下一次发射。
	if (UWorld* World = GetWorld())
	{
		// 退出时还挂着发射/回收排程 = 这次攻击被打断了，否则会静悄悄地什么都不发生
		if (World->GetTimerManager().IsTimerActive(OrbLaunchTimerHandle))
		{
			UE_LOG(LogRiverOfInk, Warning,
				TEXT("Bitey elite %s attack interrupted before the orb launch/recall fired (state switched: hard break / death / target lost)."),
				*GetNameSafe(GetOwner()));
		}

		World->GetTimerManager().ClearTimer(ChargeEndTimerHandle);
		World->GetTimerManager().ClearTimer(ChargeRepeatTimerHandle);
		World->GetTimerManager().ClearTimer(OrbLaunchTimerHandle);
		World->GetTimerManager().ClearTimer(ReturnTimerHandle);
	}

	bCharging = false;
	ClearChargeAttackArea();

	Super::OnExit_Implementation();
}

void UBiteyState_Elite::Update_Implementation(float DeltaTime)
{
	// 基类攻击状态每帧都会把朝向锁回“进入攻击状态时”记录的 LockedRotation，
	// 所以必须在 Super 之后接管朝向：冲撞期间对准该段冲撞方向，其余时间持续转向目标。
	// 只在冲撞期间接管的话，冲撞一结束朝向就会被锁回旧方向（看起来像“打完又转回去了”）。
	Super::Update_Implementation(DeltaTime);

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead)
	{
		return;
	}

	FVector Facing = FVector::ZeroVector;
	if (bCharging)
	{
		// 该段起冲瞬间瞄准目标的方向
		Facing = ChargeDirection;
	}
	else if (const APlayerCharacter* Target = Enemy->GetCombatTarget())
	{
		Facing = Target->GetActorLocation() - Enemy->GetActorLocation();
		Facing.Z = 0.0f;
	}

	// 没有目标时保持当前朝向，不强行转头
	if (!Facing.IsNearlyZero())
	{
		Enemy->SetActorRotation(Facing.GetSafeNormal().Rotation());
	}

	if (!bCharging)
	{
		return;
	}

	// 冲撞位移：撞到障碍/目标就提前收束
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

void UBiteyState_Elite::ExecuteAttack()
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

	// 每次进入只执行一种攻击方式，并推进到下一种：三种一轮循环
	ActiveAttackMode = CurrentAttackMode % BiteyEliteAttackModeCount;
	CurrentAttackMode = (ActiveAttackMode + 1) % BiteyEliteAttackModeCount;

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey elite %s attack mode %d(%s) started."),
		*Enemy->GetName(),
		ActiveAttackMode,
		GetBiteyEliteModeName(ActiveAttackMode));

	switch (ActiveAttackMode)
	{
	case static_cast<int32>(EBiteyEliteAttackMode::TripleCharge):
		EnterTripleChargeMode(*Enemy);
		break;

	case static_cast<int32>(EBiteyEliteAttackMode::LineUpCharge):
		EnterLineUpMode(*Enemy);
		break;

	case static_cast<int32>(EBiteyEliteAttackMode::OrbitFour):
	default:
		EnterOrbitOrbMode(*Enemy);
		break;
	}
}

// ──────────────────────────────
// 模式 0：四个斜向法球绕行
// ──────────────────────────────

void UBiteyState_Elite::EnterOrbitOrbMode(AEnemyBase& Enemy)
{
	if (!OrbAttackAreaClass)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Bitey elite %s orbit mode skipped: OrbAttackAreaClass is missing (assign an AAttackAreaBase_Orbit subclass on this state)."),
			*Enemy.GetName());
		ScheduleReturnToChase(Enemy);
		return;
	}

	SummonOrbs(Enemy);
	ScheduleReturnToChase(Enemy);
}

void UBiteyState_Elite::SummonOrbs(AEnemyBase& Enemy)
{
	// 先清掉上一轮残留（例如上一轮冲撞被打断、法球还没发射出去）：
	// 保证每次都是“固定几枚、固定角度”的造型，而不是越攒越多。
	ClearOrbs();

	const int32 Count = FMath::Max(1, OrbCount);
	const float StepDegrees = 360.0f / static_cast<float>(Count);
	const float EnemyYaw = Enemy.GetActorRotation().Yaw;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		SpawnOrb(Enemy, EnemyYaw + OrbFirstAngleOffset + StepDegrees * static_cast<float>(Index));
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey elite %s summoned %d orb(s): Radius=%.0f AngularSpeed=%.1f LifeTime=%.2fs."),
		*Enemy.GetName(),
		Count,
		OrbOrbitRadius,
		OrbOrbitSpeed,
		OrbLifeTime);
}

void UBiteyState_Elite::SpawnOrb(AEnemyBase& Enemy, float StartAngleDegrees)
{
	const float AngleRadians = FMath::DegreesToRadians(StartAngleDegrees);
	const FVector Radial(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
	const FVector SpawnLocation = Enemy.GetActorLocation()
		+ Radial * FMath::Max(0.0f, OrbOrbitRadius)
		+ FVector(0.0f, 0.0f, OrbHeightOffset);

	AAttackAreaBase_Orbit* Orb = CreateOrb(
		Enemy,
		SpawnLocation,
		FRotator(0.0f, StartAngleDegrees, 0.0f));
	if (!Orb)
	{
		return;
	}

	// 生成之后再开始绕行：BeginPlay 会重置基类的寿命计时
	Orb->InitializeOrbit(
		&Enemy,
		OrbOrbitRadius,
		OrbOrbitSpeed,
		StartAngleDegrees,
		OrbHeightOffset,
		OrbLifeTime);
}

// ──────────────────────────────
// 模式 1：三段冲撞 → 延迟发射法球
// ──────────────────────────────

void UBiteyState_Elite::EnterTripleChargeMode(AEnemyBase& Enemy)
{
	BeginChargeSequence(Enemy);
}

void UBiteyState_Elite::BeginChargeSequence(AEnemyBase& Enemy)
{
	ChargesRemaining = FMath::Max(1, ChargeRepeatCount);
	StartCharge(Enemy);
}

void UBiteyState_Elite::StartCharge(AEnemyBase& Enemy)
{
	// 每段冲撞开始时重新朝目标瞄准，冲撞期间不再转向
	FVector ToTarget = FVector::ZeroVector;
	if (const APlayerCharacter* Target = Enemy.GetCombatTarget())
	{
		ToTarget = Target->GetActorLocation() - Enemy.GetActorLocation();
		ToTarget.Z = 0.0f;
	}

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

void UBiteyState_Elite::StartChargeRepeat()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead || !IsStillCurrentState())
	{
		return;
	}

	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	StartCharge(*Enemy);
}

void UBiteyState_Elite::BeginCharge(AEnemyBase& Enemy)
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
			TEXT("Bitey elite %s charge has no attack area class (set ChargeAttackAreaClass on this state or AttackAreaClass on the enemy); the dash will deal no damage."),
			*Enemy.GetName());
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey elite %s charge started: Speed=%.1f Duration=%.2fs Direction=(%.2f,%.2f) SegmentsLeft=%d."),
		*Enemy.GetName(),
		Enemy.GetEffectiveMoveSpeed(Enemy.ChargeSpeed),
		Enemy.ChargeDuration,
		ChargeDirection.X,
		ChargeDirection.Y,
		ChargesRemaining);

	if (UWorld* World = GetWorld())
	{
		// rate <= 0 会被 SetTimer 直接丢弃，所以冲撞时长要抬到极小正数
		World->GetTimerManager().SetTimer(
			ChargeEndTimerHandle,
			this,
			&UBiteyState_Elite::FinishChargeByDuration,
			FMath::Max(Enemy.ChargeDuration, KINDA_SMALL_NUMBER),
			false);
	}
}

void UBiteyState_Elite::FinishCharge(AEnemyBase& Enemy, const TCHAR* EndReason)
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

	if (--ChargesRemaining > 0)
	{
		// 多段冲撞：隔一段间隔再冲下一段
		const float RepeatInterval = FMath::Max(0.0f, ChargeRepeatInterval);

		UE_LOG(LogRiverOfInk, Log,
			TEXT("Bitey elite %s charge ended: Reason=%s; %d segment(s) left, next in %.2fs."),
			*Enemy.GetName(),
			EndReason ? EndReason : TEXT("Unknown"),
			ChargesRemaining,
			RepeatInterval);

		if (UWorld* World = GetWorld())
		{
			// rate <= 0 会被 SetTimer 直接丢弃，间隔 0 时也要抬到极小正数
			World->GetTimerManager().SetTimer(
				ChargeRepeatTimerHandle,
				this,
				&UBiteyState_Elite::StartChargeRepeat,
				FMath::Max(RepeatInterval, KINDA_SMALL_NUMBER),
				false);
		}

		return;
	}

	// 冲撞链结束：两种攻击方式都是“把留存的法球收回自身、靠近后删除”，
	// 只有延迟不同（模式 1 用 TripleChargeRecallDelay，模式 2 用 OrbRecallDelay）。
	const bool bLineUpMode = ActiveAttackMode == static_cast<int32>(EBiteyEliteAttackMode::LineUpCharge);
	const float RecallDelay = bLineUpMode
		? FMath::Max(0.0f, OrbRecallDelay)
		: FMath::Max(0.0f, TripleChargeRecallDelay);

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey elite %s charge chain finished; recalling orbs in %.2fs."),
		*Enemy.GetName(),
		RecallDelay);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			OrbLaunchTimerHandle,
			this,
			&UBiteyState_Elite::RecallOrbsAndRecover,
			FMath::Max(RecallDelay, KINDA_SMALL_NUMBER),
			false);
	}
}

void UBiteyState_Elite::FinishChargeByDuration()
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		FinishCharge(*Enemy, TEXT("Duration"));
	}
}

// ──────────────────────────────
// 模式 2：横向排开 → 冲撞 → 回收
// ──────────────────────────────

void UBiteyState_Elite::EnterLineUpMode(AEnemyBase& Enemy)
{
	if (!OrbAttackAreaClass)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Bitey elite %s line-up mode: OrbAttackAreaClass is missing; the charge will run without orbs."),
			*Enemy.GetName());
	}

	SummonLineOrbs(Enemy);

	// 排开之后延迟冲撞：这段时间玩家看得到“一排法球”的起手
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			OrbLaunchTimerHandle,
			this,
			&UBiteyState_Elite::StartLineUpDash,
			FMath::Max(LineUpDashDelay, KINDA_SMALL_NUMBER),
			false);
	}
}

void UBiteyState_Elite::SummonLineOrbs(AEnemyBase& Enemy)
{
	// 与模式 0 一致：先清掉上一轮残留，保证每次都是固定造型
	ClearOrbs();

	if (!OrbAttackAreaClass)
	{
		return;
	}

	const int32 PerSide = FMath::Max(1, LineOrbsPerSide);
	const float Spacing = FMath::Max(1.0f, LineOrbSpacing);
	const FVector Center = Enemy.GetActorLocation();
	// 自身朝向的左右：ActorRightVector 为正（右），取负即左
	const FVector Right = Enemy.GetActorRightVector().GetSafeNormal2D();
	const FRotator SpawnRotation = Enemy.GetActorRotation();

	int32 SpawnedCount = 0;
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 Index = 1; Index <= PerSide; ++Index)
		{
			const FVector SpawnLocation = Center
				+ Right * (Spacing * static_cast<float>(Side * Index))
				+ FVector(0.0f, 0.0f, OrbHeightOffset);

			AAttackAreaBase_Orbit* Orb = CreateOrb(Enemy, SpawnLocation, SpawnRotation);
			if (!Orb)
			{
				continue;
			}

			// 编队：出生位置即相对宿主的偏移，之后跟着宿主一起移动与转向
			Orb->InitializeFormation(&Enemy, OrbLifeTime);
			++SpawnedCount;
		}
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey elite %s lined up %d orb(s): PerSide=%d Spacing=%.0f DashDelay=%.2fs."),
		*Enemy.GetName(),
		SpawnedCount,
		PerSide,
		Spacing,
		LineUpDashDelay);
}

void UBiteyState_Elite::StartLineUpDash()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead || !IsStillCurrentState())
	{
		return;
	}

	Enemy->RefreshCombatTarget();
	if (!Enemy->HasValidCombatTarget())
	{
		Enemy->SwitchState(UEnemyState_TargetLost::StaticClass());
		return;
	}

	// 模式 2 只冲一段
	ChargesRemaining = 1;
	StartCharge(*Enemy);
}

void UBiteyState_Elite::RecallOrbsAndRecover()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->bIsDead || !IsStillCurrentState())
	{
		return;
	}

	RecallOrbs(*Enemy);
	ScheduleReturnToChase(*Enemy);
}

void UBiteyState_Elite::RecallOrbs(AEnemyBase& Enemy)
{
	TArray<AAttackAreaBase_Orbit*> AliveOrbs;
	GatherAliveOrbs(AliveOrbs);

	int32 RecalledCount = 0;
	for (AAttackAreaBase_Orbit* Orb : AliveOrbs)
	{
		if (!IsValid(Orb))
		{
			continue;
		}

		// 已经发射出去的法球不会被回收（AAttackAreaBase_Orbit::RecallToHost 内部会忽略）
		Orb->RecallToHost(OrbRecallSpeed, OrbRecallAcceptRadius);
		if (Orb->IsRecallingToHost())
		{
			++RecalledCount;
		}
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Bitey elite %s recalling %d/%d orb(s): Speed=%.1f AcceptRadius=%.1f."),
		*Enemy.GetName(),
		RecalledCount,
		AliveOrbs.Num(),
		OrbRecallSpeed,
		OrbRecallAcceptRadius);
}

// ──────────────────────────────
// 工具
// ──────────────────────────────

AAttackAreaBase_Orbit* UBiteyState_Elite::CreateOrb(
	AEnemyBase& Enemy,
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation)
{
	UWorld* World = GetWorld();
	if (!World || !OrbAttackAreaClass)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
	AAttackAreaBase_Orbit* Orb = World->SpawnActorDeferred<AAttackAreaBase_Orbit>(
		OrbAttackAreaClass,
		SpawnTransform,
		&Enemy,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Orb)
	{
		UE_LOG(LogRiverOfInk, Warning, TEXT("Bitey elite %s orb spawn failed."), *Enemy.GetName());
		return nullptr;
	}

	// 伤害类型与过滤：Owner 是敌人，bDamageOpponentOnly 会只打玩家
	Orb->AttackDamageProfile.AttackType = EAttackType::EnemyRanged;
	Orb->bDamageOpponentOnly = true;
	Orb->bDetectObstacle = Enemy.bAttackAreaDetectObstacle;

	UGameplayStatics::FinishSpawningActor(Orb, SpawnTransform);

	Orbs.Add(Orb);
	return Orb;
}

void UBiteyState_Elite::ClearOrbs()
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

void UBiteyState_Elite::PruneOrbs()
{
	for (int32 Index = Orbs.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(Orbs[Index].Get()))
		{
			Orbs.RemoveAtSwap(Index);
		}
	}
}

void UBiteyState_Elite::GatherAliveOrbs(TArray<AAttackAreaBase_Orbit*>& OutOrbs)
{
	OutOrbs.Reset();
	PruneOrbs();

	for (const TWeakObjectPtr<AAttackAreaBase_Orbit>& Orb : Orbs)
	{
		if (AAttackAreaBase_Orbit* ValidOrb = Orb.Get())
		{
			OutOrbs.Add(ValidOrb);
		}
	}
}

void UBiteyState_Elite::ScheduleReturnToChase(AEnemyBase& Enemy)
{
	if (UWorld* World = GetWorld())
	{
		// 同上：后摇为 0 时也要真的创建计时器，否则永远回不到 Chase
		World->GetTimerManager().SetTimer(
			ReturnTimerHandle,
			this,
			&UBiteyState_Elite::ReturnToChase,
			FMath::Max(Enemy.AttackRecoveryTime, KINDA_SMALL_NUMBER),
			false);
	}
}

void UBiteyState_Elite::ClearChargeAttackArea()
{
	if (IsValid(ChargeAttackArea.Get()))
	{
		ChargeAttackArea->Destroy();
	}

	ChargeAttackArea = nullptr;
}

TSubclassOf<AAttackAreaBase> UBiteyState_Elite::ResolveChargeClass(const AEnemyBase& Enemy) const
{
	return ChargeAttackAreaClass ? ChargeAttackAreaClass : Enemy.AttackAreaClass;
}

bool UBiteyState_Elite::IsStillCurrentState() const
{
	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	const UStateBase* Current = Enemy ? Enemy->CurrentState.Get() : nullptr;
	return Current == this;
}

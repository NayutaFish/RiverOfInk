// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/LanternGhost/State/LanternGhostState_Ranged_Elite.h"

#include "Common/AttackAreaBase.h"
#include "Common/AttackArea/AttackAreaBase_Bezier.h"
#include "Common/AttackArea/AttackAreaBaseBz_LanternGhostRange.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RiverOfInk.h"
#include "TimerManager.h"

namespace
{
	/** 攻击方式数量：贝塞尔连发 / 直线散射连发 / 八方向环形。 */
	constexpr int32 LanternGhostRangedEliteModeCount = 3;

	/** 攻击方式枚举（仅用于可读性，与 CurrentAttackMode 的取值一一对应）。 */
	enum class ELanternGhostRangedEliteMode : int32
	{
		BezierVolley = 0,
		SpreadBurst = 1,
		RingBurst = 2
	};

	const TCHAR* GetModeName(int32 Mode)
	{
		switch (Mode)
		{
		case static_cast<int32>(ELanternGhostRangedEliteMode::SpreadBurst): return TEXT("SpreadBurst");
		case static_cast<int32>(ELanternGhostRangedEliteMode::RingBurst): return TEXT("RingBurst");
		case static_cast<int32>(ELanternGhostRangedEliteMode::BezierVolley):
		default: return TEXT("BezierVolley");
		}
	}
}

ULanternGhostState_Ranged_Elite::ULanternGhostState_Ranged_Elite()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULanternGhostState_Ranged_Elite::OnEnter_Implementation()
{
	Super::OnEnter_Implementation();

	if (const AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		UE_LOG(LogRiverOfInk, Log,
			TEXT("LanternGhost ranged elite %s attack state entered: NextMode=%d(%s)."),
			*Enemy->GetName(),
			CurrentAttackMode,
			GetModeName(CurrentAttackMode));
	}
}

void ULanternGhostState_Ranged_Elite::OnExit_Implementation()
{
	// 序列排程全部清理：被打断（硬值击破 / 切状态 / 死亡）时不再补发剩余发射
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RepeatTimerHandle);
		World->GetTimerManager().ClearTimer(ReturnTimerHandle);
	}

	ShotsRemaining = 0;
	bSequenceActive = false;

	Super::OnExit_Implementation();
}

void ULanternGhostState_Ranged_Elite::ExecuteAttack()
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

	if (bSequenceActive)
	{
		UE_LOG(LogRiverOfInk, Verbose,
			TEXT("LanternGhost ranged elite %s attack ignored: sequence already running."),
			*Enemy->GetName());
		return;
	}

	// 每次进入攻击状态只执行一种攻击方式，并推进到下一模式：三次蓄力攻击 = 一轮循环
	ActiveAttackMode = CurrentAttackMode % LanternGhostRangedEliteModeCount;
	CurrentAttackMode = (ActiveAttackMode + 1) % LanternGhostRangedEliteModeCount;

	// 该模式需要的射弹类必须就绪，否则本次直接跳过（循环照常推进）
	const TSubclassOf<AAttackAreaBase> RequiredClass = ActiveAttackMode == static_cast<int32>(ELanternGhostRangedEliteMode::BezierVolley)
		? ResolveBezierClass(*Enemy)
		: ResolveStraightClass(*Enemy);

	if (!RequiredClass)
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("LanternGhost ranged elite %s mode %d(%s) skipped: no projectile class (set BezierProjectileClass / StraightProjectileClass, or the correct enemy AttackAreaClass)."),
			*Enemy->GetName(),
			ActiveAttackMode,
			GetModeName(ActiveAttackMode));
		ReturnToChase();
		return;
	}

	ShotsRemaining = GetShotCountForMode(ActiveAttackMode);
	bSequenceActive = true;

	UE_LOG(LogRiverOfInk, Log,
		TEXT("LanternGhost ranged elite %s attack mode %d(%s) started: Shots=%d Interval=%.2fs Class=%s."),
		*Enemy->GetName(),
		ActiveAttackMode,
		GetModeName(ActiveAttackMode),
		ShotsRemaining,
		RepeatInterval,
		*GetNameSafe(RequiredClass.Get()));

	PerformShotStep();
}

void ULanternGhostState_Ranged_Elite::PerformShotStep()
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	UWorld* World = GetWorld();
	if (!Enemy || Enemy->bIsDead || !World || !IsStillCurrentState())
	{
		ShotsRemaining = 0;
		bSequenceActive = false;
		return;
	}

	FireOnce(*Enemy);
	--ShotsRemaining;

	if (ShotsRemaining > 0)
	{
		// 注意：rate <= 0 会被 SetTimer 直接丢弃，这里抬一个极小正数
		World->GetTimerManager().SetTimer(
			RepeatTimerHandle,
			this,
			&ULanternGhostState_Ranged_Elite::PerformShotStep,
			FMath::Max(RepeatInterval, KINDA_SMALL_NUMBER),
			false);
	}
	else
	{
		bSequenceActive = false;
		UE_LOG(LogRiverOfInk, Log,
			TEXT("LanternGhost ranged elite %s attack mode %d(%s) finished; returning to Chase."),
			*Enemy->GetName(),
			ActiveAttackMode,
			GetModeName(ActiveAttackMode));
		ScheduleReturnToChase();
	}
}

void ULanternGhostState_Ranged_Elite::FireOnce(AEnemyBase& Enemy)
{
	// 三种攻击方式全部以“自身朝向”为准：攻击状态期间基类会把朝向锁定在
	// “进入攻击时面向目标”的方向，此后不再跟随玩家移动，玩家可以靠走位躲开弹幕。
	const float FacingYaw = Enemy.GetActorRotation().Yaw;

	switch (ActiveAttackMode)
	{
	case static_cast<int32>(ELanternGhostRangedEliteMode::SpreadBurst):
		FireSpreadBurst(Enemy, FacingYaw);
		break;

	case static_cast<int32>(ELanternGhostRangedEliteMode::RingBurst):
		FireRingBurst(Enemy, FacingYaw);
		break;

	case static_cast<int32>(ELanternGhostRangedEliteMode::BezierVolley):
	default:
		FireBezierVolley(Enemy, FacingYaw);
		break;
	}
}

void ULanternGhostState_Ranged_Elite::FireBezierVolley(AEnemyBase& Enemy, float FacingYaw)
{
	UWorld* World = GetWorld();
	const TSubclassOf<AAttackAreaBase> ResolvedClass = ResolveBezierClass(Enemy);
	if (!World || !ResolvedClass)
	{
		return;
	}

	// 终点 = 自身位置 + 朝向 × 前向距离，不取玩家当前位置
	const FRotator SpawnRotation(0.0f, FacingYaw, 0.0f);
	const FVector SpawnLocation = Enemy.GetActorLocation();
	const FVector TargetLocation = SpawnLocation
		+ SpawnRotation.Vector() * FMath::Max(0.0f, BezierTargetForwardDistance);
	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
	const TSubclassOf<AAttackAreaBase_Bezier> BezierClass(ResolvedClass);

	// 左右两侧各一枚：靠控制点 P1 的横向偏移拉开弧线
	const float SideOffsets[] = { -BezierSideOffset, BezierSideOffset };
	int32 SpawnedCount = 0;
	for (const float SideOffset : SideOffsets)
	{
		AAttackAreaBase_Bezier* BezierArea = World->SpawnActorDeferred<AAttackAreaBase_Bezier>(
			BezierClass,
			SpawnTransform,
			&Enemy,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (!BezierArea)
		{
			continue;
		}

		BezierArea->Initialize(Enemy.AttackAreaLifeTime, 0.0f, false, nullptr);
		BezierArea->BezierP1PositionRate = BezierP1PositionRate;
		BezierArea->BezierP1Offset = SideOffset;
		BezierArea->bDamageOpponentOnly = true;
		BezierArea->bIsEnemyProjectile = true;
		BezierArea->bDetectObstacle = Enemy.bAttackAreaDetectObstacle;

		// 实际使用灯笼怪远程专用贝塞尔子类时，补齐慢启动与 Niagara 缩放参数
		if (AAttackAreaBaseBz_LanternGhostRange* LanternArea = Cast<AAttackAreaBaseBz_LanternGhostRange>(BezierArea))
		{
			LanternArea->InitialSpeedScale = BezierInitialSpeedScale;
			LanternArea->SlowDuration = BezierSlowDuration;
			LanternArea->SlowStartDelay = BezierSlowStartDelay;
			LanternArea->ScaleSizeStart = BezierScaleSizeStart;
			LanternArea->ScaleSizeEnd = BezierScaleSizeEnd;
			LanternArea->ScaleSizeDuration = BezierScaleSizeDuration;
		}

		UGameplayStatics::FinishSpawningActor(BezierArea, SpawnTransform);
		BezierArea->SetBezierTarget(TargetLocation);
		++SpawnedCount;
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("LanternGhost ranged elite %s bezier volley: %d/2 projectiles, P1Rate=%.2f SideOffset=%.0f."),
		*Enemy.GetName(),
		SpawnedCount,
		BezierP1PositionRate,
		BezierSideOffset);
}

void ULanternGhostState_Ranged_Elite::FireSpreadBurst(AEnemyBase& Enemy, float FacingYaw)
{
	const TSubclassOf<AAttackAreaBase> ResolvedClass = ResolveStraightClass(Enemy);
	if (!ResolvedClass)
	{
		return;
	}

	const int32 ProjectileCount = FMath::Max(1, SpreadProjectileCount);
	// 以自身朝向为中心对称展开，相邻夹角 SpreadAngleStep
	const float StartYaw = FacingYaw - SpreadAngleStep * (ProjectileCount - 1) * 0.5f;

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		SpawnStraightProjectile(Enemy, ResolvedClass, StartYaw + SpreadAngleStep * Index);
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("LanternGhost ranged elite %s spread burst: %d projectiles, step=%.1fdeg centerYaw=%.1f."),
		*Enemy.GetName(),
		ProjectileCount,
		SpreadAngleStep,
		FacingYaw);
}

void ULanternGhostState_Ranged_Elite::FireRingBurst(AEnemyBase& Enemy, float FacingYaw)
{
	const TSubclassOf<AAttackAreaBase> ResolvedClass = ResolveStraightClass(Enemy);
	if (!ResolvedClass)
	{
		return;
	}

	const int32 ProjectileCount = FMath::Max(1, RingProjectileCount);
	const float YawStep = 360.0f / static_cast<float>(ProjectileCount);

	// 以面朝方向为 0 度均匀展开：8 枚即 前/右前/右/右后/后/左后/左/左前
	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		SpawnStraightProjectile(Enemy, ResolvedClass, FacingYaw + YawStep * Index);
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("LanternGhost ranged elite %s ring burst: %d projectiles, step=%.1fdeg facingYaw=%.1f."),
		*Enemy.GetName(),
		ProjectileCount,
		YawStep,
		FacingYaw);
}

void ULanternGhostState_Ranged_Elite::SpawnStraightProjectile(
	AEnemyBase& Enemy,
	TSubclassOf<AAttackAreaBase> ProjectileClass,
	float Yaw)
{
	UWorld* World = GetWorld();
	if (!World || !ProjectileClass)
	{
		return;
	}

	const FRotator SpawnRotation(0.0f, Yaw, 0.0f);
	const FVector SpawnLocation = Enemy.GetActorLocation()
		+ SpawnRotation.Vector() * FMath::Max(0.0f, StraightMuzzleOffset);
	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

	AAttackAreaBase* Projectile = World->SpawnActorDeferred<AAttackAreaBase>(
		ProjectileClass,
		SpawnTransform,
		&Enemy,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Projectile)
	{
		return;
	}

	// 与基类攻击状态一致：直线弹靠朝向 + Speed 飞行，速度必须大于 0
	Projectile->AttackDamageProfile.AttackType = EAttackType::EnemyRanged;
	Projectile->Initialize(
		Enemy.AttackAreaLifeTime,
		FMath::Max(StraightProjectileSpeed, 1.0f),
		false,
		nullptr);
	Projectile->bDamageOpponentOnly = true;
	Projectile->bDetectObstacle = Enemy.bAttackAreaDetectObstacle;
	Projectile->bIsEnemyProjectile = true;

	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
}

TSubclassOf<AAttackAreaBase> ULanternGhostState_Ranged_Elite::ResolveBezierClass(const AEnemyBase& Enemy) const
{
	if (BezierProjectileClass)
	{
		return BezierProjectileClass;
	}

	// 回退：敌人的 AttackAreaClass 是贝塞尔子类时直接复用（与 LanternGhostState_Ranged 行为一致）
	if (Enemy.AttackAreaClass
		&& Enemy.AttackAreaClass->IsChildOf(AAttackAreaBase_Bezier::StaticClass()))
	{
		return Enemy.AttackAreaClass;
	}

	return nullptr;
}

TSubclassOf<AAttackAreaBase> ULanternGhostState_Ranged_Elite::ResolveStraightClass(const AEnemyBase& Enemy) const
{
	if (StraightProjectileClass)
	{
		return StraightProjectileClass;
	}

	// 回退：敌人的 AttackAreaClass 不是贝塞尔子类时当作直线射弹用
	if (Enemy.AttackAreaClass
		&& !Enemy.AttackAreaClass->IsChildOf(AAttackAreaBase_Bezier::StaticClass()))
	{
		return Enemy.AttackAreaClass;
	}

	return nullptr;
}

int32 ULanternGhostState_Ranged_Elite::GetShotCountForMode(int32 Mode) const
{
	switch (Mode)
	{
	case static_cast<int32>(ELanternGhostRangedEliteMode::SpreadBurst):
		return FMath::Max(1, SpreadBurstCount);

	case static_cast<int32>(ELanternGhostRangedEliteMode::RingBurst):
		return 1;

	case static_cast<int32>(ELanternGhostRangedEliteMode::BezierVolley):
	default:
		return FMath::Max(1, BezierVolleyCount);
	}
}

void ULanternGhostState_Ranged_Elite::ScheduleReturnToChase()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	const float RecoveryTime = Enemy ? Enemy->AttackRecoveryTime : 0.3f;

	// 同上：后摇为 0 时必须抬到极小正数，否则永远回不到 Chase
	World->GetTimerManager().SetTimer(
		ReturnTimerHandle,
		this,
		&ULanternGhostState_Ranged_Elite::ReturnToChase,
		FMath::Max(RecoveryTime, KINDA_SMALL_NUMBER),
		false);
}

bool ULanternGhostState_Ranged_Elite::IsStillCurrentState() const
{
	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	const UStateBase* Current = Enemy ? Enemy->CurrentState.Get() : nullptr;
	return Current == this;
}

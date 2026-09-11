// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/AttackArea/AttackAreaBase_Orbit.h"

#include "Engine/World.h"
#include "RiverOfInk.h"

namespace
{
	/**
	 * 交给基类的寿命窗口。
	 *
	 * 本类的绕行寿命与飞行寿命都是自己按 OrbitElapsedTime 判定的：基类的 ElapsedTime 只有
	 * BeginPlay 会归零，Initialize 不会重置它，所以“绕行一段时间后再发射、发射后重新起算寿命”
	 * 这件事没法完全交给基类的 LifeTime。这里给基类一个足够大的窗口兜底，避免它提前把法球收掉。
	 */
	constexpr float OrbitSafetyLifetime = 3600.0f;
}

AAttackAreaBase_Orbit::AAttackAreaBase_Orbit()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAttackAreaBase_Orbit::InitializeOrbit(
	AActor* InCenter,
	float InRadius,
	float InAngularSpeedDegrees,
	float InStartAngleDegrees,
	float InHeightOffset,
	float InOrbitLifeTime)
{
	OrbitCenter = InCenter;
	OrbitRadius = FMath::Max(0.0f, InRadius);
	OrbitAngularSpeedDegrees = InAngularSpeedDegrees;
	OrbitCurrentAngleDegrees = InStartAngleDegrees;
	OrbitHeightOffset = InHeightOffset;
	OrbitLifeTime = FMath::Max(0.0f, InOrbitLifeTime);
	OrbitElapsedTime = 0.0f;
	LaunchElapsedTime = 0.0f;
	FlightLifeTime = 0.0f;
	bLaunched = false;

	// 绕行阶段按“近战”初始化：攻击区域持续存在，并且对同一个目标只结算一次接触伤害，
	// 不会被第一次接触就销毁（远程模式会命中即销毁，那样法球一碰到玩家就没了）。
	Initialize(OrbitSafetyLifetime, 0.0f, true, nullptr);
	bIsEnemyProjectile = false;
	bOrbiting = false;

	if (InCenter)
	{
		bOrbiting = true;
		// DeltaTime = 0：只把法球摆到起始角度，角度推进留给第一次 Tick
		UpdateOrbitLocation(*InCenter, 0.0f);
	}
}

void AAttackAreaBase_Orbit::LaunchAsProjectile(const FVector& InDirection, float InSpeed, float InFlightLifeTime)
{
	if (bLaunched || IsActorBeingDestroyed())
	{
		return;
	}

	// 只取水平方向：与其它敌人射弹一致，飞行过程中高度保持不变
	FVector Direction = InDirection;
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		Direction = GetActorForwardVector();
		Direction.Z = 0.0f;
		if (!Direction.Normalize())
		{
			Direction = FVector::ForwardVector;
		}
	}

	FlightLifeTime = FMath::Max(0.05f, InFlightLifeTime);
	LaunchElapsedTime = OrbitElapsedTime;
	bOrbiting = false;
	bLaunched = true;

	// 重新武装命中表：绕行阶段可能已经命中过同一个目标，发射后要能再结算一次
	ResetHitActors();

	// 发射后按射弹处理：命中即结算并销毁、可被净墨环消除
	bIsMeleeAttack = false;
	bIsEnemyProjectile = true;
	Speed = FMath::Max(1.0f, InSpeed);
	SetActorRotation(Direction.Rotation());

	UE_LOG(LogRiverOfInk, Log,
		TEXT("Orbit attack area %s launched: Speed=%.1f FlightLife=%.2fs Direction=(%.2f,%.2f)."),
		*GetName(),
		Speed,
		FlightLifeTime,
		Direction.X,
		Direction.Y);

	// 发射瞬间可能已经和目标贴在一起（绕行半径小、玩家贴脸）：立刻结算一次，
	// 否则要等对方先离开再撞进来才会重新触发 BeginOverlap，这一发就等于穿过去了。
	if (ApplyDamageToOverlappingTargets() > 0)
	{
		Disappear(EAttackAreaDisappearReason::HitEnemy);
	}
}

void AAttackAreaBase_Orbit::Tick(float DeltaTime)
{
	if (IsActorBeingDestroyed())
	{
		return;
	}

	OrbitElapsedTime += DeltaTime;

	if (bOrbiting)
	{
		AActor* Center = OrbitCenter.Get();
		if (!IsValid(Center))
		{
			// 中心（施放者）已经没了：法球不该继续留在场上
			UE_LOG(LogRiverOfInk, Log,
				TEXT("Orbit attack area %s: orbit center is gone; disappearing."),
				*GetName());
			Disappear(EAttackAreaDisappearReason::Lifetime);
			return;
		}

		if (OrbitLifeTime > 0.0f && OrbitElapsedTime >= OrbitLifeTime)
		{
			UE_LOG(LogRiverOfInk, Log,
				TEXT("Orbit attack area %s: orbit lifetime %.2fs ended."),
				*GetName(),
				OrbitLifeTime);
			Disappear(EAttackAreaDisappearReason::Lifetime);
			return;
		}

		UpdateOrbitLocation(*Center, DeltaTime);
	}
	else if (bLaunched && (OrbitElapsedTime - LaunchElapsedTime) >= FlightLifeTime)
	{
		Disappear(EAttackAreaDisappearReason::Lifetime);
		return;
	}

	// 基类负责：近战持续判定 / 射弹重叠结算 / 障碍检测 / 发射后的直线位移
	Super::Tick(DeltaTime);
}

void AAttackAreaBase_Orbit::UpdateOrbitLocation(const AActor& Center, float DeltaTime)
{
	OrbitCurrentAngleDegrees = FMath::UnwindDegrees(
		OrbitCurrentAngleDegrees + OrbitAngularSpeedDegrees * DeltaTime);

	const float AngleRadians = FMath::DegreesToRadians(OrbitCurrentAngleDegrees);
	const FVector Radial(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f);
	const FVector Offset = Radial * OrbitRadius + FVector(0.0f, 0.0f, OrbitHeightOffset);

	// 绕行是绕着施放者的固定轨道，直接摆位（不做 sweep），避免被场景碰撞挡住后脱离轨道
	SetActorLocation(Center.GetActorLocation() + Offset, false);
}

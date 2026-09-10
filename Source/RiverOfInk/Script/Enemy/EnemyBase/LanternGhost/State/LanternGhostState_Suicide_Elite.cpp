// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/LanternGhost/State/LanternGhostState_Suicide_Elite.h"

#include "Enemy/EnemyBase/EnemyBase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Player/PlayerCharacter.h"
#include "RiverOfInk.h"
#include "TimerManager.h"

ULanternGhostState_Suicide_Elite::ULanternGhostState_Suicide_Elite()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULanternGhostState_Suicide_Elite::BeginPlay()
{
	Super::BeginPlay();

	// 绑定一次即可：状态会被反复进出，但组件与宿主同生共死。
	// 宿主死亡时要能立刻处理召唤物，所以不能挂到 OnEnter/OnExit 上。
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		Enemy->OnEnemyDeath.AddDynamic(this, &ULanternGhostState_Suicide_Elite::HandleOwnerDeath);
	}
}

void ULanternGhostState_Suicide_Elite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		Enemy->OnEnemyDeath.RemoveDynamic(this, &ULanternGhostState_Suicide_Elite::HandleOwnerDeath);
	}

	Super::EndPlay(EndPlayReason);
}

void ULanternGhostState_Suicide_Elite::HandleOwnerDeath(AActor* DeadEnemy)
{
	(void)DeadEnemy;

	bOwnerDead = true;
	PendingSummonTransforms.Reset();

	if (!bKillSummonsOnOwnerDeath)
	{
		SummonedEnemies.Reset();
		return;
	}

	int32 KilledCount = 0;
	for (const TObjectPtr<AEnemyBase>& Summoned : SummonedEnemies)
	{
		if (IsValid(Summoned) && !Summoned->bIsDead)
		{
			// 走召唤物自己的死亡流程：掉落、死亡事件、溶解与延迟销毁都照常执行
			Summoned->Die();
			++KilledCount;
		}
	}

	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("None");
	SummonedEnemies.Reset();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("LanternGhost elite %s died: %d surviving summons were ordered to die."),
		*OwnerName,
		KilledCount);
}

void ULanternGhostState_Suicide_Elite::OnEnter_Implementation()
{
	// 先打印配置，便于排查“进了状态却没召唤”的问题。
	if (const AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		UE_LOG(LogRiverOfInk, Log,
			TEXT("LanternGhost elite %s summon state entered: SummonClass=%s VFX=%s Windup=%.2f Delay=%.2f."),
			*Enemy->GetName(),
			*GetNameSafe(summonEnemyBase.Get()),
			*GetNameSafe(summonVFX.Get()),
			Enemy->AttackWindupTime,
			SummonSpawnDelay);
	}

	Super::OnEnter_Implementation();
}

void ULanternGhostState_Suicide_Elite::OnExit_Implementation()
{
	// 只清理“返回追击”的计时器：召唤生成计时器保留，
	// 避免被硬值打断时已经起手的召唤凭空消失。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackReturnHandle);
	}

	Super::OnExit_Implementation();
}

void ULanternGhostState_Suicide_Elite::ExecuteAttack()
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

	if (!summonEnemyBase)
	{
		// 未配置召唤物时退回基类的普通攻击，避免精英怪站着不动。
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s elite summon skipped: summonEnemyBase is not set; falling back to the base attack."),
			*Enemy->GetName());
		Super::ExecuteAttack();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 攻击期间基类每帧锁死朝向，这里取到的就是“自己朝向”。
	const FRotator FacingRotation = Enemy->GetActorRotation();

	TArray<FTransform> SummonTransforms;
	BuildSummonTransforms(*Enemy, SummonTransforms);

	if (SummonTransforms.IsEmpty())
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s elite summon found no valid location; returning to Chase."),
			*Enemy->GetName());
		ReturnToChase();
		return;
	}

	// 先在落点播放召唤特效
	if (summonVFX)
	{
		for (const FTransform& SummonTransform : SummonTransforms)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				summonVFX,
				SummonTransform.GetLocation(),
				SummonTransform.Rotator());
		}
	}
	else
	{
		UE_LOG(LogRiverOfInk, Warning,
			TEXT("Enemy %s elite summon has no summonVFX configured."),
			*Enemy->GetName());
	}

	PendingSummonTransforms = SummonTransforms;
	const float SpawnDelay = FMath::Max(0.0f, SummonSpawnDelay);

	if (SpawnDelay > KINDA_SMALL_NUMBER)
	{
		// 弱引用绑定本组件：状态被反复进出时召唤照常发生，
		// 但宿主被销毁后委托自动失效，不会再补生成召唤物。
		FTimerDelegate SummonDelegate = FTimerDelegate::CreateWeakLambda(
			this, [this]() { SpawnPendingSummons(); });

		World->GetTimerManager().SetTimer(
			SummonSpawnTimerHandle, SummonDelegate, SpawnDelay, false);
	}
	else
	{
		SpawnPendingSummons();
	}

	UE_LOG(LogRiverOfInk, Log,
		TEXT("LanternGhost elite %s summon attack executed: Facing=%s Locations=%d VFX=%s Delay=%.2f."),
		*Enemy->GetName(),
		*FacingRotation.ToCompactString(),
		SummonTransforms.Num(),
		summonVFX ? TEXT("Yes") : TEXT("No"),
		SpawnDelay);

	// 召唤延迟结束后再进入后摇，保证召唤过程可见。
	// 注意 SetTimer 会丢弃 rate <= 0 的计时器，两个参数都为 0 时必须抬到极小正数，
	// 否则本状态永远回不到 Chase。
	World->GetTimerManager().SetTimer(
		AttackReturnHandle,
		this,
		&ULanternGhostState_Suicide_Elite::ReturnToChase,
		FMath::Max(SpawnDelay + FMath::Max(0.0f, Enemy->AttackRecoveryTime), KINDA_SMALL_NUMBER),
		false);
}

int32 ULanternGhostState_Suicide_Elite::BuildSummonTransforms(
	const AEnemyBase& Enemy,
	TArray<FTransform>& OutTransforms) const
{
	OutTransforms.Reset();

	FVector Forward = Enemy.GetActorRotation().Vector();
	Forward.Z = 0.0f;

	if (Forward.IsNearlyZero())
	{
		Forward = Enemy.GetActorForwardVector();
		Forward.Z = 0.0f;
	}

	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	Forward = Forward.GetSafeNormal();

	// UE 中 Yaw 旋转下，朝向 F 对应的“右”为 (-F.Y, F.X, 0)。
	const FVector Right(-Forward.Y, Forward.X, 0.0f);
	const FVector BaseLocation = Enemy.GetActorLocation();
	const FRotator SummonRotation(0.0f, Forward.Rotation().Yaw, 0.0f);

	// 左前、右前各一个：负号是自身朝向的左侧。
	const float SideSigns[] = { -1.0f, 1.0f };
	const float FallbackScale = FMath::Clamp(SummonFallbackOffsetScale, 0.1f, 1.0f);

	for (const float SideSign : SideSigns)
	{
		// 首选落点同时作为兜底：占位检测全部不通过时仍然要生成，避免整个攻击什么都不做。
		const FVector PrimaryLocation = BaseLocation
			+ Forward * SummonForwardOffset
			+ Right * (SummonSideOffset * SideSign);

		// 依次尝试：完整偏移 → 缩小偏移 → 只做横向偏移（前方被墙体挡住时的兜底）。
		const FVector CandidateLocations[] =
		{
			PrimaryLocation,
			BaseLocation
				+ Forward * (SummonForwardOffset * FallbackScale)
				+ Right * (SummonSideOffset * FallbackScale * SideSign),
			BaseLocation + Right * (SummonSideOffset * SideSign)
		};

		bool bResolved = false;
		const int32 CandidateCount = static_cast<int32>(UE_ARRAY_COUNT(CandidateLocations));
		for (int32 Index = 0; Index < CandidateCount && !bResolved; ++Index)
		{
			FTransform ResolvedTransform;
			if (ResolveSummonTransform(CandidateLocations[Index], SummonRotation, ResolvedTransform))
			{
				OutTransforms.Add(ResolvedTransform);
				bResolved = true;
			}
		}

		if (!bResolved)
		{
			const FVector BestEffortLocation = bSnapSummonToGround
				? ResolveSummonLocation(PrimaryLocation)
				: PrimaryLocation;

			OutTransforms.Add(FTransform(SummonRotation, BestEffortLocation));

			UE_LOG(LogRiverOfInk, Warning,
				TEXT("Enemy %s elite summon: every candidate was blocked on the %s side; using best-effort location %s."),
				*Enemy.GetName(),
				SideSign < 0.0f ? TEXT("left") : TEXT("right"),
				*BestEffortLocation.ToCompactString());
		}
	}

	return OutTransforms.Num();
}

bool ULanternGhostState_Suicide_Elite::ResolveSummonTransform(
	const FVector& DesiredLocation,
	const FRotator& DesiredRotation,
	FTransform& OutTransform) const
{
	const FVector ResolvedLocation = bSnapSummonToGround
		? ResolveSummonLocation(DesiredLocation)
		: DesiredLocation;

	if (!IsSummonLocationClear(ResolvedLocation))
	{
		return false;
	}

	OutTransform = FTransform(DesiredRotation, ResolvedLocation);
	return true;
}

FVector ULanternGhostState_Suicide_Elite::ResolveSummonLocation(const FVector& InLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return InLocation;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams;
	if (const AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	const FVector Start = InLocation + FVector(0.0f, 0.0f, FMath::Max(0.0f, SummonGroundTraceUp));
	const FVector End = InLocation - FVector(0.0f, 0.0f, FMath::Max(0.0f, SummonGroundTraceDown));

	FHitResult Hit;
	// 高度差过大说明打到的是远处屋顶/台阶，同样视为没有可用地面
	if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, QueryParams)
		&& FMath::Abs(Hit.ImpactPoint.Z - InLocation.Z) <= SummonMaxGroundHeightDelta)
	{
		// 关键：贴的是“胶囊底面”而不是胶囊中心。直接把胶囊中心放到地面点上，
		// 胶囊会有一半埋进地面；而 Chase 用带 Sweep 的 AddActorWorldOffset 位移，
		// 起点就重叠时完全走不动（表现为召唤物原地站着只转头）。
		const float CapsuleLift = ResolveSummonHalfHeight();
		return FVector(
			InLocation.X,
			InLocation.Y,
			Hit.ImpactPoint.Z + CapsuleLift + SummonGroundZOffset);
	}

	// 浮空或找不到地面时保留自身高度，避免小怪被放到不可达的位置
	return InLocation;
}

float ULanternGhostState_Suicide_Elite::ResolveSummonHalfHeight() const
{
	// 注意不要用 GetSimpleCollisionHalfHeight()：它在 CDO 上取不到已注册的根组件，
	// 会退化成整个 Actor 的包围盒。胶囊体自身的大小与注册状态无关。
	auto QueryCapsuleHalfHeight = [](const AActor* Target) -> float
	{
		if (!Target)
		{
			return 0.0f;
		}

		if (const UCapsuleComponent* Capsule = Target->FindComponentByClass<UCapsuleComponent>())
		{
			return FMath::Max(0.0f, Capsule->GetScaledCapsuleHalfHeight());
		}

		return 0.0f;
	};

	// 优先用召唤物类自身的胶囊
	if (summonEnemyBase)
	{
		if (const float SummonHalfHeight = QueryCapsuleHalfHeight(summonEnemyBase->GetDefaultObject<AEnemyBase>()))
		{
			return SummonHalfHeight;
		}
	}

	// 兜底：宿主自身的胶囊（同类灯笼怪尺寸一致）
	return QueryCapsuleHalfHeight(GetOwner());
}

void ULanternGhostState_Suicide_Elite::SpawnPendingSummons()
{
	if (PendingSummonTransforms.IsEmpty())
	{
		return;
	}

	// 宿主已经死亡：不再补生成（召唤物不允许比宿主活得更久）
	if (bOwnerDead || !summonEnemyBase)
	{
		PendingSummonTransforms.Reset();
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	UWorld* World = GetWorld();
	if (!Enemy || Enemy->bIsDead || !World)
	{
		PendingSummonTransforms.Reset();
		return;
	}

	const TSubclassOf<AEnemyBase> ClassToSpawn = summonEnemyBase;
	APlayerCharacter* const HostTarget = Enemy->GetCombatTarget();
	int32 SpawnedCount = 0;

	for (const FTransform& SpawnTransform : PendingSummonTransforms)
	{
		AEnemyBase* Summoned = World->SpawnActorDeferred<AEnemyBase>(
			ClassToSpawn,
			SpawnTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (!Summoned)
		{
			continue;
		}

		// 用实例自己的胶囊再校正一次落点，保证生成后不与地面重叠
		const FTransform FinalTransform = MakeNonBlockingSpawnTransform(Summoned, SpawnTransform);
		UGameplayStatics::FinishSpawningActor(Summoned, FinalTransform);

		// 强制把召唤物的追击目标设为宿主当前的追击目标（就是玩家），
		// 不依赖它自己第一帧的 RefreshCombatTarget。
		if (HostTarget)
		{
			Summoned->CachedPlayer = HostTarget;
		}

		// Dist 用来判断“站着不动”是不是 ChaseStopRange 造成的正常停步
		const float DistanceToTarget = HostTarget
			? FVector::Dist2D(Summoned->GetActorLocation(), HostTarget->GetActorLocation())
			: -1.0f;

		UE_LOG(LogRiverOfInk, Log,
			TEXT("LanternGhost elite summon %s ready at Z=%.1f: Target=%s Dist=%.0f ChaseStop=%.0f ChaseContinue=%.0f AttackRange=%.0f ChaseSpeed=%.0f Detect=%.0f."),
			*Summoned->GetName(),
			FinalTransform.GetLocation().Z,
			*GetNameSafe(Summoned->GetCombatTarget()),
			DistanceToTarget,
			Summoned->ChaseStopRange,
			Summoned->ChaseContinueRange,
			Summoned->AttackRange,
			Summoned->ChaseSpeed,
			Summoned->DetectRange);

		SummonedEnemies.Add(Summoned);
		++SpawnedCount;
	}

	const int32 RequestedCount = PendingSummonTransforms.Num();
	PendingSummonTransforms.Reset();

	UE_LOG(LogRiverOfInk, Log,
		TEXT("LanternGhost elite %s summoned %d/%d minions (Class=%s)."),
		*Enemy->GetName(),
		SpawnedCount,
		RequestedCount,
		*ClassToSpawn->GetName());
}

FTransform ULanternGhostState_Suicide_Elite::MakeNonBlockingSpawnTransform(
	const AEnemyBase* Summoned,
	const FTransform& InTransform) const
{
	FTransform Result = InTransform;

	const UCapsuleComponent* Capsule = Summoned
		? Summoned->FindComponentByClass<UCapsuleComponent>()
		: nullptr;
	UWorld* World = GetWorld();
	if (!Capsule || !World)
	{
		return Result;
	}

	const float Radius = FMath::Max(1.0f, Capsule->GetScaledCapsuleRadius());
	const float HalfHeight = FMath::Max(1.0f, Capsule->GetScaledCapsuleHalfHeight());
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Summoned);
	if (const AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	// 落点 Z 已经在贴地时按胶囊半高算过，这里只是兜底：
	// 只要实例胶囊与静态物还有重叠，就按 10cm 一级往上顶，直到不再重叠。
	FVector Location = Result.GetLocation();
	int32 LiftedSteps = 0;
	while (LiftedSteps < 20
		&& World->OverlapAnyTestByObjectType(
			Location, FQuat::Identity, ObjectParams, Shape, QueryParams))
	{
		Location.Z += 10.0f;
		++LiftedSteps;
	}

	Result.SetLocation(Location);

	UE_LOG(LogRiverOfInk, Log,
		TEXT("LanternGhost elite summon %s spawn pose: Location=%s CapsuleHalfHeight=%.1f UnstickSteps=%d."),
		*GetNameSafe(Summoned),
		*Location.ToCompactString(),
		HalfHeight,
		LiftedSteps);

	return Result;
}

bool ULanternGhostState_Suicide_Elite::IsSummonLocationClear(const FVector& Location) const
{
	if (!bCheckSummonClearance || SummonClearanceRadius <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams;
	if (const AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	// 抬高到 1.2 倍半径：贴地落点若只用 1 倍，检测球会与脚下地面相切而被判成重叠
	const FVector TestLocation = Location + FVector(0.0f, 0.0f, SummonClearanceRadius * 1.2f);

	return !World->OverlapAnyTestByObjectType(
		TestLocation,
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeSphere(SummonClearanceRadius),
		QueryParams);
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Enemy/EnemyBase/LanternGhost/State/LanternGhostState_Ranged.h"
#include "RiverOfInk.h"

#include "Common/AttackArea/AttackAreaBase_Bezier.h"
#include "Enemy/EnemyBase/EnemyBase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_Chase.h"
#include "Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"

ULanternGhostState_Ranged::ULanternGhostState_Ranged()
{
PrimaryComponentTick.bCanEverTick = true;
}

void ULanternGhostState_Ranged::OnExit_Implementation()
{
if (UWorld* World = GetWorld())
{
World->GetTimerManager().ClearTimer(BezierReturnHandle);
}

Super::OnExit_Implementation();
}

void ULanternGhostState_Ranged::ExecuteAttack()
{
AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
if (!Enemy || Enemy->bIsDead)
{
return;
}

if (!Enemy->HasValidCombatTarget())
{
ReturnToChase();
return;
}

// 只有 EnemyBase 上配置的 AttackAreaClass 是贝塞尔子类时才走双贝塞尔逻辑，
// 否则回退到基类的普通攻击生成逻辑。
if (!Enemy->AttackAreaClass
|| !Enemy->AttackAreaClass->IsChildOf(AAttackAreaBase_Bezier::StaticClass()))
{
Super::ExecuteAttack();
return;
}

APlayerCharacter* Target = Enemy->GetCombatTarget();
if (!IsValid(Target))
{
ReturnToChase();
return;
}

UWorld* World = GetWorld();
if (!World)
{
return;
}

const FVector SpawnLocation = Enemy->GetActorLocation();
const FVector TargetLocation = Target->GetActorLocation();
FVector ToTarget = TargetLocation - SpawnLocation;
ToTarget.Z = 0.0f;

FRotator SpawnRotation = ToTarget.IsNearlyZero()
? Enemy->GetActorRotation()
: ToTarget.Rotation();

const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
const TSubclassOf<AAttackAreaBase_Bezier> BezierClass(Enemy->AttackAreaClass);

const float Offsets[] = { -400.0f, 400.0f };
int32 SpawnedCount = 0;

for (float Offset : Offsets)
{
AAttackAreaBase_Bezier* BezierArea = World->SpawnActorDeferred<AAttackAreaBase_Bezier>(
BezierClass,
SpawnTransform,
Enemy,
nullptr,
ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

if (!BezierArea)
{
continue;
}

BezierArea->Initialize(Enemy->AttackAreaLifeTime, 0.0f, false, nullptr);
BezierArea->BezierP1PositionRate = 0.3f;
BezierArea->BezierP1Offset = Offset;
BezierArea->bDamageOpponentOnly = true;
BezierArea->bIsEnemyProjectile = true;
BezierArea->bDetectObstacle = Enemy->bAttackAreaDetectObstacle;

UGameplayStatics::FinishSpawningActor(BezierArea, SpawnTransform);
BezierArea->SetBezierTarget(TargetLocation);

++SpawnedCount;
}

UE_LOG(LogRiverOfInk, Log,
TEXT("LanternGhost %s bezier ranged attack spawned %d areas (rate=%.2f offsets=+/-%.0f)."),
*Enemy->GetName(),
SpawnedCount,
0.3f,
400.0f);

if (UWorld* CurrentWorld = GetWorld())
{
CurrentWorld->GetTimerManager().SetTimer(
BezierReturnHandle,
this,
&ULanternGhostState_Ranged::ReturnToChase,
Enemy->AttackRecoveryTime,
false);
}
}
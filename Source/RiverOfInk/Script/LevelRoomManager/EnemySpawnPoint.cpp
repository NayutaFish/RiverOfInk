// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelRoomManager/EnemySpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AEnemySpawnPoint::AEnemySpawnPoint()
{
PrimaryActorTick.bCanEverTick = true;

SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
RootComponent = SceneRoot;

Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
Arrow->SetupAttachment(SceneRoot);
Arrow->ArrowSize = 1.5f;

Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
Billboard->SetupAttachment(SceneRoot);
}

void AEnemySpawnPoint::BeginPlay()
{
Super::BeginPlay();

// 初始 Z 旋转随机，让每个出生点朝向不同。
const float RandomYaw = FMath::FRandRange(0.0f, 360.0f);
AddActorWorldRotation(FRotator(0.0f, RandomYaw, 0.0f));

SetupInkMaterial();
}

void AEnemySpawnPoint::SetupInkMaterial()
{
InkMesh = FindComponentByClass<UStaticMeshComponent>();
if (!InkMesh)
{
UE_LOG(LogTemp, Warning, TEXT("EnemySpawnPoint %s: no UStaticMeshComponent found for ink fade."), *GetName());
return;
}

FadeMaterialInstance = InkMesh->CreateAndSetMaterialInstanceDynamic(0);
if (!FadeMaterialInstance)
{
UE_LOG(LogTemp, Warning, TEXT("EnemySpawnPoint %s: failed to create dynamic material instance."), *GetName());
return;
}

CurrentFadeValue = 0.0f;
TargetFadeValue = 0.0f;
FadeMaterialInstance->SetScalarParameterValue(FadeParameterName, 0.0f);
}

void AEnemySpawnPoint::Tick(float DeltaTime)
{
Super::Tick(DeltaTime);

if (!FadeMaterialInstance)
{
return;
}

if (!FMath::IsNearlyEqual(CurrentFadeValue, TargetFadeValue, 0.001f))
{
CurrentFadeValue = FMath::FInterpConstantTo(
CurrentFadeValue,
TargetFadeValue,
DeltaTime,
FMath::Max(0.01f, FadeInterpSpeed));

CurrentFadeValue = FMath::Clamp(CurrentFadeValue, 0.0f, 1.0f);
FadeMaterialInstance->SetScalarParameterValue(FadeParameterName, CurrentFadeValue);
}
}

void AEnemySpawnPoint::AssignSpawnCount(int32 InTotalSpawnCount)
{
TotalSpawnCount = FMath::Max(0, InTotalSpawnCount);
SpawnedCount = 0;
CurrentFadeValue = 0.0f;
TargetFadeValue = 0.0f;

if (FadeMaterialInstance)
{
FadeMaterialInstance->SetScalarParameterValue(FadeParameterName, 0.0f);
}

UE_LOG(LogTemp, Log, TEXT("EnemySpawnPoint %s assigned spawn count %d."), *GetName(), TotalSpawnCount);
}

void AEnemySpawnPoint::NotifyEnemySpawned()
{
if (TotalSpawnCount <= 0)
{
return;
}

SpawnedCount = FMath::Min(TotalSpawnCount, SpawnedCount + 1);
const float FadeValue = TotalSpawnCount > 0
? static_cast<float>(SpawnedCount) / static_cast<float>(TotalSpawnCount)
: 0.0f;

// 只更新目标值，实际渐变在 Tick 中处理。
TargetFadeValue = FadeValue;

UE_LOG(LogTemp, Log, TEXT("EnemySpawnPoint %s ink fade target: %.2f (%d/%d)."),
*GetName(), FadeValue, SpawnedCount, TotalSpawnCount);
}

float AEnemySpawnPoint::GetFadeValue() const
{
return CurrentFadeValue;
}

FTransform AEnemySpawnPoint::GetSpawnTransform() const
{
FTransform Result = GetActorTransform();

if (SpawnRadius > KINDA_SMALL_NUMBER)
{
// 在以本点为圆心的水平圆内取随机位置（XY 平面）
const float Radius = SpawnRadius * FMath::Sqrt(FMath::FRand());
const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
Result.SetLocation(Result.GetLocation() + Offset);
}

return Result;
}
# UE C++ 学习复盘｜玩家攻击与投射物解耦

日期：2026-08-13
工程：`RiverOfInk`；目标分支：`feature/rab`

## 今日完成

- 完成玩家普通攻击 Slice 3–4 的 C++ 接入与状态衔接。
- 将 AttackArea1 保持为玩家中心扇形近战判定。
- 定位并修复右键火球跟随玩家、随后停留在生成点的问题。
- 将右键 Attack2 改为沿玩家朝向移动的球形弹幕。
- 让 Hitbox 与 Niagara VFX 由同一个移动 Actor 驱动。
- 暴露右键弹幕的生命周期、速度、生成偏移和球形 Hitbox 半径。
- 完成冷编译与 PIE 启动验证。

## 问题链路

### 1. 复用玩家跟随 Actor 导致火球跟随玩家

Attack2 原本同时承担两件互相冲突的职责：

1. 作为玩家中心的攻击范围，跟随玩家位置和朝向。
2. 作为远程火球的视觉载体，应该沿发射方向离开玩家。

同一个 Actor 只能稳定满足其中一种空间关系，因此火球会随着玩家移动。

### 2. 只调用 `SpawnSystemAtLocation` 导致火球停在原地

将 Niagara 复制为独立世界特效后，确实切断了它与玩家的父子关系，但：

```cpp
UNiagaraFunctionLibrary::SpawnSystemAtLocation(...);
```

只负责在指定 Transform 创建特效，不会自动提供投射方向、速度或移动载体。于是特效脱离玩家后停在生成位置。

## C++ / UE5 关键学习

### Actor 移动与组件附着

本次采用“一个移动 Actor 同时承载 Hitbox 与 VFX”的方案：

- `AAttackArea_PlayerAttack2` 负责生命周期、伤害和移动。
- 根组件 `USphereComponent` 负责球形碰撞范围。
- 蓝图中的 Niagara 组件继续附着在 AttackArea 上，因此会随着 Actor 一起移动。
- `AAttackAreaBase::Tick` 使用 `AddActorWorldOffset(GetActorForwardVector() * Speed * DeltaTime)` 推进投射物。
- 远程投射物不设置 `FollowTarget`，避免移动逻辑和玩家跟随逻辑互相覆盖。

### `SpawnActor` 与 `SpawnActorDeferred`

普通 `SpawnActor` 会在生成过程中触发 `BeginPlay`。如果生成后才设置速度、半径或 Hitbox 类型，`BeginPlay` 期间可能已经使用了蓝图旧默认值。

这次改用：

```cpp
SpawnActorDeferred<AAttackArea_PlayerAttack2>(...);
// 配置 Initialize、Radius、bUseFanHitbox 等运行时参数
UGameplayStatics::FinishSpawningActor(AttackArea, SpawnTransform);
```

结论：凡是必须在 `BeginPlay` 前生效的配置，都应使用 Deferred Spawn，或提供专门的初始化接口并确保执行时机明确。

### `UPROPERTY` 与运行时碰撞尺寸

将这些参数暴露给蓝图组件，便于后续手感调节：

| 参数 | 默认值 | 作用 |
| --- | ---: | --- |
| `ProjectileLifeTime` | `1.5s` | 弹幕存在时间 |
| `ProjectileSpeed` | `900` | 弹幕移动速度 |
| `ProjectileSpawnForwardOffset` | `120cm` | 生成点相对玩家的前置距离 |
| `ProjectileHitboxRadius` | `50cm` | 球形 Hitbox 半径 |

仅修改自定义变量 `Radius` 不一定会立即改变已经创建的碰撞组件，因此同时调用：

```cpp
AttackArea->CollisionSphere->SetSphereRadius(ProjectileHitboxRadius);
```

这是“配置变量”和“实际 UE 组件状态”同步的一个重要注意点。

## 验证记录

- `RiverOfInkEditor` 冷编译：`Result: Succeeded`。
- PIE 地图：`TestMap_0`。
- PIE 世界：`/Game/Level/UEDPIE_0_TestMap_0`。
- 启动日志未发现 `Error`、`Fatal` 或 `Unhandled`。
- 右键火球的最终距离、命中范围和观感需要在 PIE 中结合描线 Hitbox 手动确认。

## 今日结论

攻击范围、VFX 和移动职责必须明确分离：近战范围可以跟随玩家，远程投射物必须拥有独立的方向与移动生命周期。Niagara 负责表现，不应默认承担 Gameplay 投射物的移动职责；移动、碰撞和伤害应由可测试的 C++ Actor 统一驱动。

## 后续学习与开发

- 将投射物移动从 `Tick + AddActorWorldOffset` 评估为 `UProjectileMovementComponent`，比较扫掠、碰撞和速度调节差异。
- 在 PIE 中检查球形 Hitbox 描线与火球实际可视范围是否一致。
- 为 Attack2 增加独立的弹道 VFX 方向、命中 VFX 和障碍物命中特效参数。
- 将攻击参数调节流程整理为“C++ 默认值 → 蓝图覆盖 → PIE 手感审评”的固定回归流程。

#UE5 #UE_CPP #GameplayProgramming #Combat #Projectile #PIE
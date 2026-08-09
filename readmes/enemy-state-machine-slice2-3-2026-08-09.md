# 敌人状态机 Slice 2–3 更新说明

日期：2026-08-09
分支：`feature/rab`

## 1. 本次目标

本次在 ESM-0/1 生命周期修复的基础上，完成两项白盒敌人能力：

1. 远程敌人按距离带站位并执行投射物攻击。
2. 敌人目标丢失、死亡、掉落和延迟销毁进入明确的状态链路。

不涉及 Behavior Tree、复杂寻路、动画状态机或正式掉落物美术。

## 2. Slice 2：远程敌人站位

### 配置

`AEnemyBase` 新增两个可配置字段：

- `MinimumAttackRange`：远程敌人的内圈距离。
- `MaximumAttackRange`：远程敌人的外圈距离；为 `0` 时复用 `AttackRange`。

远程敌人使用以下组合识别：

```text
bAttackAreaIsMelee = false
AttackAreaSpeed > 0
```

### 运行规则

```text
Distance > MaximumAttackRange  → 向玩家靠近
Distance < MinimumAttackRange  → 背向玩家后退
Minimum ≤ Distance ≤ Maximum   → 停止移动，等待攻击冷却
```

近战敌人继续使用 `ChaseStopRange/ChaseContinueRange` 的滞回追击规则。

攻击状态进入时重新计算目标方向，并使用 `LockedRotation` 作为攻击区域的生成位置和旋转，避免零前摇时沿旧 ActorForward 发射。

远程攻击区域继续设置 `bIsEnemyProjectile=true`，供 E Null Ring 识别。

## 3. Slice 3：TargetLost 与 Dead

### TargetLost

`UEnemyState_TargetLost` 在运行时由 `AEnemyBase::BeginPlay()` 自动注册，不要求现有敌人蓝图添加新组件。

进入条件：

- 玩家 Pawn 不存在。
- 玩家已死亡。
- Chase/Attack/HitBack 完成回调时无法获得有效目标。

进入后：

1. 清理攻击计时器。
2. 每 0.25 秒刷新玩家引用。
3. 找回有效目标后切回 `Idle`，由 Idle 的感知流程重新进入 Chase。

### Dead

`Die()` 不再直接广播事件并立即销毁，而是：

```text
Die()
  → bIsDead = true
  → 关闭敌人碰撞
  → EnemyState_Dead
  → GenerateDropOnDead()
  → OnDead
  → OnEnemyDeath
  → FNonPlayerDiedEvent
  → 延迟销毁
```

默认 `DeathDestroyDelay = 0.5s`。

`bDeadHandled` 保证掉落和死亡事件只执行一次。`EndPlay()` 会清理攻击计时器和死亡销毁计时器。

## 4. Pure Ink 掉落边界

当前经济系统的 EnemyDrop 是逻辑掉落，不生成独立可拾取 Actor：

- `GenerateDropOnDead()` 输出掉落日志。
- `FNonPlayerDiedEvent` 由 `URoguelikeEconomySubsystem` 消费。
- 经济子系统判断当前是否为 Combat Room，再执行 Pure Ink 入账。
- 非 Combat Room 或 RunFlow 尚未进入有效房间时，事件会被安全跳过。

因此“掉落生成于 OnDead”已经成立，但可见 Pure Ink 拾取物应作为后续独立切片实现，避免与当前钱包事务重复结算。

## 5. 受影响文件

- `Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyBase.h/.cpp`
- `Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyState/EnemyState_Chase.cpp`
- `Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyState/EnemyState_Attack.cpp`
- `Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyState/EnemyState_HitBack.cpp`
- `Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyState/EnemyState_Idle.cpp`
- `Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyState/EnemyState_TargetLost.h/.cpp`
- `Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyState/EnemyState_Dead.h/.cpp`
- `readmes/bug-log.md`

## 6. PIE 与编译验证

### 远程敌人

PIE 中临时配置：

```text
bAttackAreaIsMelee = false
AttackAreaSpeed = 600
MinimumAttackRange = 500
MaximumAttackRange = 900
```

观察到：

```text
Enemy ... state: Idle -> Chase
Enemy ... state: Chase -> Attack
Enemy projectile tagged for Null Ring: ...
Enemy ... attack executed: Style=Ranged ...
```

将敌人放入内圈后，位置从约 `120` 向外移动至约 `367`，符合后退到最小距离的预期。

### Dead 与 TargetLost

通过 PIE 调试属性触发死亡，观察到：

```text
Enemy ... entered Dead state; destroy delay=0.50s.
Enemy drop generated on OnDead: ... Amount=1.
Enemy ... state: Attack -> EnemyState_Dead.
```

等待延迟结束后，敌人不再出现在场景中。

让玩家生命归零后观察到：

```text
Enemy ... entered TargetLost; waiting for a valid combat target.
Enemy ... state: Chase -> EnemyState_TargetLost.
```

### 构建

关闭 Editor 后执行 UE 5.8 官方 `Build.bat`：

```text
RiverOfInkEditor Win64 Development
Result: Succeeded
BUILD_EXIT=0
```

未出现 dotnet/JIT 弹窗或新的 managed exception。已有 `TakeDamage` C4263/C4264 警告未纳入本切片。

## 7. 后续接入提示

1. 创建远程敌人蓝图时，优先复用 `AEnemyBase` 的距离带配置，不在状态组件内复制距离常量。
2. 如果加入远程侧移，应在当前“靠近/后退/停留”基础上增加可选移动策略，不要引入 Behavior Tree。
3. 如果加入可见 Pure Ink 拾取物，应让拾取物消费或确认同一死亡事件，避免钱包直接入账与拾取入账同时生效。
4. 如果需要死亡动画，将动画播放放在 `Dead` 状态；延迟销毁计时器仍由 `AEnemyBase` 持有。

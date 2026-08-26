# 投掷型 Q 追踪适配 Slice 3–4

日期：2026-08-23  
分支：`codex/q-grenade-guidance-slice1-2`

## 1. Slice 3：跨高度弹道稳定化

Slice 1–2 已完成投掷型 Q 的 `TargetedArcLanding` 运行时分支：施法时快照当前唯一标记目标，三枚 Grenade 在飞行过程中只修正水平速度方向，不重新选目标。

Slice 3 修正了固定 `LaunchVelocityZ` 对不同地形高度不稳定的问题。每枚投掷物现在按以下数据计算初始垂直速度：

```text
GuidanceTargetZ = MarkedTarget.Z + LandingHeightOffset
T = BaseFuseTime + GuidanceExtraFuseTime
RequiredLaunchZ = (GuidanceTargetZ - SpawnZ - 0.5 * GravityZ * T²) / T
```

这样引信结束时的理论弹道终点与目标高度一致；水平部分仍由 `TargetedArcLanding` 在飞行过程中使用常量角速度修正。目标无效、标记过期或超出最大距离时，沿用现有逻辑清除追踪，投掷物继续按当前速度飞行。

新增/保留的蓝图参数：

| 参数 | 默认值 | 用途 |
| --- | ---: | --- |
| `ThrownGrenadeGuidanceTurnRate` | 300 deg/s | 水平转向速度 |
| `ThrownGrenadeGuidanceLaunchVelocityZ` | 600 | 高度求解失败时的兜底值 |
| `ThrownGrenadeGuidanceLandingHeightOffset` | 0 cm | 目标落点高度偏移 |
| `ThrownGrenadeGuidanceExtraFuseTime` | 0.35 s | 追踪弹道的额外飞行时间 |
| `ThrownGrenadeGuidanceSpread` | 80 cm | 三枚弹幕相对标记目标的水平落点展开 |

## 2. Slice 4：可验收运行时观测

投掷型 Q 的关键日志现在包含：

- `Mode=TargetedArcLanding`；
- 目标 Actor；
- 有效引信时间；
- 出生高度与目标高度；
- 实际求解出的 `LaunchZ`；
- 首次水平转向日志 `guidance steering`；
- 爆炸命中数量与敌人受伤日志。

开发测试命令仅用于 PIE，不改变正式奖励流程：

```text
DebugApplySpecificRewards InkGrenade,ProjectileHoming
DebugMarkNearestHomingTarget 300
DebugMoveNearNearestHomingTarget 500
```

在 `TestMap_1` 的 PIE 起点可能与首个敌人相距超过追踪最大距离时，可先执行移动命令；该命令只用于开发验证，不改变正式战斗逻辑。移动后重新执行标记命令，再按 Q。

命令执行后按 Q。验收应同时满足：

1. `Skill spec resolved` 显示 `InkGrenade x1` 与 `ProjectileHoming x1`；
2. `guidance initialized` 显示 `TargetedArcLanding`、有效引信和高度求解值；
3. 至少出现一次 `guidance steering` 或目标已处于接受半径内；
4. 三枚投掷物均完成碰撞或引信爆炸；
5. 标记目标出现 `took 120 damage` 或对应当前伤害值；
6. 标记过期后再次释放 Q 不出现 `guidance initialized`，并继续保持普通投掷行为；
7. 目标死亡/标记转移后，投掷物不重新选择其他目标。

## 5. 2026-08-23 PIE 验收记录

- 冷编译：通过；`C:\Users\23586\AppData\Local\UnrealBuildTool\Log.txt` 为 `Result: Succeeded`，仅保留既有 `TakeDamage` 隐藏警告。
- 近距离静态目标：通过。`Distance=635.1`、`EffectiveFuse=1.25`、`Spawned=3/3`；三枚 Grenade 均完成爆炸，锁定目标 `BP_LanternGhost_Melee_C_2` 三次受到 `120` 伤害并死亡。
- 飞行中转向：通过。开启 `LogSkill Verbose` 后，三枚 Grenade 均记录 `guidance steering`，首帧水平修正为 `DeltaYaw=-5.0`。
- 远距离起点：发现测试起点与最近敌人约 `5259.8`，超过默认 `GuidanceMaxDistance=2500`，按设计清除追踪；新增 `DebugMoveNearNearestHomingTarget 500` 仅用于把 PIE 玩家移动到可重复验收距离。
- 初始地面穿透：已修复。世界碰撞忽略投掷物出生时的 `bStartPenetrating`，后续真实地面碰撞仍会引爆。
- 第二轮移动目标未命中属于移动目标在引信时离开范围的现象，不改变“锁定单一目标、不重新选目标”的规则。

## 3. 数据流

```text
右键命中
  -> ProjectileTargetingComponent 保存唯一目标 + EffectHandle
  -> Q ResolveSkillSpec()
  -> CastThrownGrenade() 快照目标、标记句柄与目标高度
  -> 求解垂直弹道 + 写入三枚 FProjectileSpec
  -> APlayerSkill_ThrownGrenade::Tick()
       -> 校验标记仍有效
       -> UpdateTargetedArcGuidance() 修正水平速度
       -> SweepForImpact() / Fuse
       -> Detonate() / 范围伤害
```

## 4. 非目标

- 不为投掷型 Q 新增第二套标记系统；
- 不在飞行中切换到新目标；
- 不把投掷物传送到目标位置；
- 不改变普通三发 Q 的 `SoftProjectileHoming` 路径；
- 不修改正式奖励卡生成与应用链路。

# Slice 1：Q 投掷型技能骨架

日期：2026-08-08
工作区：`E:\project\UE\demo0803`

## 1. 本切片范围

切片 0 的统一 `Damage / Defense` 公式已通过 PIE 后，本切片实现 Q 的第一种技能形态：短弧投掷型技能（`Q_ThrownGrenade`）。

本切片只改变 Q 的攻击几何和命中时机，不增加新的伤害类型，也不实现 E 的行为奖励、奖励池或正式美术。

## 2. 运行时结构

```text
APlayerCharacter
└── USkillComponent
    ├── SkillSlots[0] = TripleProjectile + ThrownGrenade
    ├── SkillSlots[1] = CircularSlash + Default
    └── CastTripleProjectile()
        ├── SkillForm == ThrownGrenade → APlayerSkill_ThrownGrenade
        └── SkillForm == Default       → 旧 AAttackAreaBase 散射路径
```

新增 `EPlayerSkillForm` 并写入 `FPlayerSkillSlot`。`FPlayerRuntimeData` 已经整体保存 `SkillSlots`，因此技能形态会随 GameInstance 的 RuntimeData 一起跨关卡保留。

新建的一局默认将 Q 设置为 `ThrownGrenade`；旧快照没有该字段时会反序列化为 `Default`，继续使用原三发散射路径。

## 3. 投掷物生命周期

```text
按 Q
→ SkillComponent 检查冷却与玩家状态
→ SpawnActorDeferred<APlayerSkill_ThrownGrenade>
→ 设置初速度、重力、引信、爆炸半径和伤害
→ 每帧扫掠 WorldStatic / EnemyHitbox
→ 命中或引信结束后 Detonate()
→ 范围查询 EnemyHitbox
→ AEnemyBase::TakeDamage(FTakeDamageInfo)
→ CombatDamageCalculator 统一计算最终伤害
```

投掷物使用 `/Engine/BasicShapes/Sphere` 作为蓝色占位球，默认参数为：

| 参数 | 默认值 |
| --- | ---: |
| 速度 | 850 cm/s |
| 重力 Z | -980 cm/s² |
| 引信 | 0.9 s |
| 爆炸半径 | 220 cm |
| 伤害 | 120 |

碰撞通道沿用项目现有约定：`ECC_GameTraceChannel1` 为 DamageArea，`ECC_GameTraceChannel2` 为 EnemyHitbox。

## 4. 兼容与后续扩展

- `TripleProjectile` 仍然是技能 ID；`SkillForm` 只负责替换攻击实现，避免现有升级和 HUD 依赖立刻重写。
- 当前 `TripleProjectile` 的数量升级只对旧散射路径生效，投掷型技能的范围、引信、分裂等专属升级留到后续奖励切片定义。
- 旧的 `AAttackAreaBase` 散射路径保留，便于旧存档和调试回退。
- 下一步可以在奖励选项中加入“Q 形态：投掷型”或“爆炸半径/引信/分裂”奖励；奖励应用只修改 `FPlayerSkillSlot` 或技能参数，不直接写入 Pawn 临时对象。

## 5. 验证记录

### 冷编译

UE 5.8 `RiverOfInkEditor Win64 Development`：`Result: Succeeded`。

仍存在的 C4263/C4264 是玩家和敌人的项目旧接口隐藏 UE 基类 `TakeDamage` 签名的已有警告，与本切片无关。

### PIE

自动 PIE 从 `TestMap_0` 调用 `StartNewRun`，切换至 `TestMap_1` 后调用 Q，日志包含：

```text
Run state changed: 2 -> 3
ThrownGrenade launched: Fuse=0.90 Radius=220 Damage=120.0 Velocity=X=850.000 ...
TripleProjectile thrown grenade cast: Fuse=0.90 Radius=220 Damage=120.0
ThrownGrenade detonated: Radius=220 Damage=120.0 Hits=0
PIE thrown grenade coverage complete
```

编辑器正常退出，没有 Python 错误、断言或 Fatal Error。

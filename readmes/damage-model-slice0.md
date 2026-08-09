# Slice 0：统一伤害模型与技能形态调整

日期：2026-08-08
工作区：`E:\project\UE\demo0803`

> **文档状态：部分历史。** 本文第 1–4 节的单一 `Damage / Defense` 伤害模型仍是当前实现；第 5 节及后续关于 Q 形态/奖励的内容属于旧版技能方案。新版技能请以 [新版技能系统 C++ 代码清单](skill-system-code-checklist-2026-08-09.md) 和 [以撒式技能构筑系统策划案](roguelike-build-design-draft-2026-08-08.md) 为准。

## 1. 目标

本切片完成两件事：

1. 将玩家和敌人的运行时伤害计算统一为单一 `Damage / Defense` 模型。
2. 保留旧版物理/魔法字段的读取兼容，避免旧 Blueprint 和运行时快照失效。

本切片只验证基础数值规则，不实现肉鸽形态技能。

## 2. 当前测试公式

公式由项目设置选择，默认模式为：

```text
FinalDamage = int((float)Damage - (float)Defense + 0.5)
FinalDamage = max(MinimumDamage, FinalDamage)
```

默认配置位于 `Config/DefaultGame.ini`：

```ini
[/Script/RiverOfInk.CombatDamageSettings]
Formula=SubtractDefenseRoundNearest
MinimumDamage=1
DefenseMultiplier=1.0
```

公式集中在：

```text
Source/RiverOfInk/Script/Core/CombatDamageCalculator.h
Source/RiverOfInk/Script/Core/CombatDamageCalculator.cpp
Source/RiverOfInk/Script/Core/CombatDamageSettings.h
```

`HealthComponent` 和 `EnemyBase` 不再各自维护物理/魔法计算分支，只调用：

```cpp
RiverOfInkDamage::CalculateFinalDamage(Damage, Defense)
```

以后更换测试公式时只修改计算器和项目设置，不修改所有伤害来源。

## 3. 旧版本兼容策略

### 3.1 伤害类型

`FTakeDamageInfo::DamageType` 和 `EDamageType` 暂时保留，原因是旧 Blueprint、攻击区域和事件结构仍可能引用它们。

当前运行时不再根据 `Physical`、`Magic`、`TrueDamage` 或 `Must` 分支计算。它们只是兼容字段和未来表现层标签。

新增 `Unified` 枚举值，新的 C++ 默认值使用它。

### 3.2 防御字段

新增单一字段：

```cpp
int32 Defense;
```

旧字段继续保留但标记为 Deprecated：

```cpp
int32 PhysicalResistance;
int32 MagicResistance;
```

迁移时使用：

```text
Defense > 0
→ 使用 Defense

Defense == 0 且旧字段存在值
→ 使用 max(PhysicalResistance, MagicResistance)
```

不会把两个旧抗性相加，避免迁移时出现意外的双重减伤。归一化后，旧字段会同步为 `Defense`，保证旧读取者仍能获得一致值。

### 3.3 跨场景运行时数据

`FPlayerRuntimeStats` 新增 `Defense`。`CaptureRuntimeData` 和 `ApplyRuntimeData` 都优先使用它；旧快照没有 `Defense` 时会从旧双抗字段迁移。

新的 RuntimeData 日志使用：

```text
HP=... Defense=... WalkSpeed=... SprintSpeed=...
```

## 4. 代码责任

```text
FTakeDamageInfo
  → 携带伤害值和兼容元数据

HealthComponent / EnemyBase
  → 持有 CurrentHealth、MaxHealth、Defense
  → 调用统一计算器

CombatDamageCalculator
  → 读取 CombatDamageSettings
  → 计算并返回最小值受限的整数伤害

RoguelikeRuntimeDataSubsystem
  → 保存 PlayerRuntimeStats.Defense
  → 兼容旧快照
```

伤害来源不应直接修改目标生命值，也不应重新实现减伤公式。

## 5. 后续 Q 形态奖励调整

策划案中的 `Q Beam` 改为投掷型技能，不做直线持续光束。

建议形态名称：`Q_ThrownGrenade` / `Ink Grenade`。

目标流程：

```text
按 Q
→ 向前方生成投掷物
→ 以初速度和重力形成抛物线
→ 命中敌人或到达引信时间
→ 生成范围爆炸伤害
→ 使用统一 Damage / Defense 公式
```

推荐使用独立技能 Actor，而不是把 `AAttackAreaBase` 强行改成多用途状态机：

```text
APlayerSkill_ThrownGrenade
├── 投掷移动
├── 引信/命中结束条件
├── 爆炸范围
└── 一次性范围伤害
```

第一版投掷型技能只改变攻击几何和命中时机，不引入新的伤害类型。后续可增加：

- 爆炸范围提升。
- 引信时间缩短。
- 投掷物分裂。
- 爆炸后留下短暂区域。

这些奖励属于技能形态/行为奖励，不应直接写入人物属性抗性系统。

## 6. 编译验证

关闭编辑器后使用 UE 5.8 冷编译：

```text
RiverOfInkEditor Win64 Development
Result: Succeeded
```

仍存在的 C4263/C4264 是 `APlayerCharacter::TakeDamage` 和 `AEnemyBase::TakeDamage` 隐藏 UE 基类签名的已有警告。本切片没有改变这两个项目接口，后续应单独重命名或增加明确的覆盖意图。

PIE 前移除了 `DefaultGame.ini` 中对不存在的 `GameFeatureData` 资产类型扫描配置。UE 5.8 在该项目中会因此在编辑器启动阶段触发 `FillRuntimeData` 崩溃；当前项目没有使用 GameFeatures 插件或资产，这项清理不会改变运行时玩法。

## 7. PIE 验收重点

```text
Defense=0, Damage=10.4 → FinalDamage=10
Defense=3, Damage=10.4 → FinalDamage=7
Defense >= Damage → FinalDamage=1
Physical/Magic/TrueDamage 标签 → 结果相同
旧双抗快照 → 正确迁移为单一 Defense
```

切片 0 已通过 PIE，Q 投掷型技能骨架已在 [damage-model-slice1.md](damage-model-slice1.md) 中实现；E 行为型奖励仍留待后续切片。

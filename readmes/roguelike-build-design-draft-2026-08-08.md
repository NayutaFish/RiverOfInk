# 以撒式技能构筑系统策划案（讨论稿）

> 状态：组内讨论通过；Slice 1–6 已实现，Slice 7 已建立完整 PIE 矩阵；本次已完成冷编译与 TestMap_0 启动验证，奖励选择链路仍待进入 Combat Room 后验收
>
> 工程：`E:\project\UE\demo0803` / `RiverOfInk`
>
> 目标：统一 Q/E 技能的局内构筑规则，再拆分为可独立编译和 PIE 验证的实现切片。
>
> 代码对接入口：请先阅读 [新版技能系统 C++ 代码清单](skill-system-code-checklist-2026-08-09.md)，再使用本文的规则和验收矩阵。

## 1. 文档定位

本方案将当前“技能形态互斥切换”调整为“基础技能 + 局内 Modifier 构筑”。

玩家始终拥有固定的两个技能槽：

```text
Q = Triple Projectile
E = Circular Slash
```

肉鸽奖励不新增第三个技能，也不把奖励实现为独立技能 Actor。奖励只修改 Q/E 的构筑数据，技能施放时根据当前构筑解析出一次性执行参数。

本稿记录规则、数据流和切片验收结果；C++ 已按 Slice 1–5 接入，蓝图控件仍沿用现有低保真奖励卡。

## 2. 当前实现与问题

旧版 `FPlayerSkillSlot` 使用 `EPlayerSkillForm` 表示技能形态，形态之间是互斥的。例如：

```text
SkillID = TripleProjectile
SkillForm = ThrownGrenade
```

旧版 `demo0803` 中，Q 初始槽位曾经是 `ThrownGrenade`。`CastTripleProjectile()` 检测到该形态后会直接进入 `CastThrownGrenade()`，不会继续使用普通三发投射物的数量计算；Slice 1–3 已将默认槽位恢复为普通 Q，并把 Grenade 变为 `InkGrenade` Modifier。

因此目前存在一个实际问题：

```text
Projectile Barrage / Add Projectile
```

只对普通散射投射物路径生效，默认 Grenade 路径不会使用 `GetTripleProjectileCount()`。这与以撒式“所有奖励都作用于同一个技能”的目标不一致。

另外，当前配置中 `TripleProjectileSpreadAngle` 为 15°，但普通发射实现使用固定 12° 步进，后续 Resolver 应统一这两个来源。

## 3. 核心概念

### 3.1 Base Skill

基础技能是固定的技能身份，不因奖励改变：

| 输入 | SkillID | 基础职责 |
| --- | --- | --- |
| Q | `TripleProjectile` | 生成若干个 Q Payload |
| E | `CircularSlash` | 在玩家附近生成圆形攻击区域 |

### 3.2 Skill Modifier

Modifier 是奖励带来的局内效果，可以叠加、设置上限或属于互斥组。

建议的第一版分类：

| 分类 | 示例 | 默认规则 |
| --- | --- | --- |
| Count | `AddProjectile` | 可叠加 |
| Payload | `InkGrenade` | 同一 Payload 组互斥 |
| Event | `ExtraExplosion`、`TwinSlash` | 可叠加或设置上限 |
| Utility | `CooldownReduction` | 可叠加，有最低值 |
| Area | `RadiusUp`、`RangeUp` | 可叠加，有最大值 |

Modifier 不直接生成 Actor，也不直接改变 HUD。它只改变运行时构筑数据。

### 3.3 Resolved Skill Spec

技能施放前，将基础技能和 Modifier 解析为不可变的本次施放参数：

```text
ResolvedQSpec
  ProjectileCount
  PayloadType
  ProjectileSpeed
  FuseTime
  ExplosionRadius
  ExplosionCount
  Cooldown

ResolvedESpec
  HitCount
  Radius
  Damage
  SecondHitDelay
  SecondHitAngle
  NullifyEnemyProjectiles
  Cooldown
```

Resolver 只读取数据，不修改构筑。实际 Grenade、Projectile、Circle Slash Actor 只负责执行解析结果。

## 4. 推荐数据结构

建议将 Modifier 状态放在技能槽中，使其随现有 `FPlayerRuntimeData.SkillSlots` 一起跨场景保存：

```text
FPlayerSkillSlot
  SkillID
  SkillLevel
  Modifiers[]

FSkillModifierState
  ModifierID
  StackCount
```

相比继续扩大 `EPlayerSkillForm`，该结构可以表达：

```text
TripleProjectile
+ AddProjectile x2
+ InkGrenade
+ ExtraExplosion
```

而不是只能保存一个最终形态名称。

旧快照迁移建议：

```text
ThrownGrenade → InkGrenade
NullRing      → NullRing Modifier
TwinSlash     → TwinSlash Modifier
Default       → 无形态 Modifier
```

## 5. Q 技能构筑规则

### 5.1 基础 Q

推荐将新系统的默认 Q 恢复为普通 `TripleProjectile`，让 `InkGrenade` 奖励具有明确价值。

当前普通 Q 参数仅作为第一版白盒基准，最终数值由组内确认：

| 参数 | 当前基准 |
| --- | ---: |
| 基础数量 | 3 |
| Mechanic 增长 | 每级 +2，最多 7 |
| 基础冷却 | 4.0 秒 |
| 冷却下限 | 2.0 秒 |
| 速度 | 900 cm/s |
| 生命周期 | 2.5 秒 |
| 前向生成偏移 | 120 cm |
| 侧向偏移 | 35 cm |

### 5.2 Ink Grenade

`InkGrenade` 是 Payload 转换 Modifier，不是第三个技能，也不是独立的奖励技能。

```text
TripleProjectile
  Payload = NormalProjectile

+ InkGrenade
  Payload = Grenade
```

当前 Grenade 白盒基准：

| 参数 | 当前基准 |
| --- | ---: |
| 投掷速度 | 850 cm/s |
| 重力 Z | -980 |
| 引信 | 0.9 秒 |
| 爆炸半径 | 220 cm |
| 爆炸伤害 | 120 |
| 碰撞半径 | 32 cm |
| 生成偏移 | 前方 120 cm，Z +60 cm |

### 5.3 Add Projectile

`AddProjectile` 只修改 `ProjectileCount`，不关心 Payload 类型：

```text
Base Q                         = 3 normal projectiles
Add Projectile x1              = 4 normal projectiles
Ink Grenade                    = 3 grenades
Add Projectile x1 + InkGrenade = 4 grenades
```

这保证奖励顺序不影响最终结果：先拿 Grenade 再拿 Add Projectile，与反过来拿到的构筑相同。

### 5.4 Extra Explosion

如果目标是“同一个特效球额外爆炸一次”，应使用独立的 `ExtraExplosion`，不要复用 `AddProjectile`：

```text
Ink Grenade + Extra Explosion
  = 每个 Grenade 产生两次爆炸
```

第一版建议额外爆炸复用第一次爆炸位置，可选短延迟；不要生成第二个完整投掷 Actor，以免同时引入第二条轨迹和第二次碰撞逻辑。

## 6. E 技能构筑规则

### 6.1 基础 E

基础 E 是一次 `CircularSlash`：

```text
生成圆形区域
命中范围内敌人一次
区域短暂存在
共享 E 冷却
```

### 6.2 Twin Slash

`TwinSlash` 是命中事件 Modifier：

```text
第一次 Circular Slash
等待短延迟
第二次 Circular Slash
第二段拥有独立伤害倍率和角度偏移
```

冷却仍然只计算一次，不能因为第二段重新开始 E 冷却。

### 6.3 Null Ring

`NullRing` 是攻击区域能力 Modifier：

```text
Circular Slash 的范围查询额外检测敌方投射物
符合条件的敌方投射物被销毁或禁用
```

推荐组合规则：`NullRing + TwinSlash` 可以共存，且两段攻击都拥有消弹能力。这样 Modifier 的组合具有直观的叠加效果：

```text
E = 两次斩击 + 两次消弹窗口
```

如果后续测试证明强度过高，再通过 Modifier 上限或“仅第一次消弹”的数据配置限制，而不是重新拆分技能分支。

## 7. Resolver 应用顺序

所有技能使用固定的解析顺序，避免奖励选择顺序造成结果差异：

```text
基础技能参数
  ↓
Payload / Geometry Modifier
  ↓
数量与多段攻击 Modifier
  ↓
额外事件 Modifier
  ↓
冷却、半径、速度、伤害等数值 Modifier
  ↓
最小值、最大值和合法性修正
```

示例：

```text
Base Q
+ AddProjectile x1
+ InkGrenade
+ ExtraExplosion

→ ProjectileCount = 4
→ Payload = Grenade
→ ExplosionCount = 2
```

推荐使用统一的：

```text
ResolveSkillSpec(SkillID, BuildState)
```

而不是在 `CastTripleProjectile()` 和 `CastCircularSlash()` 中为每个奖励增加独立 `if/else` 分支。否则技能形态和奖励数量增长后，会出现组合分支爆炸。

## 8. 奖励系统规则

奖励选项应从“切换形态”调整为“增加 Modifier”：

```text
FRoguelikeRewardOption
  ModifierID
  StackDelta
  Title
  Description
  BeforeValue
  AfterValue
```

`RoguelikeRewardManager` 的职责：

1. 根据当前构筑生成合法奖励。
2. 检查 Modifier 前置条件和上限。
3. 选择后调用 `SkillComponent.ApplyModifier()`。
4. 广播奖励已应用事件。

它不负责生成投射物，也不直接写入 Grenade 或 Slash Actor。

第一版奖励候选示例：

| 奖励 | 作用目标 | 叠加规则 |
| --- | --- | --- |
| Add Projectile | Q 数量 +1 | 最多 3 层 |
| Ink Grenade | Q Payload 转换 | Payload 组唯一 |
| Extra Explosion | Q 每个 Grenade +1 爆炸 | 最多 1 层 |
| Twin Slash | E 增加第二段 | 最多 1 层 |
| Null Ring | E 获得消弹 | 最多 1 层 |
| Radius Up | E 半径增加 | 最多 3 层 |
| Cooldown Down | Q/E 冷却减少 | 受最低值限制 |

## 9. 数据流与 UI

```text
RewardManager
  → SkillComponent.ApplyModifier()
  → FPlayerRuntimeData / RuntimeDataSubsystem
  → OnSkillStateChanged
  → Skill HUD 更新

Q/E 输入
  → SkillComponent.ResolveSkillSpec()
  → 生成 Projectile / Grenade / Circle Slash Actor
```

HUD 不读取未解析的内部枚举来猜测效果，而是展示解析后的构筑摘要：

```text
Q Triple Projectile
Form: Ink Grenade
Build: +1 Projectile · Extra Explosion
Effect: 4 grenades, each detonates twice
```

奖励 UI 只展示本次奖励带来的变化：

```text
Triple Projectile
Current: Ink Grenade
Reward: +1 Projectile
Result: 3 → 4 grenades
```

## 10. 实施切片建议

### Slice 0：Modifier 契约

新增 Modifier 枚举、状态结构和 Resolver 输出结构；不改变实际施放行为。

验收：冷编译通过，旧技能仍按旧路径施放。

### Slice 1：RuntimeData 保存与迁移

让 Modifier 随 `FPlayerRuntimeData.SkillSlots` Capture/Apply，补充旧 `SkillForm` 快照迁移。

验收：PIE 切换地图后 Modifier 层数和 Payload 状态不丢失。

### Slice 2：通用 Resolver

实现 Q/E 的基础参数解析和日志，不立即改变 Actor 生成路径。

验收：相同 Modifier 集合无论添加顺序如何，解析日志一致。

### Slice 3：Q Modifier

统一普通投射物和 Grenade 的数量逻辑，实现 `AddProjectile`、`InkGrenade`，再实现 `ExtraExplosion`。

验收：3 发、4 发、3/4 个 Grenade、单/双爆炸分别可验证。

### Slice 4：E Modifier

将 `TwinSlash`、`NullRing` 接入 Resolver，并验证两者组合。

验收：二段攻击、消弹、组合效果和单次冷却均符合规则。

### Slice 5：奖励池迁移

移除奖励流程对 `ChangeSkillForm` 的依赖，奖励改为发放 Modifier；加入前置条件和上限。

验收：未拥有 Modifier 时可生成，达到上限后不再生成，选择后正确写回 RuntimeData。

### Slice 6：HUD 与奖励 UI

展示当前解析后的技能构筑、奖励前后差异和 Modifier 层数。

验收：Q/E HUD 与实际施放效果同步，奖励卡不再只显示模糊的“技能升级”。

### Slice 7：完整 PIE 矩阵

验证 Q/E 单 Modifier、组合 Modifier、跨场景保存、奖励上限、冷却和重复命中。

验收：日志中能够看到 `SkillID`、Modifier 列表、解析结果和最终 Actor 行为。

## 11. 组内需要先确认的决策

建议优先确认以下四项：

1. 默认 Q 是否从当前 `ThrownGrenade` 改回普通 `TripleProjectile`。本方案推荐改回普通 Q，让 `InkGrenade` 奖励具有实际意义。
2. `Add Projectile` 每次奖励增加 1 发还是沿用当前每级增加 2 发。本方案推荐奖励单次 +1，避免早期强度跳跃。
3. `NullRing + TwinSlash` 是否允许共存。本方案推荐允许共存，两段都具备消弹能力。
4. `ExtraExplosion` 的作用范围是每个 Grenade，还是整次 Q 施放只额外产生一次爆炸。本方案推荐按每个 Grenade 计算，规则更直观，但需要设置上限。

## 12. 非目标与风险

本阶段不引入：

- GAS 或复杂技能框架。
- 独立的技能蓝图继承树。
- 局外成长和永久存档奖励。
- 随机修改奖励顺序的复杂组合规则。
- 需要多关卡加载才能成立的技能逻辑。

主要风险：

- Modifier 组合过多导致数值爆炸，需要每类 Modifier 设置上限。
- 旧 `SkillForm` 快照迁移不完整，导致跨场景后技能回退。
- Grenade 额外爆炸重复命中同一敌人，需要明确 `HitPolicy`。
- Resolver 与 HUD 各自计算文案，造成显示与实际效果不一致。

## 13. 讨论结论记录

| 决策项 | 结论 | 日期 | 备注 |
| --- | --- | --- | --- |
| 默认 Q | 普通 Triple Projectile | 2026-08-08 | `InkGrenade` 作为构筑 Modifier |
| Add Projectile 增量 | 单次 +1 | 2026-08-08 | 最多 3 层 |
| NullRing 与 TwinSlash | 可以共存 | 2026-08-08 | 两段 E 都可沿用消弹能力 |
| ExtraExplosion 作用范围 | 每个 Grenade | 2026-08-08 | 复用同一投掷物位置，最多 1 层 |
| 开始实施 | Slice 1–7 | 2026-08-08 | Slice 6 已接入构筑摘要；Slice 7 已完成冷编译与 TestMap_0 启动验证，奖励选择链路待 Combat Room 验收 |

## 14. Slice 1–7 实施记录

### Slice 1：RuntimeData 保存与迁移

`FPlayerSkillSlot` 新增 `Modifiers` 数组，随现有 `FPlayerRuntimeData.SkillSlots`
一起由 `USkillComponent::CaptureRuntimeData / ApplyRuntimeData` 保存和恢复。

旧快照兼容规则：

- `ThrownGrenade` → `InkGrenade x1`；
- `NullRing` → `NullRing x1`；
- `TwinSlash` → `TwinSlash x1`；
- `Default` → 不添加 Modifier。

旧 `SkillForm` 字段暂时保留，Resolver 会优先读取 Modifier，并以旧字段作为回退。

### Slice 2：通用 Resolver

`USkillComponent::ResolveSkillSpec(SkillID)` 按固定顺序输出 `FResolvedSkillSpec`，
并在 `LogSkill` 中记录技能、构筑、数量、Payload、爆炸次数、命中次数、半径和冷却。

### Slice 3：Q Modifier

- `AddProjectile`：普通投射物和 Grenade 共用数量解析，单层 `+1`，最多 3 层；
- `InkGrenade`：将 Q Payload 转为投掷型 Grenade；
- `ExtraExplosion`：每个 Grenade 在同一位置追加一次爆炸，最多 1 层；
- Q 发射角度统一使用 `TripleProjectileSpreadAngle`，不再使用旧的固定 12°。

Slice 3 阶段先通过 `USkillComponent::ApplyModifier()` 接入运行时，
奖励池迁移在 Slice 5 完成，奖励卡展示在 Slice 5 中同步接入 Modifier 预览。

### Slice 4：E Modifier

- `ResolveSkillSpec(CircularSlash)` 读取 `TwinSlash`、`NullRing`，不再把两者视为互斥形态；
- `TwinSlash` 将 `HitCount` 解析为 2，延迟段复用同一份不可变 Spec，只改变角度、前向偏移和伤害倍率；
- `NullRing` 将 `bNullifyEnemyProjectiles` 解析为 true；当与 Twin Slash 共存时，首段和二段都会把该参数传给攻击区域；
- 两段攻击只在首次施放时写入一次 E 冷却，二段由计时器完成，不重复计冷却。

### Slice 5：奖励池迁移

- `ERoguelikeRewardType::Modifier` 携带 `ModifierID`、`StackDelta`、`BeforeValue`、`AfterValue`；
- `RoguelikeRewardManager::GenerateRewardOptions()` 只生成当前合法且未达上限的 Modifier；
- `ExtraExplosion` 依赖 `InkGrenade`，`TwinSlash` 与 `NullRing` 独立判定并允许共存；
- 选择后调用 `USkillComponent::ApplyModifier()`，日志同时输出层数和解析后的数量/命中/半径/冷却；
- 奖励卡类别、效果预览和占位图标按 Modifier 映射，旧的 `UpgradeSkill` / `ChangeSkillForm` 仅保留兼容应用路径，不再由新奖励池生成。

### Slice 6：HUD 与奖励 UI

技能 HUD 不再自行推导构筑效果。`USkillComponent` 提供两个只读展示接口：

- `GetSkillBuildSummary()`：按固定顺序显示 Payload 和 Modifier 层数；
- `GetResolvedSkillSummary()`：直接读取 `FResolvedSkillSpec`，显示投射物/爆炸/命中次数、半径和冷却。

`UPlayerSkillWidget` 通过 `OnSkillStateChanged` 事件刷新构筑摘要，冷却剩余时间仍由短周期计时器只更新进度条和倒计时文本。这样奖励选择后，`ApplyModifier()` 广播一次即可同步 Q/E 卡片；冷却倒计时不会反复触发构筑解析。

原有 WBP 技能栏没有新增摘要控件时，C++ 会把摘要追加到等级文本中，避免旧控件树出现“技能实际已升级但 HUD 没有反馈”。原生白盒控件树则为每张卡增加独立的 `BuildSummary` 文本区。

奖励 UI 沿用 Slice 5 的二选一卡片，但现在明确展示：技能名称、Current Form、奖励类别、效果前后值和 Modifier 层数。若 WBP 尚未建立四个详细文本绑定，C++ 会把同一份信息合并到 `Description`，因此旧版控件树仍可测试。

### Slice 7：完整 PIE 矩阵

Slice 7 采用“先构建、再按单项到组合项”的顺序，避免把 UI 显示正确误认为技能行为正确：

| 编号 | 场景 | 关键观察 |
| --- | --- | --- |
| 7.1 | 新局，Q/E 基础构筑 | Q=3 个普通投射物；E=1 次命中；HUD 显示 `Payload`、`Effect` 和冷却 |
| 7.2 | Q `InkGrenade` | Q 变为 Grenade，数量仍由同一 Resolver 管理 |
| 7.3 | Q `AddProjectile` | 逐层增加数量，达到上限后奖励池不再生成 |
| 7.4 | Q `ExtraExplosion` | 只有已拥有 Grenade 时出现；每个 Grenade 按解析次数爆炸 |
| 7.5 | E `TwinSlash` | 延迟二段命中、角度/前向偏移、单次冷却 |
| 7.6 | E `NullRing` | 施放时范围内敌方投射物被清除 |
| 7.7 | E `TwinSlash + NullRing` | 两段均带消弹参数，互不覆盖 |
| 7.8 | 奖励 UI | 两张卡显示 Current Form、类别、前后值、图标；选择后 HUD 同步 |
| 7.9 | 跨房间/跨场景 | `CaptureRuntimeData / ApplyRuntimeData` 后 Modifier 和 HUD 摘要不丢失 |
| 7.10 | 冷却/重复命中 | Q/E 冷却只写入一次；Twin Slash 不重复计冷却，同一命中策略可观察 |

当前验收状态：代码静态检查、冷编译和 PIE 基础启动已通过。编辑器使用本地可写的 DDC/Zen/Shader 工作目录启动，`StartPIE` 成功进入 `/Game/Level/UEDPIE_0_TestMap_0`，随后已正常停止 PIE。日志确认 Health HUD、Skill HUD、Q/E 图标、Q/E 构筑摘要和准备房间跳过刷怪均已初始化。奖励选择、Modifier 组合和跨房间保存仍需在 Combat Room 中按下表继续验证。

### PIE 验收矩阵

编辑器重新编译后，按以下顺序验证：

1. 新局进入 Combat Room：Q 默认生成 3 个普通投射物，日志显示 `Payload=NormalProjectile`；
2. 通过蓝图调用 `ApplyModifier(TripleProjectile, InkGrenade)`：Q 生成 3 个 Grenade；
3. 再调用 `ApplyModifier(TripleProjectile, AddProjectile)`：Q 生成 4 个 Grenade；
4. 再调用 `ApplyModifier(TripleProjectile, ExtraExplosion)`：每个 Grenade 日志出现 `explosion 1/2` 和 `explosion 2/2`；
5. 离开并重新进入房间：`Capture/Apply` 日志显示 Q Modifier 列表未丢失。
6. Room Clear 打开奖励 UI：候选卡均为 `RewardType=Modifier`；选择 `TwinSlash` 后再次生成候选仍可看到 `NullRing`，反向亦然；
7. 依次选择 `InkGrenade → ExtraExplosion`：第二张卡在未持有 Grenade 时不生成，持有后生成；达到各 Modifier 上限后不再生成对应卡。

Slice 1–6 的 Editor/PIE 验收仍需在关闭 Live Coding 后执行；Slice 7 已给出完整顺序。本次 PIE 仅覆盖准备房间启动和 HUD 初始化，不能替代奖励选择与技能行为矩阵。

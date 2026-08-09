# 新版技能系统 C++ 代码清单

日期：2026-08-09
工程：`E:\project\UE\demo0803` / `RiverOfInk`
适用分支：`feature/rab`

> **代码入口说明**：本文是新版“固定 Q/E + 局内 Modifier 构筑”的代码对接清单，优先级高于旧版技能形态说明。当前技能槽仍保留 `EPlayerSkillForm`，但它只负责旧快照兼容；新奖励应写入 `FSkillModifierState`，不应直接替换 `SkillForm`。

## 1. 当前架构结论

```text
Q = TripleProjectile
E = CircularSlash

RewardManager
  → SkillComponent::ApplyModifier()
  → FPlayerSkillSlot.Modifiers
  → SkillComponent::ResolveSkillSpec()
  → 生成本次施放的 Projectile / Grenade / Circle Slash
  → OnSkillStateChanged
  → Skill HUD 构筑摘要刷新

Reward HUD 只负责候选预览和选择；选择完成后关闭，不承担技能参数计算。
```

Modifier 只保存局内构筑状态，不直接生成 Actor，也不在 HUD 中重复计算技能参数。所有施放参数和展示摘要都必须从 `ResolveSkillSpec()` 或其派生接口读取。

## 2. C++ 文件清单

### 2.1 数据契约与解析核心

| 文件 | 责任 | 当前状态 |
| --- | --- | --- |
| `Source/RiverOfInk/Script/Player/Skill/PlayerSkillTypes.h` | 定义 `EPlayerSkillID`、`ESkillModifierID`、`ESkillPayloadType`、`FSkillModifierState`、`FResolvedSkillSpec`、`FPlayerSkillSlot`、`FRoguelikeRewardOption` | ✅ 新版权威数据契约；`SkillForm` 仅兼容 |
| `Source/RiverOfInk/Script/Player/Skill/SkillComponent.h` | 暴露技能施放、Modifier、Resolver、Capture/Apply、HUD 查询接口 | ✅ 新版核心接口 |
| `Source/RiverOfInk/Script/Player/Skill/SkillComponent.cpp` | 初始化 Q/E、校验上限/前置、应用 Modifier、迁移旧形态、解析参数、生成技能 Actor | ✅ 新版核心实现 |

`USkillComponent` 是技能运行时数据的唯一来源。关键接口：

```cpp
bool CanApplyModifier(EPlayerSkillID SkillID, ESkillModifierID ModifierID, int32 StackDelta = 1) const;
bool ApplyModifier(EPlayerSkillID SkillID, ESkillModifierID ModifierID, int32 StackDelta = 1);
FResolvedSkillSpec ResolveSkillSpec(EPlayerSkillID SkillID) const;
FText GetSkillBuildSummary(EPlayerSkillID SkillID) const;
FText GetResolvedSkillSummary(EPlayerSkillID SkillID) const;
void CaptureRuntimeData(FPlayerRuntimeData& OutRuntimeData) const;
void ApplyRuntimeData(const FPlayerRuntimeData& InRuntimeData);
```

### 2.2 输入与技能施放入口

| 文件 | 责任 | 当前状态 |
| --- | --- | --- |
| `Source/RiverOfInk/Script/Input/PlayerInputComponent.h` / `PlayerInputComponent.cpp` | 加载 Enhanced Input 资产，广播 `OnQDelegate` / `OnEDelegate` | ✅ 输入入口 |
| `Source/RiverOfInk/Script/Player/PlayerCharacter.h` / `PlayerCharacter.cpp` | 持有 `USkillComponent`，转发 Q/E 槽位施放，负责 RuntimeData 和 HUD 生命周期 | ✅ 角色装配层 |
| `Source/RiverOfInk/Script/Player/PlayerState/PlayerState_Idle.cpp` | 空闲状态接收 Q/E，并切入技能状态 | ✅ 状态入口 |
| `Source/RiverOfInk/Script/Player/PlayerState/PlayerState_Move.cpp` | 移动状态接收 Q/E，并切入技能状态 | ✅ 状态入口 |
| `Source/RiverOfInk/Script/Player/PlayerState/PlayerState_Skill1.cpp` | Q 技能状态，调用 `TryCastSkillSlot(0)`，处理短暂滑行动画 | ✅ Q 状态包装 |
| `Source/RiverOfInk/Script/Player/PlayerState/PlayerState_Skill2.cpp` | E 技能状态，调用 `TryCastSkillSlot(1)`，处理短暂滑行动画 | ✅ E 状态包装 |

输入实际调用链：

```text
Q/E InputAction (Started)
  → UPlayerInputComponent::OnQ / OnE
  → OnQDelegate / OnEDelegate
  → PlayerState_Idle 或 PlayerState_Move::OnQ / OnE
  → PlayerState_Skill1 / PlayerState_Skill2::OnEnter
  → APlayerCharacter::TryCastSkillSlot1 / 2
  → USkillComponent::TryCastSkillSlot(0 / 1)
```

### 2.3 技能 Actor 与伤害区域

| 文件 | 责任 | 当前状态 |
| --- | --- | --- |
| `Source/RiverOfInk/Script/Common/AttackAreaBase.h` / `AttackAreaBase.cpp` | 普通 Q 投射物移动、碰撞和伤害委托 | ✅ `NormalProjectile` Payload |
| `Source/RiverOfInk/Script/Player/Skill/PlayerSkill_ThrownGrenade.h` / `PlayerSkill_ThrownGrenade.cpp` | `InkGrenade` 的抛物线移动、地面/敌人命中、引信和多次爆炸 | ✅ Q Modifier 专用 Actor |
| `Source/RiverOfInk/Script/Player/Skill/PlayerSkill_CircleDamageArea.h` / `PlayerSkill_CircleDamageArea.cpp` | E 圆形伤害区域、消除标记敌方投射物、视觉平面 | ✅ E 基础区域 / `NullRing` |

`USkillComponent::ResolveSkillSpec()` 先输出不可变的本次施放参数，再由施放函数选择 Actor：

```text
TripleProjectile + NormalProjectile → AAttackAreaBase
TripleProjectile + InkGrenade       → APlayerSkill_ThrownGrenade
CircularSlash                       → APlayerSkill_CircleDamageArea
TwinSlash                           → 延迟再次生成 CircleDamageArea
```

### 2.4 奖励与 UI

| 文件 | 责任 | 当前状态 |
| --- | --- | --- |
| `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeRewardManager.h` / `RoguelikeRewardManager.cpp` | Room Clear 后生成候选、过滤前置/上限、应用奖励、关闭 UI、广播 `OnRewardApplied` | ✅ 新奖励池入口 |
| `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeRewardWidget.h` / `RoguelikeRewardWidget.cpp` | 将奖励写入两张卡片，显示技能、当前形态、类别、前后值、Modifier 层数并处理选择 | ✅ C++ 绑定 + Blueprint 兼容 |
| `Source/RiverOfInk/Script/UI/PlayerSkillWidget.h` / `PlayerSkillWidget.cpp` | 显示 Q/E 图标、等级、构筑摘要和冷却；订阅 `OnSkillStateChanged` | ✅ 事件驱动 HUD |

奖励调用链：

```text
Room Clear
  → ARoguelikeRewardManager::ShowRewardAfterRoomClear()
  → GenerateRewardOptions()
  → USkillComponent::CanApplyModifier()
  → URoguelikeRewardWidget::SetupRewardOptions()
  → Button → SelectOption(Index)
  → ARoguelikeRewardManager::SelectReward()
  → ApplyReward()
  → USkillComponent::ApplyModifier()
  → OnSkillStateChanged
  → UPlayerSkillWidget::RefreshSkills()
```

新奖励池使用 `ERoguelikeRewardType::Modifier`。旧的 `UpgradeSkill` / `ChangeSkillForm` 仅保留给旧 Blueprint、旧快照和迁移路径。

### 2.5 跨场景 RuntimeData

| 文件 | 责任 | 当前状态 |
| --- | --- | --- |
| `Source/RiverOfInk/Script/RoguelikeSystem/PlayerRuntimeData.h` | 持有 `SkillSlots`、`SkillUpgradeStates`、人物运行时属性和 Buff | ✅ 跨场景数据契约 |
| `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeRuntimeDataSubsystem.h` / `RoguelikeRuntimeDataSubsystem.cpp` | 由 `UGameInstanceSubsystem` 持有快照，负责 Capture/Register/Apply/Reset | ✅ 全局运行时快照 |
| `Source/RiverOfInk/Script/Player/PlayerCharacter.cpp` | Pawn 生成时 Apply，离开关卡前 Capture | ✅ 装配调用点 |

```text
APlayerCharacter::CaptureRuntimeData()
  → USkillComponent::CaptureRuntimeData()
  → FPlayerRuntimeData.SkillSlots
  → URoguelikeRuntimeDataSubsystem
  → 新 Pawn
  → APlayerCharacter::ApplyRuntimeData()
  → USkillComponent::ApplyRuntimeData()
  → MigrateLegacySkillForms / NormalizeSkillModifiers
```

## 3. 当前 Modifier 规则

| 技能 | Modifier | 效果 | 上限/前置 |
| --- | --- | --- | --- |
| Q | `AddProjectile` | 投射物/手雷数量 `+1` | 最大 3 层；基础数量 3 |
| Q | `InkGrenade` | 普通 Q Payload 转为投掷手雷 | 最大 1 层 |
| Q | `ExtraExplosion` | 每枚手雷额外爆炸一次 | 最大 1 层；必须先有 `InkGrenade` |
| Q | `CooldownDown` | 冷却 `-0.5s` | 最大 4 层；最低 2.0s |
| E | `TwinSlash` | 延迟二段攻击，二段伤害为 80% | 最大 1 层；与 `NullRing` 可共存 |
| E | `NullRing` | E 范围内消除标记敌方投射物 | 最大 1 层；与 `TwinSlash` 可共存 |
| E | `RadiusUp` | 半径 `+60` | 最大 3 层；最高 440 |
| E | `CooldownDown` | 冷却 `-0.4s` | 最大 3 层；最低 1.6s |

E 二段当前配置来自 `USkillComponent`：延迟 `0.18s`、Yaw 偏移 `35°`、前向偏移 `110`、伤害倍率 `0.8`。Resolver 将 `TwinSlash + NullRing` 合并成同一份 Spec，二段沿用消弹参数。

## 4. 旧版兼容边界

源代码中以下内容暂时不能删除：

- `EPlayerSkillForm` 和 `FPlayerSkillSlot::SkillForm`：旧 RuntimeData 快照兼容字段。
- `USkillComponent::CanApplySkillForm()` / `ApplySkillForm()`：旧 Blueprint/旧奖励数据的兼容入口。
- `USkillComponent::MigrateLegacySkillForms()`：映射 `ThrownGrenade → InkGrenade`、`TwinSlash → TwinSlash Modifier`、`NullRing → NullRing Modifier`。

新代码不要：

1. 在 `CastTripleProjectile()` 或 `CastCircularSlash()` 中新增奖励专用 `if/else` 分支。
2. 直接修改 `SkillSlots` 里的 Modifier 数组而绕过 `ApplyModifier()`。
3. 在 HUD 或 RewardWidget 中复制 Resolver 的数值计算。
4. 把 `InkGrenade`、`TwinSlash`、`NullRing` 当成第三技能槽。

## 5. 文档状态索引

| 文档 | 状态 | 使用方式 |
| --- | --- | --- |
| `readmes/roguelike-build-design-draft-2026-08-08.md` | ✅ 当前设计与实现说明 | 解释规则、切片和验收矩阵；本文补充实际代码入口 |
| `readmes/游戏链路更新说明_2026-08-06.md` | ✅ 当前运行链路补充 | 只看奖励、房间和 RunFlow 对接 |
| `readmes/bug-log.md` | ✅ 当前问题与验证记录 | 以最新日期条目为准，旧条目保留为历史证据 |
| `readmes/damage-model-slice0.md` | ⚠️ 部分历史 | 统一 `Damage / Defense` 章节仍有效；其中 Q 形态/奖励建议属于旧方案 |
| `readmes/damage-model-slice1.md` | ⚠️ 历史方案 | 记录旧的 `SkillForm = ThrownGrenade` 分支；不要按其默认 Q 或奖励流程接新代码 |

## 6. 验证清单

- [x] `PlayerSkillTypes.h` 的 Modifier、Resolver 和奖励字段已存在。
- [x] 默认槽位为 Q `TripleProjectile`、E `CircularSlash`，旧快照可迁移。
- [x] `ApplyModifier()` 统一处理上限和 `ExtraExplosion` 前置条件。
- [x] Q 普通投射物、Q `InkGrenade`、E `TwinSlash`、E `NullRing` 共享 `ResolveSkillSpec()`。
- [x] `CaptureRuntimeData / ApplyRuntimeData` 保留 Modifier 和升级状态。
- [x] HUD 通过 `OnSkillStateChanged` 更新构筑摘要，不复制技能计算。
- [x] 冷编译和准备房间 PIE 启动已有记录；构建过程未出现 dotnet 弹窗。
- [ ] 在 Combat Room 逐项完成奖励点击、Modifier 组合、达到上限后的候选过滤验证。
- [ ] 完成 `TwinSlash + NullRing` 两段攻击的重复命中与消弹行为矩阵。

队友接入新奖励时，优先检查本清单第 2、3、4 节，再修改 `RoguelikeRewardManager` 和 `SkillComponent`；不要以 `damage-model-slice1.md` 的旧 `SkillForm` 流程作为实现依据。

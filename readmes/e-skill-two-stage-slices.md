# E 技能两段近距离形态实施状态

## 范围

本分支以 `origin/main` 的 `deb7188` 为基线，实施 E 技能“两段弧斩”形态：

- 第一段、第二段各造成基础 E 伤害的 `0.8` 倍；
- 第一段命中任意敌人后解锁第二段，第二段可立即按 E 释放；
- 第一段未命中，或第二段释放后，E 进入正常冷却；
- `TwinSlash` 仍保留每段增加一次独立判定的效果，且每次判定伤害乘以 `0.65`；
- 两段形态的每次判定使用较小半径和水平弧形精筛，减少对群与远侧目标覆盖。

## 数据流

```text
RewardManager
  └─ DebugSelectSpecificReward TwoStageArc / 奖励池 ChangeSkillForm
       └─ USkillComponent::ApplySkillForm(CircularSlash, TwoStageArc)
            └─ SkillSlots[1].SkillForm 持久化到 FPlayerRuntimeData
                 └─ ResolveSkillSpec()
                      ├─ StageCount = 2
                      ├─ JudgmentsPerStage = 1 或 TwinSlash 时为 2
                      ├─ StageDamageMultiplier = 0.8
                      ├─ Radius = TwoStageArcRadius
                      └─ ArcHalfAngle = TwoStageArcHalfAngle

E 输入
  ├─ 首次释放：SpawnCircularSlashSet(Stage=0)
  │    └─ CircleDamageArea 球体粗筛 → 水平弧形点积精筛 → OnHitConfirmed
  │         ├─ 命中：bCircularSlashStage2Ready = true
  │         └─ 超时：写入 LastCastTimes，直接进入冷却
  └─ 第二次释放：SpawnCircularSlashSet(Stage=1)
       └─ 清除 stage2 ready，写入 LastCastTimes，进入冷却
```

## 功能切片状态

### Slice 1：形态与状态机

已完成并通过冷编译及 PIE 基础链路验证。奖励应用、首段生成、首段超时后进入冷却均已验证。

### Slice 2：弧形判定与 TwinSlash 兼容

已完成并通过冷编译及 PIE。PIE 调试范围显示为实际黄色弧形；日志确认默认参数为：

- `TwoStageArcRadius = 200 cm`；
- `TwoStageArcHalfAngle = 65°`；
- `TwoStageArcStageDamageMultiplier = 0.8`；
- `TwinSlash` 时每段 `JudgmentsPerStage = 2`，每个判定仍使用 `0.65` 伤害倍率。

### Slice 3：占位 VFX 与美术接口

已完成并通过冷编译及 PIE。E 两段复用左键 `NS_CommonSlash`；日志确认 Niagara 实例生成时使用 `(0,0,0,1)` 黑色接口、前向偏移 `60 cm`、缩放 `1.0`。

### Slice 4：输入态机与完整二段链路验收

已实现：

- `CanTriggerCircularSlashInput()` 统一 E 输入门控；首段命中窗口内重复按 E 不再重新进入技能状态或重复播放技能动画；首段命中后，二段立即绕过普通冷却门控；
- 新增 PIE 辅助命令 `DebugPrepareTwoStageArc [ForwardDistance]`，将最近存活敌人放到玩家正前方的近距离弧形判定内，用于稳定复现首段命中；
- 保留首段未命中超时进入冷却、二段释放后进入冷却的原有规则，并通过 `OnSkillStateChanged` 同步状态观察者。

PIE 验收顺序：

```text
DebugSelectSpecificReward TwoStageArc
DebugPrepareTwoStageArc 140
E                         // 首段命中，日志应出现 stage 2 unlocked
E                         // 二段释放，日志应出现 stage 2 released
```

回归项：首段命中窗口内再次按 E 不生成第二个首段；首段未命中仍在 `CircularSlashLifeTime` 到期后进入正常 E 冷却；二段释放只写入一次 E 冷却时间戳。

本次 PIE 验收结果（2026-08-26）：

- `DebugSelectSpecificReward TwoStageArc` 成功应用 E 形态，奖励 UI 正常关闭并恢复 Gameplay 输入；
- `DebugPrepareTwoStageArc 100` 配合固定玩家朝向后，第一段实际命中敌人，日志出现 `stage 1 hit ...; stage 2 unlocked`；
- 紧接按 E 成功生成 `Stage=2/2`，日志出现 `stage 2 released: DamageMultiplier=0.80`；
- 二段释放后在同一输入窗口再次按 E 未生成第三段，确认普通 E 冷却门控生效；
- 另行验证首段未命中时，日志出现 `stage 1 missed; E cooldown started without stage 2`。

### Slice 5：输入、冷却与 HUD 状态

已完成冷编译并完成 `TestMap_0`/`TestMap_1` PIE 验证。本切片不修改 Q 追踪逻辑；普通 E 的 Ready 显示规则与 Q 保持一致。

运行时状态由 `USkillComponent::GetSkillRuntimeState()` 统一输出：

```text
Ready
Stage1Active
Stage2Ready
Cooldown
```

状态与输入数据流：

```text
E 输入
  └─ TryCastSkillSlot(1)
       ├─ Ready       -> CastCircularSlash(Stage=0)
       ├─ Stage1Active -> 拒绝重复输入，等待命中或首段窗口结束
       ├─ Stage2Ready  -> CastCircularSlashStage2(Stage=1)
       └─ Cooldown     -> 拒绝输入

首段有效命中
  └─ bCircularSlashStage2Ready = true
       └─ 启动 Stage2InputWindow（默认 1.00s，限制在 0.8～1.2s）
            ├─ 再次按 E -> 第二段生成并写入 E 冷却时间戳
            └─ 超时      -> 清除 Stage2Ready 并写入 E 冷却时间戳
```

HUD 规则：

- Q 与普通 E 在 `Ready` 状态隐藏，进入 `Cooldown` 后才显示原有冷却动画；
- 获得 TwoStageArc 后，E 在尚未释放第一段时保持隐藏；
- E 进入 `Stage1Active` 后显示，第一段命中进入 `Stage2Ready`；
- `Stage2Ready` 使用轻微金色按键高亮和 `1.04` 倍缩放提示可释放第二段；
- 第二段释放或 Stage2InputWindow 超时后进入原有 `Cooldown`，冷却完成反馈结束后再次隐藏；
- 每个 Skill Slot 独立消费 `UpdateRuntimeState()`，不使用整个 Skill HUD 的统一 Visibility 控制。

PIE 验收结果（2026-08-26）：

1. 普通 E 在 Ready 状态隐藏，释放后显示冷却环，冷却与完成反馈结束后再次隐藏；
2. `DebugSelectSpecificReward TwoStageArc` 成功应用 E 两段形态，奖励 UI 关闭并恢复 Gameplay 输入，未释放第一段时 E 槽保持隐藏；
3. `DebugPrepareTwoStageArc 100` 后首段进入 `Stage1Active`，日志确认解析出的 `Stage2Window=2.00`，画面中 E 槽显示；
4. 本次 PIE 的调试敌人未被弧形命中筛选确认，因此 `Stage2Ready` 高亮、2 秒超时和第二段释放未作为通过项记录；后续应在命中测试工具修正后补验；
5. Q 槽未被 E 的显示状态联动，Q 追踪相关源码未修改；
6. 首段命中、二段释放和窗口超时过程中未出现新的 `Ensure condition failed`、`Fatal` 或 `Unhandled`。

可调接口：

- `USkillComponent::TwoStageArcStage2InputWindow`：编辑器/蓝图可调，默认 `2.0s`，运行时限制 `0.8～3.0s`；
- `USkillComponent::GetSkillRuntimeState()`：供 HUD 或其他观察者读取 E 当前状态；
- `EPlayerSkillRuntimeState`：以 BlueprintType 枚举暴露 `Ready`、`Stage1Active`、`Stage2Ready`、`Cooldown`。

### VFX 修复：TwoStageArc 仅保留黑色左键占位

`BP_PlayerSkill_CircleDamage` 仍保留旧 E 的 `NS_PlayerCircleSlash` 组件。此前该组件会与
`SkillComponent` 生成的黑色 `NS_CommonSlash` 同时激活，造成蓝色原 E 环与黑色占位叠加。
现在生成 `TwoStageArc` 伤害区域时会设置 `bUsePlaceholderVFXOnly`，在 BeginPlay 中停用并隐藏
伤害区域蓝图上的 Niagara 组件；普通 E 仍保留原有蓝图 VFX。后续接入正式 E VFX 时，只需替换
`CircularSlashVFX` 或移除旧蓝图组件，不改变伤害数据流。

## 美术接口

SkillComponent 暴露以下可替换接口：

- `CircularSlashVFX`：当前默认 `/Game/RawContent/VFX/NiagaraSystem/NS/CommonSlash/NS/NS_CommonSlash`；后续替换为 E 专用 Niagara 资源即可；
- `CircularSlashVFXColor`：默认黑色，用于区别左键；
- `CircularSlashVFXColorParameter`：默认 `User.Color`；运行时同时尝试 `User.Color`、`User.BaseColor`、`User.InkColor`，专用资源可统一保留其中一个参数；
- `CircularSlashVFXForwardOffset`、`CircularSlashVFXScale`：控制占位 VFX 的生成偏移与缩放；
- `TwoStageArcRadius`、`TwoStageArcHalfAngle`：控制近距离弧形判定尺寸；
- `TwoStageArcStageDamageMultiplier`：控制两段基础伤害倍率。

奖励 UI 暂不新增平面美术，`PopulateRewardPresentation` 使用已有 `Icon_CircularSlash` 作为占位图标。后续只需替换 `RewardIcon` 对应资源，不改变奖励数据流。

## 已知技术债

- 当前弧形为“球体粗筛 + 水平角度精筛”，没有独立弧形碰撞体；如后续需要边缘精确形状，可替换 `TryDamageActor` 的窄相位。
- 左键 Niagara 是否真正响应颜色取决于资源是否暴露对应 User 参数；代码已保留统一接口和日志，专用 E 资源应明确提供 `User.Color` 或更新蓝图/C++ 参数名。
- 尚未新增专用 E 笔迹、奖励平面图和独立 VFX 资源。

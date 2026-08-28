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
                      ├─ bHasTwinSlash = TwinSlash 是否已应用
                      ├─ TwinSlashDamageMultiplier = 1.0 或 0.65
                      ├─ StageDamageMultiplier = 0.8
                      ├─ Radius = TwoStageArcRadius
                      └─ ArcHalfAngle = TwoStageArcHalfAngle

E 输入
  └─ APlayerCharacter::RequestSkill2Input()
       ├─ Stage1Active -> 缓存一次第二段请求，等待首段命中
       ├─ Stage2Ready  -> 立即消费；若动作态仍阻塞，则由 EndAttack() 重试
       ├─ Ready        -> SwitchState(Skill2) -> TryCastSkillSlot(1)
       └─ Cooldown     -> 拒绝新的首段输入

首次释放：SpawnCircularSlashSet(Stage=0)
  └─ CircleDamageArea 球体粗筛 → 水平弧形点积精筛 → OnHitConfirmed
       ├─ 命中：bCircularSlashStage2Ready = true，并消费已缓存的第二段请求
       └─ 超时：写入 LastCastTimes，直接进入冷却

第二次释放：SpawnCircularSlashSet(Stage=1)
  └─ 清除 stage2 ready，写入 LastCastTimes，进入冷却

单次判定伤害
  └─ BaseEDamage × StageDamageMultiplier × TwinSlashDamageMultiplier
       ├─ 默认 E：D × 1.0
       └─ TwoStageArc + TwinSlash：D × 0.8 × 0.65 = 0.52D
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

### Slice 3：TwoStageArc 专用 VFX 与美术接口

已完成并通过冷编译及 PIE。普通 E 继续使用原有 `NS_CommonSlash`；`TwoStageArc` 使用专用正向刀光和镜像刀光，并按玩家水平朝向计算生成旋转与偏移。专用资源通过 `TwoStageArcVFX` / `TwoStageArcMirrorVFX` 配置，不再依赖左键黑色占位资源。

### Slice 4：输入态机与完整二段链路验收

已实现：

- `CanTriggerCircularSlashInput()` 统一 E 输入门控；首段命中窗口内重复按 E 不再重新进入技能状态或重复播放技能动画；首段命中后，二段立即绕过普通冷却门控；
- `Idle`、`Move`、`Skill2` 三个状态统一转发到 `RequestSkill2Input()`；`Skill2` 状态也持续监听 E，因此连续按键不会因为状态切换丢失；
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
  └─ APlayerCharacter::RequestSkill2Input()
       ├─ Stage1Active -> BufferCircularSlashStage2Input()，只保留一次请求
       ├─ Stage2Ready  -> TryConsumeBufferedCircularSlashStage2Input()
       │                    └─ TryCastCircularSlashStage2Immediately()
       ├─ Ready        -> SwitchState(Skill2)
       │                    └─ OnEnter -> TryCastSkillSlot(1) -> CastCircularSlash(Stage=0)
       └─ Cooldown     -> 拒绝新的首段输入

首段有效命中
  └─ bCircularSlashStage2Ready = true
             └─ 启动 Stage2InputWindow（默认 2.00s，限制在 0.8～3.0s）
             ├─ 已有缓存 -> 立即消费缓存并生成第二段，写入 E 冷却时间戳
             ├─ 再次按 E -> 缓存后立即消费，生成第二段并写入 E 冷却时间戳
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
4. `DebugPrepareTwoStageArc 100` 配合固定玩家朝向后，首段实际命中并进入 `Stage2Ready`；日志确认 `Stage2Window=2.00`，HUD 显示可释放状态；
5. 在 2 秒窗口内再次按 E 成功释放第二段，冷却开始；窗口超时日志确认 `stage 2 input window expired`；
6. Q 槽未被 E 的显示状态联动，Q 追踪相关源码未修改；
7. 首段命中、二段释放和窗口超时过程中未出现新的 `Ensure condition failed`、`Fatal` 或 `Unhandled`。

可调接口：

- `USkillComponent::TwoStageArcStage2InputWindow`：编辑器/蓝图可调，默认 `2.0s`，运行时限制 `0.8～3.0s`；
- `USkillComponent::GetSkillRuntimeState()`：供 HUD 或其他观察者读取 E 当前状态；
- `EPlayerSkillRuntimeState`：以 BlueprintType 枚举暴露 `Ready`、`Stage1Active`、`Stage2Ready`、`Cooldown`。

### VFX 实现：TwoStageArc 专用正向/镜像刀光

`BP_PlayerSkill_CircleDamage` 的旧 `NS_PlayerCircleSlash` 组件只在普通 E 路径保留；生成
`TwoStageArc` 伤害区域时会隐藏该旧组件，由 `SkillComponent` 生成专用 `TwoStageArcVFX`。
`TwoStageArcMirrorVFX` 用于镜像挥砍方向；叠加 `TwinSlash` 时，每阶段的第二个判定使用镜像资源，形成 X 形叠加。
专用 VFX 的地面夹角、玩家前后/左右偏移、缩放和镜像 Yaw 均由 `SkillComponent` 参数控制，不改变伤害数据流。

## 美术接口

SkillComponent 暴露以下可替换接口：

- `CircularSlashVFX`：当前默认 `/Game/RawContent/VFX/NiagaraSystem/NS/CommonSlash/NS/NS_CommonSlash`；后续替换为 E 专用 Niagara 资源即可；
- `CircularSlashVFXColor`：默认黑色，用于区别左键；
- `CircularSlashVFXColorParameter`：默认 `User.Color`；运行时同时尝试 `User.Color`、`User.BaseColor`、`User.InkColor`，专用资源可统一保留其中一个参数；
- `CircularSlashVFXForwardOffset`、`CircularSlashVFXScale`：控制占位 VFX 的生成偏移与缩放；
- `TwoStageArcVFX`、`TwoStageArcMirrorVFX`：TwoStageArc 的正向与镜像专用刀光；
- `TwoStageArcVFXScale`、`TwoStageArcVFXHeightOffset`、`TwoStageArcVFXForwardOffset`、`TwoStageArcVFXRightOffset`：控制专用刀光的缩放和相对玩家位置；
- `TwoStageArcVFXYawOffset`、`TwoStageArcVFXGroundAngle`：控制沿玩家朝向的镜像夹角和刀光平面与地面的夹角，均支持负角度；
- `TwoStageArcRadius`、`TwoStageArcHalfAngle`：控制近距离弧形判定尺寸；
- `TwoStageArcStageDamageMultiplier`：控制两段基础伤害倍率。

### Slice 6：TwinSlash 显式解析与组合验收

已完成并通过冷编译及 PIE。`FResolvedSkillSpec` 不再通过总 `HitCount` 推断 TwinSlash，改为显式输出：

- `bHasTwinSlash`：是否拥有 TwinSlash；
- `TwinSlashDamageMultiplier`：每次判定的倍率，默认 `1.0`，拥有 TwinSlash 时为 `0.65`；
- `JudgmentsPerStage`：每个阶段的实际判定数，默认 `1`，拥有 TwinSlash 时为 `2`。

运行时公式：

```text
TwoStageArc + TwinSlash
  每段：2 次判定
  每次：D × 0.8 × 0.65 = 0.52D
  每段合计：1.04D
  两段合计：2.08D
```

本次 PIE 日志验收（2026-08-26）：

- `DebugSelectSpecificReward TwoStageArc` 成功应用形态；
- 连续执行 `DebugSelectSpecificReward TwinSlash` 成功重新打开测试卡并应用 modifier；
- 解析日志确认 `Stages=2 JudgmentsPerStage=2 TwinSlash=true TwinSlashMultiplier=0.65`；
- 第一段日志确认 `DamagePerJudgment=62.4 StageTotal=124.8`；
- 第二段日志确认同样为 `DamagePerJudgment=62.4 StageTotal=124.8`，且出现 `stage 2 released`；
- 两段流程未生成第三段。

### Slice 7：Debug 奖励入口与回归验收

已完成。修复 `DebugSelectSpecificReward` 在同一 TestMap_1 PIE 会话连续选择不同奖励时仍受生产环境 `bRewardShownForRoom` 门闩阻挡的问题。生产流程的一次房间奖励限制保持不变，只有 Debug 入口在前一个反馈完成、`ActiveRewardWidget` 已清空后允许重新打开测试卡。

推荐测试指令：

```text
DebugSelectSpecificReward TwoStageArc
DebugSelectSpecificReward TwinSlash
DebugPrepareTwoStageArc 100
```

Slice 7 验收结果：

- 连续奖励指令不再报 `another reward UI is already shown`；
- 第二个奖励仍经过正常 `ApplyReward`、反馈动画和输入恢复流程；
- TwoStageArc 与 TwinSlash 可以同时存在，普通 E/Q 既有逻辑未被改变；
- PIE 运行期间未出现新增编译错误、`Ensure condition failed`、`Fatal` 或 `Unhandled`。

奖励 UI 暂不新增平面美术，`PopulateRewardPresentation` 使用已有 `Icon_CircularSlash` 作为占位图标。后续只需替换 `RewardIcon` 对应资源，不改变奖励数据流。

### Slice 8：第二段 E 手感——输入缓存与命中后即时消费（2026-08-28）

本次更新只调整 E 输入路由和第二段执行时机，不改变伤害公式、命中判定、E 冷却、第二段输入窗口或 VFX 选择。

实现内容：

- 新增 `bCircularSlashStage2InputBuffered`，作为一次性输入缓存；首段进行中连续按 E 时只记录一个请求，不重新进入 `Skill2` 或重复播放首段动画；
- `Idle`、`Move`、`Skill2` 的 E 事件统一进入 `APlayerCharacter::RequestSkill2Input()`；`PlayerState_Skill2` 在状态期间订阅 `OnEDelegate`，解决连续按 E 时事件被状态切换吞掉的问题；
- 首段命中确认后，如果缓存存在，立即调用二段消费路径；该路径只绕过通用 `CanStartAction()` 门槛，不绕过死亡、冲刺等安全限制；
- 如果命中回调发生时角色仍处于其他攻击动作，缓存不会丢失，`EndAttack()` 将在动作恢复到可施放状态后再次尝试消费；
- 在首段未命中、第二段窗口超时、形态切换、RuntimeData 应用、二段成功释放和结束游戏时清除缓存，避免跨技能串键。

本轮 PIE 已由用户完成；本文记录的是本次代码更新的预期链路和代码层结果，不替代逐个构筑的画面、伤害与消弹实测记录。

## TwoStageArc 与其他 E 技能构筑兼容性统计（代码层）

统计口径：以 `ResolveSkillSpec(CircularSlash)`、`CanApplyModifier()` 和 `ApplyModifier()` 的实际代码为准；“兼容”表示构筑可以被保存、解析并进入对应运行时路径，不代表每个组合的视觉表现都已在 PIE 中逐项验收。`D` 表示基础 E 单次判定伤害，以下合计假设所有判定均命中同一目标。

当前 8 个核心形态/功能组合均可解析：

| 构筑 | 阶段数 | 每阶段判定数 | 单次判定 / 附加效果 | 代码层结论 |
| --- | ---: | ---: | --- | --- |
| 普通 E | 1 | 1 | `D` | ✅ 兼容 |
| 普通 E + `TwinSlash` | 1 | 2 | 每次 `0.65D`，全段 `1.30D`；同时判定 | ✅ 兼容 |
| 普通 E + `NullRing` | 1 | 1 | `D`；范围内消除标记敌方投射物 | ✅ 兼容 |
| 普通 E + `TwinSlash` + `NullRing` | 1 | 2 | 每次 `0.65D`；同时判定并消弹 | ✅ 兼容 |
| `TwoStageArc` | 2 | 1 | 每次 `0.80D`，两段合计 `1.60D` | ✅ 兼容 |
| `TwoStageArc` + `TwinSlash` | 2 | 2 | 每次 `0.80 × 0.65D = 0.52D`，两段合计 `2.08D` | ✅ 兼容 |
| `TwoStageArc` + `NullRing` | 2 | 1 | 每次 `0.80D`；两段均保留消弹 | ✅ 兼容 |
| `TwoStageArc` + `TwinSlash` + `NullRing` | 2 | 2 | 每次 `0.52D`；两段均同时判定并消弹 | ✅ 兼容 |

兼容边界和注意事项：

- `TwoStageArc` 是 E 的形态字段；`TwinSlash`、`NullRing` 是可叠加 Modifier。新奖励流程应先应用 `TwoStageArc`，再通过 `ApplyModifier()` 添加其他 E Modifier；旧的 `ApplySkillForm(CircularSlash, TwinSlash/NullRing)` 兼容入口会改写旧形态字段，可能覆盖 `TwoStageArc`，不应用于新的叠加奖励。
- `RadiusUp` 可以通过规则校验并保存到 E，但当前 `ResolveSkillSpec()` 在识别 `TwoStageArc` 后会把半径改为 `TwoStageArcRadius`，因此它对 TwoStageArc 的弧形判定半径暂时没有实际效果；这是当前唯一需要特别标记的 E 构筑参数语义边界。
- `CooldownDown` 对所有 E 构筑有效，影响普通 E 冷却和二段释放/超时后的冷却；它不改变 `TwoStageArcStage2InputWindow`，也不会人为增加二段解锁延迟。
- `TwinSlashDelay`、`TwinSlashSecondYawOffset`、`TwinSlashSecondForwardOffset` 仍保留为序列化/编辑器兼容字段；当前运行时 TwinSlash 使用每阶段两次同时判定，不再按旧字段延迟生成第二次判定。
- VFX 方面，`TwoStageArc` 使用专用正向/镜像刀光；叠加 `TwinSlash` 时每阶段的第二个判定使用镜像刀光，形成 X 形叠加；`NullRing` 只改变消弹逻辑，不额外替换刀光资源。

## 已知技术债

- 当前弧形为“球体粗筛 + 水平角度精筛”，没有独立弧形碰撞体；如后续需要边缘精确形状，可替换 `TryDamageActor` 的窄相位。
- 左键 Niagara 是否真正响应颜色取决于资源是否暴露对应 User 参数；代码已保留统一接口和日志，专用 E 资源应明确提供 `User.Color` 或更新蓝图/C++ 参数名。
- 尚未新增专用 E 笔迹、奖励平面图和独立 VFX 资源。

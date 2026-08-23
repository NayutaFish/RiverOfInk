# Q 追踪联动奖励 Slice 7 数据流

日期：2026-08-22  
分支：`codex/q-homing-duration-slice1-2`

## 1. Slice 7 范围

Slice 7 将 Q 的 `ProjectileHoming` 构筑能力接入肉鸽奖励池，奖励名称为“引墨”。

- 只对 `EPlayerSkillID::TripleProjectile` 生效。
- 最大层数为 1；已拥有该构筑时不再生成无效重复奖励。
- 本切片不改变右键标记的唯一目标规则，也不新增第二套追踪系统。
- 暂无专用追踪图标，奖励展示暂复用弹幕数量占位图标，保持图标接口稳定，后续只替换资源路径。

## 2. 奖励生成数据流

```text
ARoguelikeRewardManager::GenerateRewardOptions()
    -> 构造 ProjectileHoming 候选
    -> USkillComponent::CanApplyModifier(TripleProjectile, ProjectileHoming, 1)
    -> 过滤已满层或不合法候选
    -> MakeModifierOption()
    -> FillModifierPreview()
    -> PopulateRewardPresentation()
    -> URoguelikeRewardWidget 展示奖励卡
```

“引墨”的预览值使用布尔状态映射：

```text
BeforeValue = bEnableHoming ? 1 : 0
AfterValue  = 1
OldValue    = 关闭 / 开启
NewValue    = 开启
```

## 3. 选择与应用数据流

```text
玩家点击奖励卡
    -> URoguelikeRewardWidget::SelectOption()
    -> ARoguelikeRewardManager::SelectReward()
    -> CanApplyModifier()
    -> USkillComponent::ApplyModifier()
    -> ProjectileHoming 层数 0 -> 1
    -> 重算 TripleProjectile 的 FSkillSpec
    -> 播放选择反馈
    -> 关闭奖励 UI
    -> 恢复 Gameplay 输入
```

应用失败时不会继续关闭流程，调用方保留当前奖励界面并输出失败日志。

## 4. Q 施法数据流

```text
Q 输入
    -> USkillComponent::ResolveSkillSpec(TripleProjectile)
    -> HasProjectileHoming() 读取 ProjectileHoming 层数
    -> FSkillSpec::bEnableHoming
    -> 生成左前 / 前 / 右前三枚弹幕
    -> 读取 ProjectileTargetingComponent 当前标记目标快照
    -> 将目标 Actor 与 FCombatEffectHandle 写入每枚 FProjectileSpec
    -> AAttackAreaBase / APlayerSkill_ThrownGrenade 每帧 UpdateHoming()
```

追踪是飞行过程中的方向修正，不是生成时直接传送或重新选择目标：

- 起飞后等待 `HomingStartDelay = 0.06s`。
- 以 `HomingTurnRate = 360 deg/s` 使用常量角速度修正朝向。
- 超过 `HomingMaxDistance = 2500` 或进入 `HomingAcceptanceRadius = 80` 后停止修正。
- 目标死亡、标记失效或标记已转移时清除追踪，弹幕继续按当前方向飞行。

## 5. 右键标记与追踪联动

```text
右键命中敌人
    -> ProjectileTargetingComponent::ApplyOrTransferHomingMark()
    -> 旧目标标记移除
    -> 新目标成为唯一当前标记目标
    -> 写入持续时间标识
    -> Q 施法时读取当前目标与标记句柄
```

没有“引墨”奖励时，Q 保持原有左前 / 前 / 右前散射；拥有奖励但没有有效标记时，同样保持散射，不会进行范围扫描或自动吸附。

## 6. 当前可调参数

参数位于 `USkillComponent` / `FProjectileSpec`，均可通过 `EditAnywhere, BlueprintReadWrite` 访问：

| 参数 | 默认值 | 作用 |
| --- | ---: | --- |
| `ProjectileHomingTurnRate` | 360 deg/s | 飞行中的最大转向速度 |
| `ProjectileHomingStartDelay` | 0.06 s | 发射后的追踪延迟 |
| `ProjectileHomingMaxDistance` | 2500 | 最大追踪距离 |
| `ProjectileHomingAcceptanceRadius` | 80 | 接近目标后停止修正的半径 |

## 7. PIE 验收记录

在 `TestMap_1` PIE 中使用 `DebugShowRewardSelection` 验证：

1. 成功找到 `RoguelikeRewardManager`。
2. 成功生成并显示 3 项奖励卡。
3. 点击奖励后完成应用流程。
4. 选择反馈结束后奖励 UI 关闭。
5. Gameplay 输入恢复。
6. 日志未出现新的 `Ensure condition failed`、`Fatal` 或 `Unhandled`。

本次随机展示的三项为“净墨环”“疾速回转”“纯墨”，因此未在该次随机 roll 中直接点击“引墨”；“引墨”候选的编译、合法性过滤、预览和展示分支已通过 Editor Target 构建检查。

## 8. 后续工作

- 制作并替换专用 `ProjectileHoming` 奖励图标。
- 增加固定奖励种子或调试命令，便于稳定验收“引墨”卡片和实际追踪命中。
- 在战斗测试场景补充右键转移标记、Q 三弹幕同步修正、目标失效后直线飞行的录屏验收。

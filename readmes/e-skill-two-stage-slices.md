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

# 玩家普攻 Slice 3–4：状态记录与后续讨论

日期：2026-08-13
工程：`E:\project\UE\demo0803` / `RiverOfInk`
目标分支：`feature/rab`

## 当前状态

| 项目 | 状态 | 说明 |
| --- | --- | --- |
| C++ 实现 | ✅ 已完成 | 普攻阶段、二段缓存、后摇冲刺取消、命中停顿与 VFX 占位已接入。 |
| UE 5.8 冷编译 | ✅ 已完成 | `RiverOfInkEditor` 构建结果为 `Result: Succeeded`。 |
| Combat Room PIE 启动 | ✅ 冒烟通过 | 已进入 `TestMap_1`，玩家与敌人实例正常生成。 |
| 攻击/二段/冲刺取消手感 | ⏳ 未测试 | 本次没有完成可靠的实际按键审评，不将其标记为通过。 |
| 命中停顿与 VFX 观感 | ⏳ 未测试 | 需要在正常可视 PIE 中结合敌人命中进行确认。 |

当前切片应视为“代码完成、手感未测试”。

## C++ 文件清单

| 文件 | 责任 |
| --- | --- |
| `Source/RiverOfInk/Script/Player/PlayerState/PlayerState_Attack1.h/.cpp` | `Startup → Active → Recovery`、二段输入缓存、后摇 `Space` 冲刺取消、普攻 VFX 生成与第二段 VFX 回退。 |
| `Source/RiverOfInk/Script/Player/Attack/AttackArea_PlayerAttack1.h/.cpp` | 普攻有效帧攻击区域与命中后 `HitStopDuration` 触发。 |
| `Source/RiverOfInk/Script/Player/PlayerCharacter.h/.cpp` | 支持二段攻击重播现有攻击 Montage，避免二段继续沿用上一段播放位置。 |

## 当前默认参数

```text
AttackStartupTime          = 0.08 s
AttackActiveTime           = 0.10 s
AttackRecoveryTime         = 0.14 s
AttackInputBufferWindow    = 0.18 s
MaxComboSteps              = 2
ComboSecondDamageMultiplier = 1.0
ComboSecondHitboxRadiusMultiplier = 1.5
AttackStartupMoveSpeed     = 350
AttackActiveMoveSpeed      = 180
AttackRecoveryMoveSpeed    = 500
bAllowDashCancelInRecovery = true
HitStopDuration            = 0.045 s
AttackVFXForwardOffset     = 60 cm
ComboSecondVFXForwardOffset = 100 cm
AttackVFXScale              = 1.0
ComboSecondVFXScale         = 1.5
```

第二段未配置专用 VFX 时复用：

```text
/Game/RawContent/VFX/NiagaraSystem/NS/CommonSlash/NS/NS_CommonSlash
```

本切片没有增加普通命中击退；敌人硬值被击破后的 `HitBack` 仍沿用现有敌人状态机路径。

第二段现在使用攻击区域蓝图默认 `Radius` 的 `1.5` 倍作为 Hitbox 半径，并使用独立的 VFX 缩放和前向偏移；未配置 `ComboSecondVFX` 时仍复用第一段 Niagara，但会以更大尺寸作为临时区分。上述 VFX 参数可在玩家的 `PlayerState_Attack1` 组件中手动调整，使视觉范围与实际 Hitbox 对齐。

## 本轮攻击范围与弹幕修复

AttackArea1 保持以玩家为圆心的扇形近战判定；右键 Attack2 已改为沿攻击方向移动的球形弹幕：

- AttackArea_PlayerAttack1：默认总角度 110°，青色调试边界。
- AttackArea_PlayerAttack2：使用基类 `CollisionSphere` 作为实际 Hitbox，不再使用扇形或玩家跟随。
- Attack2 的 Hitbox 与蓝图中的火球 Niagara 由同一个移动 Actor 驱动，避免 VFX 停留在生成点。
- 使用 Deferred Spawn 在 `BeginPlay` 前配置速度、生命周期、半径和障碍物检测，避免蓝图旧默认值抢先生效。
- 当前默认参数：速度 `900`、生命周期 `1.5s`、生成前置偏移 `120cm`、球形 Hitbox 半径 `50cm`。

PIE 已确认 `UEDPIE_0_TestMap_0` 正常创建；冷编译结果为 `Result: Succeeded`。实际右键视觉与手感仍以当前 PIE 手动审评为准。
## 后续实施建议

### P0：先完成当前切片手动 PIE 审评

建议按以下顺序记录手感结果：

1. 单击：确认前摇期间不产生伤害，Active 阶段才命中，Recovery 后能恢复移动。
2. 连按：在第一段 Active/Recovery 期间按第二次鼠标左键，确认二段不会丢输入，也不会重复生成第一段攻击区域。
3. 冲刺取消：只在 Recovery 按 `Space`，确认能进入 Dash；Startup/Active 按下时不应误取消。
4. 命中反馈：确认 `0.045s` Hit-stop 不过长，并检查命中、硬值击破、死亡三种反馈是否互相叠加异常。
5. VFX：确认第一段和二段的生成位置、朝向、生命周期；当前二段复用第一段仅为占位。

### P1：根据手感结果做攻击节奏微调

优先调整 `Startup / Active / Recovery` 与 `AttackInputBufferWindow`，再调整三个阶段的移动速度。建议一次只改一组参数，避免无法判断改动来源。

### P1：补充第二段攻击表现

在不改变攻击状态接口的前提下，为 `ComboSecondVFX` 配置独立资源；随后再将第二段 Montage、伤害倍率和硬值倍率做成可独立调参项。第一段 VFX 可继续作为无资源时的安全回退。

### P1：继续优化普攻与冲刺衔接

当前只允许 Recovery → Dash。若手感测试确认窗口过窄，再考虑增加 Dash 输入缓存或把取消窗口扩展到 Active 末段；扩展前应先确认不会跳过命中判定和 Hit-stop。

### P2：补充可重复的战斗回归检查

将普攻阶段、二段缓存、Dash 取消、命中停顿和硬值击破的关键日志整理成一组回归检查，避免后续攻击 VFX、敌人反应或动画改动破坏已验证行为。

## 暂不纳入本次提交

- 正式攻击动画与第二段专用 Montage。
- 第二段专用 VFX、命中闪光和敌人受击 VFX。
- 普通命中击退与新的硬值规则。
- 当前切片的手感通过结论。

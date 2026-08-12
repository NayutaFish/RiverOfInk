# 敌人状态机 Slice 4–5：冲撞型白盒闭环

## 范围

本切片沿用现有 `Idle → Chase → Attack / HitBack / TargetLost / Dead` 状态机，新增可配置的冲撞分支，不改变默认普通近战敌人的行为。

```text
Chase
  └─ 距离位于 ChargeMinRange…ChargeStartRange 且 bUseChargeAttack=true
       ↓
Charge Windup → Charge Active → Charge Recovery → Chase
                         │
                         ├─ 命中障碍/阻挡：进入 Recovery
                         └─ 硬值击破：立即切 HitBack
```

## 参数列表

| 参数 | 默认值 | 用途 |
|---|---:|---|
| `bUseChargeAttack` | `false` | 开关。关闭时沿用普通 Chase/Attack。 |
| `ChargeStartRange` | `1100` | 冲撞最大起始距离。 |
| `ChargeMinRange` | `450` | 冲撞最小起始距离；更近时走普通 Attack。 |
| `ChargeWindupTime` | `0.65 s` | 蓄力时间，方向在进入状态时锁定。 |
| `ChargeSpeed` | `1400` | 冲撞水平速度。 |
| `ChargeDuration` | `0.75 s` | 冲撞阶段最长持续时间。 |
| `ChargeRecoveryTime` | `0.8 s` | 冲撞结束后的恢复时间。 |

冲撞复用敌人的 `AttackAreaClass` 作为跟随型近战碰撞区域，因此伤害值、命中特效和命中音效仍由攻击区域蓝图配置；不需要新增一套伤害契约。

## 推荐 PIE 配置

对 `BP_EnemyTest1` 先使用以下白盒参数：

```text
bUseChargeAttack       = true
ChargeStartRange       = 1100
ChargeMinRange         = 450
ChargeWindupTime       = 0.65
ChargeSpeed             = 1400
ChargeDuration          = 0.75
ChargeRecoveryTime      = 0.80
AttackAreaClass         = BP_EnemyTest1_AttackArea_C
MaxHardValue            = 100
HardValueRecoveryDelay  = 0.75
HardValueRecoveryRate   = 50
HardBreakCooldown       = 0.35
HitBackDuration         = 0.20
HitBackSpeed             = 600
```

## PIE 验收矩阵

1. 玩家距离敌人 `> ChargeStartRange`：敌人保持 Chase，不进入 Charge。
2. 玩家距离在 `ChargeMinRange…ChargeStartRange`：日志依次出现 `charge windup`、`charge started`、`charge ended`、`charge recovery complete`。
3. 玩家进入冲撞路径：跟随型近战区域只对玩家结算一次伤害。
4. 冲撞期间硬值被击破：出现 `charge interrupted by hard break`，状态进入 `HitBack`，不得继续位移或保留攻击区域。
5. 冲撞碰到墙体/阻挡：以 `Reason=Blocked` 结束 Active，经过 Recovery 后回到 Chase。
6. `bUseChargeAttack=false`：回归原有普通近战状态机。

## 非目标

本切片不接入正式动画、角色模型、污染控制分支和 Behavior Tree；冲撞表现继续以位移、碰撞、日志和现有攻击区域为白盒验证依据。

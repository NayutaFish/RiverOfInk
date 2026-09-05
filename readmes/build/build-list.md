# 构筑清单

> 本清单以 `BuildPresentationResolver` 当前目录中的稳定 `BuildId` 为准，用于构筑 HUD、详细信息 HUD 和构筑 Icon 对接。
>
> `InkGrenade` 的实际语义是“投掷物式范围攻击”，不是炸弹；相关 Icon 禁止出现炸弹或类似物体，应突出范围、落点与区域覆盖。

| BuildId | 类别 | IconKey | 功能理解 | Icon 资源状态 |
| --- | --- | --- | --- | --- |
| `Q.TripleProjectile` | Q 技能 | `TripleProjectile` | Q 技能基础形态：一次发射三枚投射物。 | `_Redrawn` 已生成并导入 |
| `Q.AddProjectile` | Q 技能 | `ProjectileCount` | Q 投射物数量增加。 | `_Redrawn` 已生成并导入 |
| `Q.InkGrenade` | Q 技能 | `InkGrenade` | Q 投射物变为延迟落点范围攻击。 | `_Redrawn` 已生成并导入；不得使用炸弹意象 |
| `Q.ExtraExplosion` | Q 技能 | `ExtraExplosion` | Q 投射物命中后追加一次爆炸效果。 | `_Redrawn` 已生成并导入 |
| `Q.CooldownDown` | Q 技能 | `Cooldown` | Q 冷却时间缩短 0.5 秒。 | `_Redrawn` 已生成并导入 |
| `Q.ProjectileHoming` | Q 技能 | `ProjectileHoming` | Q 投射物获得追踪目标的能力。 | `_Redrawn` 已生成并导入 |
| `E.CircularSlash` | E 技能 | `CircularSlash` | E 技能基础形态：近身圆形斩击。 | `_Redrawn` 已生成并导入 |
| `E.TwoStageArc` | E 技能 | `TwoStageArc` | E 分为两段近距离弧形攻击，首段命中后解锁第二段。 | `_Redrawn` 已生成并导入 |
| `E.TwinSlash` | E 技能 | `TwinSlash` | E 每段增加一次独立伤害判定，表现为双斩。 | `_Redrawn` 已生成并导入 |
| `E.NullRing` | E 技能 | `ProjectileErase` | E 斩击区域会抹除其中的敌方投射物。 | `_Redrawn` 已生成并导入 |
| `E.RadiusUp` | E 技能 | `Radius` | E 斩击半径增加 60。 | `_Redrawn` 已生成并导入 |
| `E.CooldownDown` | E 技能 | `Cooldown` | E 冷却时间缩短 0.4 秒。 | 复用 `Cooldown` `_Redrawn` Icon |

## Icon 命名与解析

重绘 Icon 使用以下对象路径模式：

```text
/Game/RawContent/UI/Reward/Textures/T_UI_Build_<IconKey>_Redrawn
```

当前已有重绘资源：`CircularSlash`、`Cooldown`、`ExtraExplosion`、`InkGrenade`、`ProjectileCount`、`ProjectileErase`、`ProjectileHoming`、`Radius`、`TripleProjectile`、`TwinSlash`、`TwoStageArc`。

所有当前清单中的 IconKey 均有对应 `_Redrawn` 资源；不得根据显示名称臆造新的 BuildId。

## HUD 修改记录

详情 HUD 的最新功能、排版和选中态纹理要求见：

- [`build-hud-modification-log.md`](./build-hud-modification-log.md)

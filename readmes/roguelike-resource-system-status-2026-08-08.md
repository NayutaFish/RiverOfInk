# Pure Ink 经济系统状态记录

> 状态：暂停继续实施，保留 E0–E2 代码作为基础设施。
>
> 工程：`E:\project\UE\demo0803` / `RiverOfInk`

## 已完成

### E0：数据契约

- `FPureInkWallet`
- `EPureInkChangeReason`
- `FShopItemDefinition`
- `EShopItemEffectType`
- `FRunBuffData`
- `EPlayerRuntimeStat`

### E1：EconomySubsystem

`URoguelikeEconomySubsystem` 由 `UGameInstance` 持有，跨地图保存当前 Run 的 Pure Ink。

已支持：

- 增加 Pure Ink；
- 消费 Pure Ink；
- `OnPureInkChanged` 事件；
- 新局和返回准备房间时重置；
- 余额不能为负数或溢出。

### E2：收入来源

- Combat Room 中敌人死亡掉落 Pure Ink；
- Combat Room Clear 发放固定 Room Result 奖励；
- 敌人死亡和房间结算均有去重保护；
- 敌人掉落量和房间结算量可在蓝图中配置。

当前白盒默认值：

- 敌人掉落：`1 Pure Ink`；
- Combat Room Result：`20 Pure Ink`。

## 当前暂停范围

以下内容暂不实施：

- Shop Room 固定末位生成；
- Shop Item 购买与 SoldOut；
- 固定数值恢复；
- 跨 Combat Room 的临时属性 Buff 应用与计数；
- 购买后立即触发 Reward；
- Pure Ink HUD 和 Shop UI。

后续继续开发时，从资源系统策划案的 E3（Shop Room）开始，使用现有
`URoguelikeEconomySubsystem`，不要重新在 Pawn 或 Widget 中创建余额副本。

## 验证状态

E0–E2 已通过 UnrealHeaderTool、C++ 编译和链接验证；PIE 收入矩阵暂留到经济系统恢复实施时执行。

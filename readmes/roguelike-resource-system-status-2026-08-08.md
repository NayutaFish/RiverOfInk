# Pure Ink 经济系统状态记录

> 状态：E0–E2 已恢复实施；P1.5 最小 Shop Manager 已落地，Shop Room 接入和 UI 仍待后续切片。
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

## P1.5 最小 Shop Manager（2026-08-09）

已新增 `ARoguelikeShopManager`：

- `ERoguelikeRoomType::Shop` 作为房间类型；
- 默认固定报价：恢复 500 HP（10 Pure Ink）、额外一次二选一奖励入口（25 Pure Ink）；
- 购买前检查房间类型、余额、未知物品和 SoldOut；
- 通过 `URoguelikeEconomySubsystem::TrySpendPureInk` 原子扣款；
- 同一 Shop Manager 内每个物品只允许购买一次；
- `DemoRoomManager` 对非 Combat 房间跳过敌人刷怪。

本轮完整构建通过；默认地图序列目前仍为 Combat，因此 Shop 购买事务需要等 Shop Room 配置到房间池后再做 PIE 点击验证。

## 当前暂停范围

以下内容仍暂不实施：

- Shop Room 固定末位生成；
- 跨 Combat Room 的临时属性 Buff 应用与计数；
- 购买后立即触发 Reward；
- Pure Ink HUD 和 Shop UI。

后续继续开发时，从资源系统策划案的 E3（Shop Room 配置与 UI）开始，使用现有
`URoguelikeEconomySubsystem`，不要重新在 Pawn 或 Widget 中创建余额副本。

## 验证状态

E0–E2、P1.5 Shop Manager 已通过 UnrealHeaderTool、C++ 编译和链接验证；PIE 已验证敌人掉落、Room Result（20）与余额累加（21）。Shop 购买 PIE 暂留到 Shop Room 接入后执行。

# Pure Ink 经济系统状态记录

> 状态：E0–E2、P1.5、Economy Slice 3、Slice 4 与 Shop HUD V1 已落地。
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

## P1.5 / Economy Slice 4：Shop 购买与交互（2026-08-10）

已新增 `ARoguelikeShopManager`：

- `ERoguelikeRoomType::Shop` 作为房间类型；
- 默认固定三个报价：Quick Rinse（恢复 250 HP / 5 Pure Ink）、Pure Wash（恢复 500 HP / 10 Pure Ink）、Deep Cleanse（恢复 1000 HP / 18 Pure Ink）；
- 购买前检查 Shop 房间类型、余额、未知物品、SoldOut 与生命是否可恢复；
- 通过 `URoguelikeEconomySubsystem::TrySpendPureInk` 原子扣款；
- 同一 Shop Manager 内每个物品只允许购买一次；
- `DemoRoomManager` 对非 Combat 房间跳过敌人刷怪。

Economy Slice 3 已接入 RunFlow：默认白盒序列为 Combat → Boss Combat → Shop；Shop 会固定在生成序列最后一格。

### Shop Area / Trader 与输入

- `ARoguelikeShopManager` 自带 220 cm 白盒 `ShopArea`、圆柱形 Trader 标记和 `INK TRADER` 名牌；最终美术可直接替换 Trader Mesh；
- 玩家进入 Area 时显示事件驱动的交互提示 HUD：`[ J ] Talk to Ink Trader`；
- 默认按键 J（`APlayerCharacter::ShopInteractionKey`）打开 Shop HUD；
- Shop HUD 使用 UI Only 输入模式，Esc 关闭、恢复游戏输入；玩家仍在 Area 内时立即恢复交互提示，因此可再次按 J 打开。

### Shop HUD V1

- 原生 C++ UMG，采用 1280×720 参考布局和 `ScaleToFit`，适配不同分辨率；
- 顶部显示当前 Pure Ink 和 Esc 关闭提示；默认展示三个购买栏位；
- 每个栏位均包含 `BuyButton`、`ItemImage`、`DescriptionText`、`PureInkImage`、`PureInkCostText`；
- 按钮状态会呈现 `BUY`、`NEED INK`、`UNAVAILABLE`、`SOLD OUT`；
- Pure Ink 余额和购买结果由 `OnPureInkChanged` / `OnPurchaseCompleted` 事件驱动刷新，不依赖 Widget Tick。

## Economy Slice 3：Shop Room 固定末位（2026-08-10）

- `BuildRoomSequence` 从候选池中保留一个 Shop，其他候选继续按权重无放回抽取；
- Shop 追加到 `ActiveRoomSequence` 最后一格，长度为 1 的序列也遵循该规则；
- 多余 Shop 不会重复进入序列；
- `DemoRoomManager` 对 Shop 等非 Combat 房间继续跳过敌人刷怪；
- 默认白盒保留前一个 Encounter Tier 切片的 Boss Combat，并将 Shop 放在其后。

## 当前暂停范围

以下内容仍暂不实施：

- 跨 Combat Room 的临时属性 Buff 应用与计数；
- 购买后立即触发 Reward；
- Shop 以外的常驻 Pure Ink HUD；

后续继续开发时，从资源系统策划案的 Shop 后续效果切片开始，使用现有
`URoguelikeEconomySubsystem`，不要重新在 Pawn 或 Widget 中创建余额副本。

## 验证状态

E0–E2、P1.5 Shop Manager、Slice 3、Slice 4 和 Shop HUD V1 已通过 UnrealHeaderTool、C++ 编译和链接验证（`Result: Succeeded`）。
本次 PIE 日志验证了 `M01_Combat_A`（Index 0）、`M01_Combat_B`（Index 1，Boss）和
`M01_Shop_A`（Index 2，Shop），并输出 `ShopAtLast=true`；前置 Combat 正常启动并刷出敌人。
Shop PIE 在独立游戏窗口中验证：进入 Shop Area 显示 J 提示；按 J 打开 Shop HUD；Esc 关闭后再次按 J 可重新打开。

# 当前商店商品统计

统计基准：当前工作区 `main` 源码。商品定义来源于 `ARoguelikeShopManager::AddDefaultOffersIfUnset()`；该函数只在 `ShopItems` 为空时注入默认商品。

## 结论

当前默认商店共有 6 个商品：

- 3 个 `RestoreHealth` 即时恢复商品；
- 2 个 `TemporaryStatBoost` 临时属性强化商品；
- 当前 `URoguelikeShopWidget` 仍固定显示前 3 个槽位，因此后 3 个商品已由 Manager 定义和实现，但不会出现在现有 HUD 中；
- 1 个 `ImmediateRewardChoice` 商品；购买后复用现有奖励/构筑选择 UI 触发 1 次选择，不占用普通 Combat Room 的房间奖励门闩。

## 默认商品明细

| 默认槽位 | `ItemId` | 当前代码 Title | 中文 HUD 建议名 | 效果类型 | 实际效果参数 | 价格 | 当前 HUD |
| ---: | --- | --- | --- | --- | --- | ---: | --- |
| 0 | `shop_restore_small` | 快速冲洗 | 快速冲洗 | `RestoreHealth` | `EffectValue = 25`，恢复当前生命，最高不超过最大生命 | 5 | 显示 |
| 1 | `shop_restore_health` | 纯净洗涤 | 纯净洗涤 | `RestoreHealth` | `EffectValue = 50`，恢复当前生命，最高不超过最大生命 | 10 | 显示 |
| 2 | `shop_restore_full` | 深层净化 | 深层净化 | `RestoreHealth` | `EffectValue = 100`，恢复当前生命，最高不超过最大生命 | 18 | 显示 |
| 3 | `shop_temp_walk_speed` | 疾流 | 疾流 | `TemporaryStatBoost` | `StatType = WalkSpeed`，`AdditiveValue = 120`，`MultiplierValue = 1.0`，持续 2 个 Combat Room | 12 | 不显示 |
| 4 | `shop_temp_defense` | 墨甲 | 墨甲 | `TemporaryStatBoost` | `StatType = Defense`，`AdditiveValue = 15`，`MultiplierValue = 1.0`，持续 2 个 Combat Room | 12 | 不显示 |
| 5 | `shop_build_choice` | 墨引 | 墨引 | `ImmediateRewardChoice` | `EffectValue = 1`，触发 1 次现有奖励/构筑选择；`StatType = MaxHealth`（不使用），`EffectMultiplier = 1.0`，`CombatRoomDuration = 0` | 15 | 不显示 |



### 代码中的完整字段

`FShopItemDefinition` 当前包含以下字段：

| 字段 | 作用 |
| --- | --- |
| `ItemId` | 商品稳定标识，同时用于购买和本房间售出状态 |
| `Title` | 商品名称；默认商品使用中文名称 |
| `Description` | 商品描述；默认商品使用中文描述 |
| `Cost` | 纯墨价格 |
| `EffectType` | `RestoreHealth`、`TemporaryStatBoost` 或 `ImmediateRewardChoice` |
| `EffectValue` | 恢复量、临时属性加成量，或 `ImmediateRewardChoice` 的触发次数（当前为 1） |
| `StatType` | 临时属性目标；现有商品使用 `WalkSpeed`、`Defense`；构筑选择商品填充为 `MaxHealth` 但不读取 |
| `EffectMultiplier` | 乘算部分；现有 2 个临时商品均为 `1.0` |
| `CombatRoomDuration` | 临时效果剩余的 Combat Room 数；回血商品为 `0`，两个属性商品为 `2` |

## 当前购买条件

商品必须同时满足以下条件才会通过 `CanPurchaseItem()` 和 `PurchaseItem()`：

1. 商品存在，价格大于 0，且尚未在当前 `ShopManager` 中购买；
2. 当前处于有效的 Shop Room。正式商店默认 `bRequireShopRoom = true`；
3. GameInstance 经济系统中的 Pure Ink 余额不少于商品价格；
4. 商品效果当前可应用。

恢复类商品的额外条件：

- 玩家和 `HealthComponent` 有效；
- 当前生命值低于最大生命值；
- `EffectValue` 大于 0；
- 购买成功后按 `min(MaxHealth, CurrentHealth + EffectValue)` 设置生命值。

临时属性商品的额外条件：

- 玩家和 `RoguelikeRuntimeDataSubsystem` 有效；
- 持续 Combat Room 数大于 0；
- 加成值和乘数为有限数值，乘数大于 0；
- 非移速属性要求乘数为 `1.0`；
- 购买后写入 Run 数据，并立即重新应用玩家运行时属性。

构筑选择商品的额外条件：

- 玩家和 `SkillComponent` 有效；
- 当前地图存在 `RoguelikeRewardManager`，且其 `RewardWidgetClass` 已配置；
- 购买时必须能生成至少一个合法奖励选项；否则本次购买会回滚扣款和售出标记；
- 购买成功后关闭商店，打开现有奖励选择 UI；完成选择后重新打开同一商店。
- 选择内容复用 `GenerateRewardOptions()` 当前奖励池；该池包含可用的资源/生命即时奖励和技能构筑选项。

## 购买与持续时间

- 每个商品在一个 `ShopManager` 中只能购买一次；不同商品可以在余额足够时分别购买；
- 扣款通过 `URoguelikeEconomySubsystem::TrySpendPureInk()` 完成；
- 效果应用失败时会移除售出标记并退还 Pure Ink；
- `ImmediateRewardChoice` 购买完成后由奖励管理器广播选择结果；普通 Combat Room 的 `bRewardShownForRoom` 门闩保持不变；
- 临时商品的持续时间在每个 Combat Room 结束后消耗 1 次，减到 0 后移除；Shop Room 和其他非 Combat Room 不消耗次数；
- 当前默认列表没有商品会修改 `BaseAttackPower`，虽然 `EPlayerRuntimeStat` 已支持该属性。

## 当前 HUD 覆盖范围

`URoguelikeShopWidget` 使用 `VisibleOfferCount = 3`，刷新时只读取 `ShopItems[0]` 到 `ShopItems[2]`；因此新增商品暂时只能由 Manager/数据接口访问：

| HUD 槽位 | 对应商品 | 当前状态 |
| ---: | --- | --- |
| 0 | `shop_restore_small` | 可显示、可购买（还需满足回血条件） |
| 1 | `shop_restore_health` | 可显示、可购买（还需满足回血条件） |
| 2 | `shop_restore_full` | 可显示、可购买（还需满足回血条件） |
| 3 | `shop_temp_walk_speed` | Manager 已定义，HUD 不显示 |
| 4 | `shop_temp_defense` | Manager 已定义，HUD 不显示 |
| 5 | `shop_build_choice` | Manager 已定义并可购买，HUD 不显示 |

当前 Native HUD 已使用中文商品名、余额、按钮和反馈文案；售罄状态显示为 `售罄`，且对应栏位会清空其余文本并禁用购买。

售罄状态由 `ARoguelikeShopManager::PurchasedItemIds` 按 Shop Room 实例保存。关闭并重新打开 HUD 不会清空售罄状态；进入新的 Shop Room（创建新的 ShopManager）后，所有商品重新可购买。

## 配置边界

- `ShopItems` 是 `ARoguelikeShopManager` 的公开数组，可以由地图实例配置自定义商品；
- 只要该数组非空，`AddDefaultOffersIfUnset()` 就不会追加上述 6 个默认商品；
- 因此最终运行时商品列表应以当前地图中的 `ShopManager.ShopItems` 为准；本文统计的是无自定义配置时的默认列表；
- 当前 `Content` 资产中未检索到上述 6 个 `ItemId` 的额外序列化覆盖。

## 相关实现位置

| 内容 | 文件 |
| --- | --- |
| 商品结构体和效果枚举 | `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeEconomyTypes.h` |
| 默认商品和购买校验 | `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeShopManager.cpp` |
| 商店 Manager 接口及 `ShopItems` | `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeShopManager.h` |
| HUD 三槽位限制、商品显示和按钮状态 | `Source/RiverOfInk/Script/UI/RoguelikeShopWidget.h/.cpp` |
| 临时属性数据和 Combat Room 计数 | `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeRuntimeDataSubsystem.h/.cpp` |
| 奖励/构筑选择 UI 和应用 | `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeRewardManager.h/.cpp` |

# Economy Slice 4 / Shop HUD V1 C++ 代码清单

日期：2026-08-10  
工程：`E:\project\UE\demo0803` / `RiverOfInk`  
目标分支：`feature/rab`

> [!summary]
> 本清单覆盖 Shop Room 购买交互 V1：玩家进入 `ShopArea` 后显示 J 提示，按 J 打开三栏 Shop HUD，按 Esc 关闭并恢复游戏输入。余额仍由 `URoguelikeEconomySubsystem` 唯一持有。

## 1. 运行链路

```text
ShopArea overlap
  → PlayerCharacter.SetNearbyShopManager()
  → J / ShopInteractionKey
  → ARoguelikeShopManager::TryOpenShop()
  → URoguelikeShopWidget / UIOnly input
  → BuyButton
  → ARoguelikeShopManager::PurchaseItem()
  → EconomySubsystem::TrySpendPureInk()
  → RestoreHealth effect
  → OnPurchaseCompleted / OnPureInkChanged
  → Shop HUD refresh
```

## 2. C++ 文件清单

| 文件 | 责任 | 状态 |
| --- | --- | --- |
| `Source/RiverOfInk/Script/Player/PlayerCharacter.h/.cpp` | 持有默认 `ShopInteractionKey = EKeys::J`、注册/清理附近 Shop、转发交互 | ✅ |
| `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeShopManager.h/.cpp` | Shop Area、Trader 白盒、提示 HUD、三项报价、购买校验、输入模式切换 | ✅ |
| `Source/RiverOfInk/Script/UI/RoguelikeShopPromptWidget.h/.cpp` | 事件驱动的 `[ J ] Talk to the Ink Trader` 提示 | ✅ |
| `Source/RiverOfInk/Script/UI/RoguelikeShopWidget.h/.cpp` | 原生三栏 Shop HUD、购买按钮、余额/状态刷新、Esc 关闭、初始焦点 | ✅ |
| `Source/RiverOfInk/RiverOfInk.Build.cs` | 增加 `SlateCore`，满足原生 UMG 输入回复与链接依赖 | ✅ |
| `Content/Level/TestMap_Shop.umap` | Shop 白盒验证地图；Shop Manager 已恢复 `bRequireShopRoom=true` | ✅ |
| `readmes/roguelike-resource-system-status-2026-08-08.md` | 经济系统实施状态与验证记录 | ✅ |

## 3. 数据与购买规则

默认报价由 `ARoguelikeShopManager::AddDefaultOffersIfUnset()` 注入：

| Slot | ItemId | 显示名 | 效果 | 价格 |
| --- | --- | --- | --- | ---: |
| 0 | `shop_restore_small` | Quick Rinse | 恢复 250 HP | 5 |
| 1 | `shop_restore_health` | Pure Wash | 恢复 500 HP | 10 |
| 2 | `shop_restore_full` | Deep Cleanse | 恢复 1000 HP | 18 |

购买必须同时满足：

- Item 存在且价格大于 0；
- Shop Room 有效，除非仅用于数据测试并关闭 `bRequireShopRoom`；
- Item 尚未购买；
- 玩家生命未满且效果类型为当前已实现的 `RestoreHealth`；
- Pure Ink 余额足够；
- 通过 `TrySpendPureInk()` 原子扣款后才应用恢复效果。

Reward、临时属性 Buff 和常驻 Pure Ink HUD 不在本 Slice 的实现范围内。

## 4. Shop HUD 元素

`URoguelikeShopWidget` 使用原生 WidgetTree 构建 1280×720 参考布局，再通过 `UScaleBox::ScaleToFit` 适配屏幕。

每个购买栏位均包含以下核心元素：

- `ShopItemNBuyButton`
- `ShopItemNImage`
- `ShopItemNDescriptionText`
- `ShopItemNPureInkImage`
- `ShopItemNPureInkCostText`

按钮状态：

| 状态 | 条件 |
| --- | --- |
| `BUY` | 购买校验通过 |
| `NEED INK` | 余额不足 |
| `UNAVAILABLE` | 效果不可用、生命已满或未配置 |
| `SOLD OUT` | 当前 Shop 已购买该 Item |

HUD 不使用 Tick：余额变化订阅 `OnPureInkChanged`，购买完成订阅 `OnPurchaseCompleted`。

## 5. 输入与状态验收

| 场景 | 预期 | 状态 |
| --- | --- | --- |
| 玩家在 Area 外 | 不显示提示，不响应 Shop J | ✅ |
| 玩家进入 Area | 显示 `[ J ] Talk to the Ink Trader` | ✅ PIE |
| 按 J | 加入 Shop HUD，切换到 UIOnly，聚焦首个可用购买按钮 | ✅ PIE |
| 按 Esc | 移除 Shop HUD，恢复 GameOnly 和移动/视角输入 | ✅ PIE |
| 关闭后再次按 J | 可重新打开 Shop HUD | ✅ PIE |
| 有足够余额且生命受损时购买 | 扣款、恢复生命、按钮变为 Sold Out | ⏳ 待补充成功购买 PIE |

## 6. 接入注意事项

1. 正式 Shop Room 保持 `bRequireShopRoom=true`；直接打开白盒地图时，必须由 RunFlow 将当前房间设为 Shop 才会开放交互。
2. 不要在 Pawn 或 Widget 中复制 Pure Ink 余额；统一读取 `URoguelikeEconomySubsystem`。
3. Trader 当前是白盒圆柱和 `INK TRADER` 名牌，后续只替换 `TraderMarker` 的 Mesh/视觉，不改 Area 与输入链路。
4. 后续购买效果应扩展 `CanApplyItemEffect()` 与 `ApplyImmediateItemEffect()`，不要把 Reward 或 Buff 逻辑塞进按钮回调。

## 7. 验证记录

- UE 5.8 `Build.bat`：`Result: Succeeded`。
- 独立 PIE：进入 Shop Area 后看到 J 提示。
- 独立 PIE：J 打开 Shop HUD，Esc 关闭，J 再次打开。
- 编辑器退出 PIE 后，`TestMap_Shop` 的 `bRequireShopRoom` 已恢复为 `true`。
- 现有 `TakeDamage` C4263/C4264 属于历史警告，本 Slice 未修改其基类虚函数命名。

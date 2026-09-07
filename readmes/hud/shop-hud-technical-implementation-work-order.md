# 墨铺 Shop HUD｜技术实施工单

> 状态：已实施，待运行时验收
>
> 项目：RiverOfInk
>
> 最终验收基准：[`shop-hud-final-acceptance-2026-09-07.png`](../../Content/reference/shop-hud-final-acceptance-2026-09-07.png)

## 1. 工单目标

将现有 Shop Room 白盒商店改造成验收图所示的运行时墨铺 HUD：中央竖向宣纸卷轴、五个纵向商品行、统一的购买按钮、关闭入口、纯墨余额和按商品售罄状态。

本工单不改变商品的经济规则含义，不复制 Pure Ink 余额，不把商店逻辑移动到 Widget；它只负责把已有商品数据和购买事务呈现为最终交互界面，并补齐当前三槽位白盒实现与最终验收图之间的差异。

## 2. 最终画面契约

最终画面必须满足：

- 背景仍可见，但由商店显示层按验收图进行适度压暗；不能出现遮挡整个画面的不透明黑色矩形。
- 中央为纵向纸张面板，面板外边缘不裁切，四周保留游戏场景。
- 顶部左侧显示 `墨铺`；顶部右侧显示墨滴、纯墨余额和朱印装饰。
- 商品按固定顺序纵向显示五行：
  1. 快速冲洗：恢复 250 生命，5 纯墨；
  2. 纯净洗涤：恢复 500 生命，10 纯墨；
  3. 深层净化：恢复 1000 生命，18 纯墨；
  4. 疾流：移速 +120，持续 2 个战斗房间，12 纯墨；
  5. 墨甲：防御 +15，持续 2 个战斗房间，12 纯墨。
- 验收截图示例使用纯墨 `36`，第二项已经购买，因此第二行只显示居中的 `售罄`；该行高度和分隔线必须保留。
- 底部只有一个全局 `购买` 按钮，不为每一行生成独立购买按钮。
- 购买按钮下方显示 `关闭` 入口；键盘 `Esc` 继续可关闭商店。
- 所有商品名称、效果、价格、余额和状态均由运行时文本绘制，不烘焙进美术纹理。

## 3. 当前代码基线

当前实现已经具备部分数据和事务基础：

| 位置 | 当前事实 | 实施影响 |
| --- | --- | --- |
| `Source/RiverOfInk/Script/UI/RoguelikeShopWidget.h:43` | `VisibleOfferCount` 固定为 `3` | 改为五行纵向显示，并移除每槽位购买按钮作为最终交互模型 |
| `Source/RiverOfInk/Script/UI/RoguelikeShopWidget.cpp:112` | 使用原生 `WidgetTree` 构建白盒界面 | 可以继续复用原生树，但需要重构为纵向纸张面板和统一购买按钮 |
| `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeShopManager.cpp:392` | 默认数据已经定义五个商品 | 保持 ItemId 与数值稳定，Widget 不复制商品定义 |
| `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeEconomyTypes.h:52` | `FShopItemDefinition` 已包含标题、描述、价格、效果类型、数值、目标属性和持续房间数 | 直接读取该结构，不新增仅为 HUD 服务的商品副本 |
| `ARoguelikeShopManager::PurchasedItemIds` | 售罄状态由 Shop Manager 按当前 Shop Room 持有 | HUD 只查询 `IsItemPurchased()`，关闭/重开不能清空售罄状态 |
| `ARoguelikeShopManager::TryOpenShop()` / `CloseShop()` | 已有 Shop Room 检查、打开、关闭和输入模式切换 | 保持 J 打开、Esc 关闭和恢复 GameOnly 的现有语义 |
| `URoguelikeEconomySubsystem` | 是 Pure Ink 余额唯一持有者 | HUD 只读取余额并订阅现有事件 |

当前 `RoguelikeShopWidget` 仍使用深蓝色全屏白盒、横向三栏和每栏一个按钮；这些是旧白盒实现，不是最终验收布局。

## 4. 实施范围

### 4.1 Widget 结构与布局

在 `URoguelikeShopWidget` 中重构原生 WidgetTree：

- 根节点为屏幕空间 Canvas；保留可调透明背景层，但背景层不能改变世界画面的可见性。
- 使用中央对齐的纵向纸张面板；面板素材、底部山水、分隔线和购买墨条从 `/Game/RawContent/UI/Shop/` 加载。
- 使用 `ScaleBox` 或等价方式适配不同分辨率，保持面板和文字比例，不允许非等比拉伸。
- 标题行包含 `墨铺`、纯墨墨滴、余额文字和朱印装饰。
- 商品列表使用五个固定行。每行至少包含商品名称、效果描述、价格/墨滴、可选状态显示和选择热区。
- 每行保持固定高度；售罄时清空名称、描述和价格的可见内容，但不折叠行、不移动后续行。
- 底部创建一个全局 `购买` 按钮和一个 `关闭` 按钮/入口。
- 根节点和非交互装饰层不能阻断不应被阻断的输入；打开商店期间继续使用 UIOnly，关闭时恢复原有 GameOnly、鼠标和焦点状态。

### 4.2 选中与购买交互

最终画面只有一个购买按钮，因此 Widget 需要维护一个短生命周期的 `SelectedOfferIndex`，但不把它写入持久化运行时数据。

- 商品行可通过鼠标点击或键盘焦点选中。
- 打开商店时默认选中第一个可购买商品；如果第一个不可购买，则选择下一个可购买商品。
- 已售罄、余额不足、生命已满、效果不可用或未配置的商品不能通过全局购买按钮提交。
- 选中态只能使用轻量的纸面底色、墨线或透明度变化提示，不能新增与验收图冲突的高亮卡片。
- 全局 `购买` 按钮调用 `ARoguelikeShopManager::PurchaseItem(SelectedItemId)`，不能在 Widget 中直接扣款或修改生命/属性。
- 购买成功后先刷新余额和五行状态，再把购买商品显示为售罄，并将选中项移动到下一个可购买商品。
- 购买失败只更新运行时反馈，不应消耗 Pure Ink，不应错误地标记售罄。
- `关闭` 和 `Esc` 使用现有 `CloseShop()` 链路，关闭后重新按 J 能再次打开。

### 4.3 商品文案与数据映射

保持下列 ItemId、效果和价格不变；显示文案改为验收图中的中文，后续可迁移到项目本地化资源：

| ItemId | 中文标题 | 中文效果文本 | 价格 | 效果契约 |
| --- | --- | --- | ---: | --- |
| `shop_restore_small` | 快速冲洗 | 恢复 250 生命 | 5 | `RestoreHealth`, `EffectValue=250` |
| `shop_restore_health` | 纯净洗涤 | 恢复 500 生命 | 10 | `RestoreHealth`, `EffectValue=500` |
| `shop_restore_full` | 深层净化 | 恢复 1000 生命 | 18 | `RestoreHealth`, `EffectValue=1000` |
| `shop_temp_walk_speed` | 疾流 | 移速 +120 · 持续 2 个战斗房间 | 12 | `TemporaryStatBoost`, `WalkSpeed`, `EffectValue=120`, `CombatRoomDuration=2` |
| `shop_temp_defense` | 墨甲 | 防御 +15 · 持续 2 个战斗房间 | 12 | `TemporaryStatBoost`, `Defense`, `EffectValue=15`, `CombatRoomDuration=2` |

余额显示统一为 `纯墨 {Balance}`，价格只显示数值与纯墨滴图标。不能在 Widget 中建立第二份余额或商品效果计算逻辑。

### 4.4 售罄状态

售罄状态是本工单的关键验收点：

- 使用 `ShopManager->IsItemPurchased(ItemId)` 判断，不依据标题文本判断。
- 售罄商品只显示 `售罄`；商品名称、效果、价格、购买操作和选中态均隐藏/禁用。
- 售罄行仍保留原高度、纸面背景和分隔线，不能导致列表压缩或其他商品上移。
- 关闭再打开 HUD，售罄状态必须保持；进入新的 Shop Room 后由新的 Shop Manager 重新开始。
- `PurchasedItemIds` 仍由 Manager 管理，不能转移到 Widget 或 GameInstance 余额对象。

## 5. 资产接入边界

美术资产由对应美术工单交付，技术实现只负责加载、布局和状态叠加：

- `/Game/RawContent/UI/Shop/T_UI_ShopHUD_Panel`
- `/Game/RawContent/UI/Shop/T_UI_ShopHUD_LandscapeFooter`
- `/Game/RawContent/UI/Shop/T_UI_ShopHUD_PurchaseButton`
- `/Game/RawContent/UI/Shop/T_UI_ShopHUD_PureInkDrop`
- `/Game/RawContent/UI/Shop/T_UI_ShopHUD_Seal`
- `/Game/RawContent/UI/Shop/T_UI_ShopHUD_RowDivider`

资产缺失时允许使用明确的中性占位，不能回退到当前深蓝三卡片作为最终验收画面。文本字体使用项目现有字体或本地化字体设置，不在 PNG 中绘制。

## 6. 建议修改文件

### 必要修改

- `Source/RiverOfInk/Script/UI/RoguelikeShopWidget.h`
- `Source/RiverOfInk/Script/UI/RoguelikeShopWidget.cpp`

### 视实现选择修改

- `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeShopManager.cpp`：将默认商品的标题/描述迁移为中文 `FText` 或本地化键；不改变 ItemId、价格和效果数值。
- `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeShopManager.h`：仅在需要公开统一购买/选中状态接口时修改。
- `Content/RawContent/UI/Shop/*`：导入美术工单交付的 UI 纹理资产。

### 明确不修改

- `URoguelikeEconomySubsystem` 的余额所有权和扣款事务规则；
- `FShopItemDefinition` 的字段结构，除非编译或本地化接入发现明确缺口；
- Reward HUD、战斗 HUD、技能 HUD 和构筑 HUD 的布局与输入；
- `PlayerSkillWidget`、`CombatBuildHudWidget` 和奖励选择流程；
- Shop Room 判定、每房间单次购买语义，除非验收发现当前实现与契约不符。

## 7. 验收矩阵

### 7.1 数据与布局

- [ ] Shop HUD 由 J 在有效 Shop Room 中打开，Esc 和 `关闭` 均可关闭。
- [ ] 面板中央竖向显示，五个商品行全部可见，顺序与 ItemId 表一致。
- [ ] 标题为 `墨铺`，余额显示为 `纯墨 {Balance}`，不出现 `INK EXCHANGE`、`Pure Ink` 等旧英文文案。
- [ ] 只有一个全局 `购买` 按钮，不存在三个横向购买按钮。
- [ ] 运行时画面与验收图的纸张、分隔线、底部装饰和按钮层级一致。

### 7.2 售罄与购买

- [ ] 初始余额为 36、玩家生命受损时，五行商品可正确显示购买可用性。
- [ ] 购买 `shop_restore_health` 后，余额减少 10，生命恢复 500（不超过最大生命），第二行只显示 `售罄`。
- [ ] 关闭并重新打开 HUD，第二行仍为 `售罄`，其余行位置不改变。
- [ ] 选择 `shop_temp_walk_speed` 后应用移速 +120，持续两个 Combat Room，并在对应行显示售罄。
- [ ] 选择 `shop_temp_defense` 后应用防御 +15，持续两个 Combat Room，并在对应行显示售罄。
- [ ] 余额不足、生命已满、效果不可用和已售罄时，购买按钮不会扣款或标错状态。
- [ ] 同一 Shop Room 内每个 ItemId 只能购买一次；进入新 Shop Room 后状态重新建立。

### 7.3 输入与分辨率

- [ ] 打开 HUD 后游戏输入按现有规则切换到 UIOnly，关闭后恢复 GameOnly。
- [ ] HUD 不产生重复实例，关闭后重新打开不重复绑定事件。
- [ ] `1280×720`、`1920×1080`、`1483×1061` 和窗口缩放下，面板不变形、不裁切、不跑出安全区。
- [ ] 购买、关闭和焦点操作不会触发角色移动、攻击或技能输入。

## 8. 验证与提交要求

实现完成后按项目 `AGENTS.md` 执行验证：

1. 构建 Editor Target 前关闭 `UnrealEditor.exe` 和 Live Coding。
2. 只使用 UE bundled 的 `Engine/Build/BatchFiles/Build.bat`，同一时间只运行一个构建。
3. 构建结束检查 `%LOCALAPPDATA%/UnrealBuildTool/Log.txt` 的 `Result:`、`Exception`、`Unhandled` 和 `Fatal`。
4. 在 PIE 中完成上述购买、售罄、关闭、重开和分辨率验收，并保留最终截图。
5. 提交时将技术代码、美术 `.uasset` 和验收证据分组说明，不把无关 HUD/VFX 改动混入本工单提交。

## 9. 完成定义

五个商品能以验收图规定的纵向墨铺布局稳定显示，统一购买按钮能调用现有 Shop Manager 事务，售罄状态在同一 Shop Room 内持久保持，临时商品效果按既有运行时数据规则生效，关闭/重开和不同分辨率均通过验收。最终实现不复制经济数据、不依赖 Tick、不恢复旧三栏白盒作为最终视觉。

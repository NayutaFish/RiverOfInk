---
title: "经济系统 Shop Slice 4 与 Shop HUD V1 复盘"
date: 2026-08-10
project: RiverOfInk
branch: feature/rab
status: completed
tags:
  - obsidian/review
  - ue5
  - economy
  - shop
  - ui
---

# 经济系统 Shop Slice 4 与 Shop HUD V1 复盘

> [!summary]
> 本轮完成了从玩家靠近 Shop Area、出现交互提示，到 J 打开商店、Esc 关闭并再次打开的完整白盒链路。购买事务已经接入 EconomySubsystem，但“有余额且受损后成功购买”的 PIE 证据留到下一轮补齐。

## 关联笔记

- [[roguelike-resource-system-status-2026-08-08]]
- [[economy-shop-slice4-code-checklist-2026-08-10]]
- [[roguelike-resource-system-design-draft-2026-08-08]]
- [[bug-log]]

## 目标与结果

### 目标

- 玩家靠近 `ShopArea` 或 Trader 时看到交互提示。
- 默认使用键盘 J 打开 Shop HUD。
- Shop HUD 默认展示三个购买栏位。
- 每个栏位提供物品图、描述、Pure Ink 图标、价格和购买按钮。
- Esc 关闭商店，关闭后仍可在 Area 内再次交互。

### 结果

| 结果 | 状态 |
| --- | --- |
| Shop Area overlap 与玩家附近状态 | ✅ |
| J 交互提示与输入绑定 | ✅ |
| 三栏原生 C++ Shop HUD | ✅ |
| UIOnly / GameOnly 输入切换 | ✅ |
| Esc 关闭与 J 再打开 | ✅ |
| 三项治疗报价与购买校验 | ✅ 代码完成 |
| 成功扣款、恢复生命、Sold Out 的完整 PIE 点击证据 | ⏳ |

## 实现决策

### 1. 余额只由 EconomySubsystem 持有

Shop HUD 只读取 `URoguelikeEconomySubsystem` 的余额，并订阅 `OnPureInkChanged`。这样避免 Pawn、Shop Manager 和 Widget 各自缓存余额，减少跨房间状态分叉。

### 2. 先做三项可立即验证的治疗商品

本轮只开放 `RestoreHealth`，默认报价为：

- Quick Rinse：250 HP / 5 Pure Ink
- Pure Wash：500 HP / 10 Pure Ink
- Deep Cleanse：1000 HP / 18 Pure Ink

Reward、临时 Buff 等效果继续保留在数据契约中，但不提前伪装成已实现交易，后续通过 `CanApplyItemEffect()` 和统一事务入口接入。

### 3. 原生 WidgetTree + ScaleToFit

项目当前 HUD 主要由 C++ 原生 WidgetTree 构建，因此 Shop HUD 采用同一模式。使用 1280×720 参考布局和 `UScaleBox::ScaleToFit`，先保证结构、输入和不同分辨率下的整体比例，再替换正式美术资源。

### 4. UIOnly 输入与初始焦点

打开商店后切换到 `FInputModeUIOnly`，聚焦首个可用购买按钮；关闭时恢复 `FInputModeGameOnly`、鼠标状态、移动和视角输入。Esc 由 Shop Widget 处理，避免把关闭逻辑复制到 PlayerCharacter。

## 验证证据

> [!check]
> UE 5.8 Editor Target 构建完成，UBT 日志为 `Result: Succeeded`。

> [!check]
> 独立 PIE 中进入 Shop Area 后显示 `[ J ] Talk to the Ink Trader`；点击聚焦游戏窗口后，J 打开 Shop HUD，Esc 关闭，随后再次按 J 成功重新打开。

> [!info]
> 自动化按键需要先把焦点交给独立 PIE 游戏窗口；仅对编辑器视口图片发送按键时，事件不会稳定路由到游戏输入。这是验证工具的焦点限制，不是游戏输入绑定的运行时规则。

## 本轮踩坑与修正

### 初始 overlap 可能早于委托绑定

地图切换或玩家出生点已经在 ShopArea 内时，单纯依赖 `OnComponentBeginOverlap` 可能错过首次进入事件。Shop Manager 在下一帧额外调用 `RegisterPlayerIfAlreadyInsideShopArea()`，补齐初始状态。

### 玩家碰撞通道不是默认 Pawn

项目玩家胶囊使用 `GameTraceChannel3`。ShopArea 同时监听 `ECC_Pawn` 和 `ECC_GameTraceChannel3`，避免原生玩家与现有蓝图玩家的碰撞配置差异导致提示不出现。

### SlateCore 链接依赖

`NativeOnKeyDown` 使用 `FReply`，只声明 UMG/Slate 不足以完成链接，最终在模块依赖中补充 `SlateCore`。

### 测试地图与正式房间条件

为了直接验证独立 Shop 地图，PIE 期间曾临时关闭 `bRequireShopRoom`；验证结束后已恢复并保存为 `true`。正式流程仍由 RunFlow 将当前房间设为 Shop 后开放交互。

## 哪些地方做得好

- 交互提示、打开、关闭、重开形成闭环，且没有依赖 Tick 轮询玩家距离。
- 购买规则留在 Shop Manager，HUD 只负责展示和转发按钮事件。
- 余额变化与购买结果使用 Delegate 刷新，符合现有技能 HUD 的事件驱动方向。
- 白盒 Trader 与最终 Trader Mesh 解耦，后续替换视觉不需要重写交互。
- 通过直接可配置 `FKey` 提供 J 默认值，没有为了一个按键新增不必要的 Input Action 资产。

## 哪些地方仍需改进

- 当前没有在 PIE 中完整走通“玩家受伤 + 余额足够 + 点击购买 + 生命恢复 + Sold Out”的成功事务证据。
- ItemImage 和 PureInkImage 目前使用 Engine DefaultTexture 着色白盒，后续需要正式图标资源。
- Shop HUD 只在商店打开时显示余额；常驻 Pure Ink HUD 仍是后续切片。
- 购买失败反馈已具备，但还需要在实际资金与生命状态矩阵中验证所有按钮状态转换。

## 下一步

1. 在可控 PIE 场景注入一组 Pure Ink，并让玩家受损，完成三栏购买成功矩阵。
2. 验证购买后 EconomySubsystem 余额、HealthComponent 生命值和 Sold Out 状态同步。
3. 接入正式 Trader Mesh、ItemImage、Pure Ink 图标和 UI 视觉规范。
4. 实施 Reward / 临时 Buff 购买效果切片，继续复用统一购买事务。
5. 评估是否将 Shop 交互键迁移到项目统一 Input Action 配置，以支持设置菜单重绑定。

## 结论

本轮已经把“资源系统有 Shop 数据”推进到“玩家可以靠近、打开、关闭并再次打开商店”的可玩白盒交互。下一轮的核心不是再扩展 HUD 外壳，而是补足成功购买的 PIE 证据，并把白盒资源替换为正式视觉资产。

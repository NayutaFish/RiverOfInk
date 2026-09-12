# Result HUD 数据流

## 目标与边界

Result HUD 只读取一局结束时的**冻结快照**并呈现；它不直接统计战斗数据、不修改奖励，也不决定胜负。这样在页面展示、暂停游戏、重新开局或地图切换时，已显示的结算数据不会被后续事件污染。

## 总览

```text
战斗 / 商店 / 奖励系统
        │ 发布 EventBus 事件
        ▼
USettlementDataRecorder（单局累计）
        │ CaptureSnapshot + FreezeCollection
        ▼
URoguelikeRunFlowSubsystem（ResultSnapshot + RunOutcome）
        │ OnRunStateChanged → Result
        ▼
ARiverOfInkGameMode（创建并打开页面）
        ▼
URoguelikeRunResultWidget（纯展示、暂停、按钮意图）
        │ RestartRun / LoadPreparationRoom
        ▼
URoguelikeRunFlowSubsystem（校验、重置、切图）
```

## 1. 单局数据累计

`USettlementDataRecorder` 是静态记录器，模块加载时订阅 `FEventBus`，在一局开始时由 `URoguelikeRunFlowSubsystem::BeginNewRun()` 重置。

| 事件 | 记录字段 | 来源说明 |
| --- | --- | --- |
| `FNonPlayerDiedEvent` | `NonPlayerDeathCount` | 击败敌人总数 |
| `FOnEliteEnemyDiedEvent` | `EliteEnemyDeathCount` | 精英敌人数量 |
| `FOnNonPlayerTakeDamageFromPlayer` | `TotalDamageDealtToNonPlayer` | 玩家实际对非玩家造成的最终伤害 |
| `FShopCurrencyGainedEvent` | `TotalShopCurrencyGained` | 获得的纯墨总额 |
| `FShopPurchaseCompletedEvent` | `ShopPurchaseCount` | 完成购买次数 |
| `FRewardSelectedEvent` | `RewardPicks` | 奖励选择顺序；不合并、不排序 |

每项 `FSettlementRewardPick` 同时保留奖励类型、技能、形态、词条、层数、货币或生命值等稳定字段。Result HUD 用这些枚举字段经 `FBuildPresentationResolver` 查找项目内已有的 Build 图标，**不解析本地化文字，也不使用验收图中的占位图标**。

## 2. 房间与胜负收口

正常房间流程为：`InRoom` → 房间清空 → 发放纯墨 → 选择奖励 → `Completed`。奖励确认时，`ARoguelikeRewardManager` 先应用奖励，再发布 `FRewardSelectedEvent`，因此最终结算能包含该次已确认奖励。

RunFlow 有两条进入结算的路径：

- **失败：** `APlayerCharacter` 发布 `FPlayerDiedEvent`；`URoguelikeRunFlowSubsystem::HandlePlayerDefeated()` 只接受当前受控角色且状态为 `InRoom` 的事件，再以 `Defeat` 收口。
- **胜利：** 最后一个大关完成后，RunFlow 以 `Victory` 收口。

两条路径都会进入 `FinalizeCurrentRun(Outcome, Reason)`：

1. 仅允许从 `InRoom` 转入 `Result`，重复收口直接忽略。
2. 保存玩家运行时数据，供已有运行时存档消费者继续使用。
3. `CaptureSnapshot()` 值拷贝当前结算累计数据到 `ResultSnapshot`。
4. `FreezeCollection()` 拒绝所有迟到的战斗、商店和奖励事件。
5. 写入 `RunOutcome`，并发布 `OnRunStateChanged(..., Result, ...)`。

快照会保留至真正开始下一局；返回准备区不会清空本局结算。

## 3. 页面打开与展示

`ARiverOfInkGameMode::HandleRunStateChanged()` 收到 `Result` 后调用 `ShowRunResultHud()`：

1. 首次按 `RunResultWidgetClass` 创建 `URoguelikeRunResultWidget`，未配置蓝图子类时使用原生类。
2. `OpenForCurrentRun()` 从 GameInstance 的 RunFlow 读取 `ResultSnapshot` 与 `RunOutcome`。
3. 页面把快照映射为战绩、前六项增益历程与胜负标题；每项增益通过稳定字段解析现有 Build 图标。
4. 页面进入 UI-only 输入模式、显示鼠标、暂停游戏，并隐藏战斗 HUD。
5. 战绩、奖励、操作区按顺序淡入；页面本身不写回任何结算数据。

Result HUD 的宣纸为全屏不透明背景；两侧水镇纹理使用屏幕边缘锚点，不依赖固定 16:9 内容画布。战斗中的 `UCombatBuildHudWidget` 与结算页无数据所有权关系，但通过 `OverallHudScale = 0.85` 统一缩小显示。

## 4. 结果页离开

| 用户操作 | Widget 调用 | RunFlow 行为 |
| --- | --- | --- |
| 再来一局 | `RestartRun()` | 校验第一房间配置 → 重置玩家、经济、结算记录与进度 → 进入首房间 |
| 返回准备区 | `LoadPreparationRoom()` | 切换到准备区；保留结果快照，直到下一局开始 |

按钮点击后先禁用操作并恢复游戏输入。若 RunFlow 校验或地图切换请求失败，页面恢复 UI 输入、重新启用按钮并显示失败提示；不会丢弃当前结果快照。

## 5. PIE 验收入口

在已激活的房间（或直接 PIE 到已配置测试地图）中可使用：

- `DebugFinishRunVictory`：激活直接 PIE 地图所需的房间状态后，以胜利路径收口。
- `DebugTriggerPlayerDefeat`：激活直接 PIE 地图所需的房间状态后，调用角色的正常死亡路径。

两条命令最终复用上述 `FinalizeCurrentRun()` 与 Result HUD 数据流，而不是单独伪造 UI 数据。

## 关键代码位置

- `Source/RiverOfInk/Script/Core/SettlementDataRecorder.*`：事件订阅、累计、快照与冻结。
- `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeRunFlowSubsystem.*`：状态机、胜负收口、地图切换。
- `Source/RiverOfInk/Script/GameMode/RiverOfInkGameMode.*`：状态监听与页面创建。
- `Source/RiverOfInk/Script/UI/RoguelikeRunResultWidget.*`：快照展示、输入和结果页操作。
- `Source/RiverOfInk/Script/GameMode/RiverOfInkPlayerController.*`：PIE 调试命令。

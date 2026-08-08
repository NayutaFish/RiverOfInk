# Pure Ink 局内资源与 Shop Room 策划案（讨论稿）

> 状态：规则已确认；E0–E2 已实施，E3+ 暂停
>
> 工程：`E:\project\UE\demo0803` / `RiverOfInk`
>
> 本稿只定义局内货币、Shop Room 和 Shop Item 的规则，不修改技能构筑系统。

## 1. 目标与边界

本系统为纯局内 Roguelike 资源系统。游戏中暂时只有一种货币：`Pure Ink`。

Pure Ink 的用途是让玩家在 Shop Room 中购买一次性商品。商品效果分为：

1. 即时恢复；
2. 在后续若干个 Combat Room 中生效的玩家属性强化；
3. 购买后立即触发一次普通 Roguelike Reward 选择。

本版本不加入：

- 局外成长或永久货币；
- 第二种货币、碎片或材料；
- 技能系统的新形态实现；
- Shop Room 的带权随机刷新；
- 复杂背包、库存堆叠和售后退款系统。

## 2. 已确认的规则

### 2.1 额外 Reward 的语义

“额外一次 Reward 选择”不是增加 Reward 卡片数量，也不是保存一个以后使用的 Token。

玩家购买对应商品后，立即打开一次普通 Reward UI：

```text
购买 Shop Item
    → 立即触发 Reward UI
    → 仍然生成 2 项 Roguelike 构筑奖励
    → 玩家选择其中 1 项
    → 应用奖励并关闭 UI
```

因此，该商品的效果是一次即时事件，而不是 `PendingExtraRewardPicks` 之类的持久化计数。

Reward 的具体内容仍由现有 `RoguelikeRewardManager` 生成，商品不绕过奖励池，也不直接修改技能数据。

### 2.2 Pure Ink 获取途径

Pure Ink 只有两类来源：

1. `Enemy Drop`：敌人被击败时掉落；
2. `Room Result`：Combat Room 结算时发放固定房间奖励。

两类来源都必须调用同一个 Economy 接口，不能由敌人、UI 或 Shop 直接修改余额。

```text
Enemy Death  ─┐
              ├─> EconomySubsystem::AddPureInk(...)
Room Result  ─┘
```

第一版建议先使用固定测试数值，数值放在敌人配置和房间配置中，不硬编码在结算流程里。

### 2.3 临时属性强化的计数规则

商品提供的临时属性强化跨 MajorStage 保留，但只按实际房间类型消耗次数。

当前版本只有 `Combat` 类型会操作 `RemainCombatCount`：

```text
进入 Combat Room
    → 临时强化对本房间生效
    → RemainCombatCount - 1

进入 Shop / Preparation / Result
    → 不减少 RemainCombatCount
```

为保证 `N` 表示“后续 N 个 Combat Room”，进入房间时的顺序定义为：

1. 先让仍有次数的 Buff 对当前 Combat Room 生效；
2. 再将 `RemainCombatCount` 减一；
3. 当前 Combat Room 结束后，若次数为零则移除该 Buff。

例如购买 `N=2` 的强化后：

```text
Combat Room 1：生效，2 → 1
Combat Room 2：生效，1 → 0，房间结束后移除
Combat Room 3：不再生效
```

此规则不依赖地图名称，也不依赖关卡序号，只读取 `FRoguelikeRoomDefinition.RoomType`。

### 2.4 Shop Item 购买次数

每个商品在一个 Shop Room 中只能购买一次。

购买成功后：

- 商品卡标记为 `SoldOut`；
- 不能再次扣款；
- 不能重复应用效果；
- Shop UI 可以继续显示其他未购买商品。

是否允许玩家在同一个 Shop Room 购买多个不同商品：暂定允许，只要 Pure Ink 足够。

### 2.5 恢复数值

恢复使用固定数值，不使用百分比。

例如：

```text
Restore +300 HP
```

实际应用时：

```text
NewCurrentHealth = Min(CurrentHealth + RestoreValue, MaxHealth)
```

如果玩家当前生命值已经满，恢复类商品应显示为不可购买，避免购买后没有实际效果。

### 2.6 Shop Room 位置

当前版本 Shop Room 固定出现在地图序列的最后一个位置：

```text
ActiveRoomSequence[Length - 1] = Shop Room
```

前面的房间继续按现有 Combat Room 规则生成。

后续版本再加入 Shop Room 的带权随机刷新：

- Shop Room 作为 Room Pool 中的一种 `RoomType`；
- 只允许在配置的可刷新索引出现；
- 使用现有 `SelectionWeight` 参与抽取；
- 当前版本不提前实现随机逻辑。

## 3. 建议的数据结构

### 3.1 Pure Ink 钱包

建议新增独立的 `URoguelikeEconomySubsystem : UGameInstanceSubsystem`，负责跨地图保存货币和处理交易。

```text
FPureInkWallet
- int32 Balance
```

推荐接口：

```text
AddPureInk(int32 Amount, EPureInkGainSource Source)
TrySpendPureInk(int32 Cost, EPureInkSpendReason Reason)
GetPureInkBalance()
ResetForNewRun()
OnPureInkChanged
```

原因枚举至少包含：

```text
EnemyDrop
RoomResult
ShopPurchase
NewRunReset
```

`URoguelikeRuntimeDataSubsystem` 继续负责玩家快照；它不需要通过 Pawn 的 Capture/Apply 来保存 Pure Ink。

原因是货币属于 Run Economy，不属于 Pawn 的瞬时属性。这样可以避免玩家重生或跨地图时重复注册、重复应用或丢失货币。

### 3.2 Shop Item 定义

```text
FShopItemDefinition
- FName ItemId
- FText Title
- FText Description
- int32 Cost
- EShopItemEffectType EffectType
- float EffectValue
- int32 CombatRoomDuration
```

第一版效果类型：

```text
RestoreHealth
TemporaryStatBoost
ImmediateRewardChoice
```

属性强化的目标属性暂时限定为当前运行时数据已有字段：

```text
MaxHealth
Defense
WalkSpeed
SprintSpeed
```

这里的 `Defense` 是当前项目的单一防御值，不重新引入 Physical / Magical 双抗。

### 3.3 临时 Buff 状态

当前 `FRunBuffData` 仍是占位结构，后续应补充为：

```text
FRunBuffData
- FName BuffId
- EPlayerRuntimeStat StatType
- float AdditiveValue
- float MultiplierValue
- int32 RemainCombatCount
```

临时 Buff 不应直接覆盖基础属性。建议运行时按以下方式计算：

```text
EffectiveStat = StableRuntimeStat + ActiveRunBuffModifiers
```

这样 Buff 到期时只需移除 Modifier，不需要猜测原始属性值。

## 4. 推荐的模块职责

| 模块 | 职责 | 不负责 |
|---|---|---|
| `URoguelikeEconomySubsystem` | Pure Ink 余额、增减、重置、货币事件 | 玩家属性、商品 UI |
| `URoguelikeRuntimeDataSubsystem` | 玩家生命、属性、技能快照、临时 Buff | 货币扣款交易 |
| `URoguelikeRunFlowSubsystem` | Run、MajorStage、Room 序列和地图切换 | Shop 价格和购买逻辑 |
| `ARiverOfInkGameMode` | 解释当前 RoomType 和房间状态 | 直接操作商品数据 |
| `ARoguelikeShopManager` | Shop 商品、购买验证、效果应用、Shop 完成事件 | 直接改 UI 以外的房间序列 |
| `ARoguelikeRewardManager` | 生成和应用普通二选一奖励 | 扣除 Pure Ink |
| `URoguelikeShopWidget` | 显示余额和商品，发起购买请求 | 自己修改余额或玩家属性 |

## 5. Shop 购买事务

购买必须是原子流程，避免“扣款成功但效果没有应用”：

```text
ShopWidget 请求购买
    → ShopManager 检查 Shop 状态、商品索引、SoldOut
    → 检查商品效果是否可应用
    → EconomySubsystem 尝试扣除 Pure Ink
    → 应用 Item Effect
    → 标记商品 SoldOut
    → 广播 PurchaseCompleted
```

对于 `ImmediateRewardChoice`：

1. 先确认当前 Reward Pool 能生成合法的 2 个选项；
2. 再扣除 Pure Ink；
3. 立即打开现有 Reward UI；
4. 玩家二选一后，正常走 `ApplyReward`；
5. 奖励 UI 关闭后回到 Shop Room。

如果没有合法奖励或 UI 无法创建，商品不能购买，也不能扣除 Pure Ink。

## 6. 当前版本的房间链路

```text
Combat Room
    → Enemy Drop 累计 Pure Ink
    → Room Result 发放固定 Pure Ink
    → Room Clear Reward（二选一）
    → Exit

...

Shop Room（当前固定为序列最后一间）
    → 显示 Pure Ink 余额和商品
    → 购买一次性商品
    → 即时恢复 / 临时 Buff / 立即 Reward 二选一
    → 购买完成后继续留在 Shop
    → 玩家进入出口
    → RunFlow 进入后续流程或 Result
```

Shop Room 不生成敌人，不依赖 `DemoRoomManager` 的 Combat 刷怪流程。

## 7. 事件与日志

继续使用现有 `LogRoguelike`，关键日志包含：

```text
Pure Ink changed: Old=... New=... Delta=... Source=...
Enemy Pure Ink drop: Enemy=... Amount=...
Room result Pure Ink: RoomIndex=... Amount=...
Shop offer generated: ItemId=... Cost=...
Shop purchase succeeded: ItemId=... Cost=... Balance=...
Shop purchase rejected: ItemId=... Reason=...
Temporary buff entered combat: BuffId=... Remain=...
Temporary buff expired: BuffId=...
Immediate reward requested by shop item: ItemId=...
```

Pure Ink HUD 和 Shop UI 通过 `OnPureInkChanged` 更新，不使用 Tick 轮询。

## 8. 实施切片建议

### Slice 0：数据契约

新增钱包、商品和临时 Buff 的结构与枚举，不接 UI，不改技能行为。

验收：项目可编译；新 Run 能得到余额 0；结构能被 Blueprint 读取。

### Slice 1：EconomySubsystem

实现 Pure Ink 增加、消费、余额事件和新局重置。

验收：调用 EnemyDrop、RoomResult 能增加余额；余额不能变负；RestartRun 后归零。

### Slice 2：收入来源

接入敌人死亡事件和 Combat Room Result，确保每个来源只结算一次。

验收：同一敌人不能重复掉落；同一 Room Clear 不能重复发放房间奖励。

### Slice 3：Shop Room 固定末位

增加 `ERoguelikeRoomType::Shop`，在 `ActiveRoomSequence[Length - 1]` 放置 Shop Room，跳过敌人刷怪。

验收：当前序列只有最后一间是 Shop；前序 Combat 行为不变。

### Slice 4：Shop Item 购买

实现商品生成、价格验证、一次性购买和固定数值恢复。

验收：余额不足不能购买；购买成功扣款一次；商品变为 SoldOut；满血时恢复类商品不可购买。

### Slice 5：临时属性强化

实现 Buff Capture/Apply、进入 Combat Room 时的计数递减和到期移除。

验收：Shop/Preparation 不减少次数；连续跨 MajorStage 的 Combat Room 正确减少；`N=2` 恰好作用于两个 Combat Room。

### Slice 6：购买后即时 Reward

实现 Shop Item 触发现有二选一 Reward UI，选择后回到 Shop Room。

验收：一次购买只触发一次；始终为 2 项选 1；Reward 失败时不扣款；商品不能再次购买。

### Slice 7：Shop UI 与 PIE 矩阵

实现余额显示、价格、商品状态、购买反馈和最终序列流程。

## 9. 暂不确定的参数

规则已经确认，但以下内容仍属于平衡和内容配置：

- 敌人掉落 Pure Ink 的固定数值；
- 每个 Combat Room Result 的固定 Pure Ink 数值；
- Shop 商品数量和价格；
- 临时强化的属性值与 `N`；
- Shop 出口是否必须购买后才能激活；
- Shop Room 后面是继续加载下一房间还是直接进入 Result。

这些参数应放在 Room、Enemy 和 Shop Item 配置中，不写死在系统流程代码中。

## 10. 讨论结论记录

| 议题 | 当前结论 |
|---|---|
| 货币 | 只有 Pure Ink |
| 货币来源 | Enemy Drop + Room Result |
| 额外 Reward | 购买后立即打开普通 2 选 1 UI |
| 临时 Buff 范围 | 跨 MajorStage 保留 |
| 临时 Buff 计数 | 只有进入 Combat Room 才递减 |
| 商品购买次数 | 每个 Shop Item 一次 |
| 恢复数值 | 固定值，不能超过 MaxHealth |
| Shop 位置 | 当前固定在 ActiveRoomSequence 的最后一项 |
| Shop 随机刷新 | 后续加入带权随机和可刷新索引配置 |
| 当前状态 | 等待组内讨论，未实施 |

# RuntimeDataSubsystem 跨场景数据流

## 当前状态

本模块实现的是**单局游戏内、内存级别的跨场景数据通信**。

- `OpenLevel` 销毁旧关卡中的 Pawn 和 Actor 后，玩家的局内数据仍由
  `UGameInstanceSubsystem` 持有。
- 新关卡生成新的 `APlayerCharacter` 后，会先完成组件默认初始化，再应用已有快照。
- 当前已覆盖：血量、最大血量、物理抗性、魔法抗性、行走速度、疾跑速度、三个技能槽和技能升级状态。
- 当前未覆盖磁盘存档；关闭游戏或销毁 `GameInstance` 后数据会丢失。
- `FRunBuffData` 目前只是占位结构，尚未有增益组件或增益应用逻辑。

## 类职责

| 类/结构 | 职责 | 不应承担的职责 |
| --- | --- | --- |
| `FPlayerRuntimeData` | 聚合一局中需要跨场景传输的值类型数据 | 不持有 Pawn、Component 或 World 指针 |
| `UHealthComponent` | 持有血量和抗性；实现健康数据的 Capture/Apply | 不决定玩家 Actor 的销毁、重生或失败流程 |
| `USkillComponent` | 持有技能槽和升级等级；实现技能数据的 Capture/Apply | 不持有跨场景 Pawn 引用 |
| `APlayerCharacter` | 协调自身组件，将组件数据汇总为一个快照或应用一个快照 | 不直接持有跨场景快照 |
| `URoguelikeRuntimeDataSubsystem` | 持有当前局的 `FPlayerRuntimeData` 副本 | 不负责关卡切换、奖励 UI 或重新开始游戏 |
| `URoguelikeLevelFlowSubsystem` | 持有大关/小关序列，并在 Level Travel 前请求 Capture | 不负责玩家数据结构或游戏失败/重启决定 |
| `ARoguelikeRewardManager` | 修改当前关卡中玩家的实时技能并广播奖励事件 | 不直接拥有跨场景快照 |

## 核心数据结构

`FPlayerRuntimeData` 位于 `Source/RiverOfInk/Script/RoguelikeSystem/PlayerRuntimeData.h`，当前包含：

```text
FPlayerRuntimeData
├── TArray<FPlayerSkillSlot> SkillSlots
├── TMap<EPlayerSkillID, FSkillUpgradeState> SkillUpgradeStates
├── FPlayerRuntimeStats Stats
│   ├── MaxHealth
│   ├── CurrentHealth
│   ├── PhysicalResistance
│   ├── MagicResistance
│   ├── WalkSpeed
│   └── SprintSpeed
└── TArray<FRunBuffData> RunBuffs     // 占位
```

Subsystem 内部另外维护 `bHasPlayerRuntimeData`，用于区分“还没有建立本局快照”和“快照存在但数据为空”。

## 正常时序

### 1. 首次进入本局

```text
创建 APlayerCharacter
    ↓
HealthComponent / SkillComponent 使用默认值初始化
    ↓
APlayerCharacter::BeginPlay()
    ↓
RuntimeDataSubsystem::HasPlayerRuntimeData() == false
    ↓
CapturePlayerRuntimeData(Player)
    ↓
建立本局第一份快照
```

首次 Capture 的目的是登记当前 Pawn 的有效默认值，不是重新生成一个 Pawn。

### 2. 从旧关卡进入新关卡

所有由肉鸽流程发起的切图都应经过
`URoguelikeLevelFlowSubsystem::RequestLevelTravel()`：

```text
玩家进入已激活出口
    ↓
LevelFlowSubsystem::AdvanceToNextLevel()
    ↓
RequestLevelTravel()
    ↓
找到当前 Player 0
    ↓
RuntimeDataSubsystem::CapturePlayerRuntimeData(Player)
    ↓
UGameplayStatics::OpenLevel()
```

Capture 必须发生在 `OpenLevel` 之前，因为 `OpenLevel` 会销毁当前 World 中的 Pawn 和组件。

### 3. 新关卡生成 Pawn

```text
新 World 创建新的 APlayerCharacter
    ↓
组件完成默认初始化
    ↓
APlayerCharacter::BeginPlay()
    ↓
RuntimeDataSubsystem::HasPlayerRuntimeData() == true
    ↓
ApplyRegisteredPlayerRuntimeData(NewPlayer)
    ├── HealthComponent::ApplyRuntimeData()
    ├── SkillComponent::ApplyRuntimeData()
    └── 恢复 WalkSpeed / SprintSpeed
    ↓
创建 HUD，并发布当前血量事件
```

因此，`Apply` 是“恢复跨场景状态”，不是“替代组件默认初始化”。默认初始化先建立一个合法的新 Pawn，Apply 再覆盖需要继承的字段。

当前 `UHealthComponent::BeginPlay()` 和 `APlayerCharacter::BeginPlay()` 都会调用一次
`InitializeHealth()`，所以首次生成时可能看到两条初始化日志。这是现有生命周期的重复调用，
不代表快照被覆盖；真正存在快照时，后续的 Apply 会再用快照值覆盖默认值。后续整理初始化职责时，
不要把 Apply 当成新的默认初始化入口。

## 奖励与快照的关系

奖励选择时，`ARoguelikeRewardManager` 直接修改当前 Pawn 的 `USkillComponent`：

```text
点击奖励
    ↓
SelectReward()
    ↓
ApplyReward()
    ↓
SkillComponent::AddSkillToFirstEmptySlot()
    或 SkillComponent::ApplySkillUpgrade()
    ↓
OnRewardApplied.Broadcast()
    ↓
出口激活
    ↓
玩家进入出口后，下一次 RequestLevelTravel() Capture
```

当前设计下，奖励会在玩家进入出口并切图时写入 RuntimeData 快照。若未来需要支持“选择奖励后、进入出口前死亡/重启仍保留奖励”，应在奖励成功应用后增加一次显式 Capture 或统一的 RuntimeData 提交接口。

## 当前不跨场景传输的状态

以下状态目前按“关卡瞬时状态”处理，不写入 `FPlayerRuntimeData`：

- 技能当前冷却时间；Apply 时会清空新 World 中的 `LastCastTimes`。
- 冲刺、普通攻击和技能状态机的当前状态。
- 冲刺冷却、攻击冷却和直接伤害无敌计时器。
- `LastAttacker`、`bIsSprinting`、死亡表现和临时动画状态。

如果设计要求改变，先在数据契约中明确哪些状态是“局内持久状态”，再新增字段，不要直接把整个 Pawn 或 Component 指针放进 Subsystem。

## 调用契约

### 获取 Subsystem

```cpp
UGameInstance* GameInstance = GetGameInstance();
if (GameInstance)
{
    URoguelikeRuntimeDataSubsystem* RuntimeData =
        GameInstance->GetSubsystem<URoguelikeRuntimeDataSubsystem>();
}
```

### 推荐调用规则

1. 正常切图：调用 `URoguelikeLevelFlowSubsystem` 的推进接口，不要在业务 Actor 中直接 `OpenLevel`。
2. 切图前：必须 Capture 当前有效玩家。
3. 新 Pawn：让 `APlayerCharacter::BeginPlay()` 负责判断 Capture 或 Apply，不要在每个关卡蓝图中重复 Apply。
4. 新增可持久化组件：为组件增加 `CaptureRuntimeData()` / `ApplyRuntimeData()`，再由 `APlayerCharacter` 汇总，不要让组件直接互相访问。
5. 新增局内增益：扩展 `FRunBuffData` 和对应组件后，再接入 Capture/Apply。

## 新局重置边界

`URoguelikeRuntimeDataSubsystem::ResetPlayerRuntimeData()` 只负责清理玩家快照。

重新开始游戏的决定属于外部游戏流程模块。该模块需要协调：

```text
开始新局
    ↓
RuntimeDataSubsystem::ResetPlayerRuntimeData()
    ↓
LevelFlowSubsystem 重置大关、小关索引和已生成序列
    ↓
LoadPreparationLevel()
```

不要让 `RoguelikeRuntimeDataSubsystem` 自己发出重启事件，也不要把失败/结算逻辑塞进 `LevelFlowSubsystem`。

## 当前已知缺口

按优先级排列：

1. 接入外部新局重置入口，避免旧快照污染下一局。
2. 决定玩家死亡后重生时保留、重置还是恢复哪些数据。
3. 奖励应用后增加统一的快照提交时机（若需要支持奖励后立即失败）。
4. 为技能槽、技能 ID 和升级等级增加 Apply 前校验。
5. 实现 `FRunBuffData` 及增益组件的 Capture/Apply。
6. 若要支持关闭游戏后继续游玩，再创建独立的 `USaveGame` 数据结构；`UGameInstanceSubsystem` 本身不是磁盘存档。

## 日志与验证

关键日志类别：

- `LogRoguelikeRuntimeData`：Subsystem 初始化、快照注册、快照应用和失败原因。
- `LogSkill`：技能数据 Capture/Apply、技能添加和升级结果。
- `LogRoguelike`：奖励选择、出口激活和关卡流程相关事件。

最小 PIE 验证路径：

```text
TestMap_0 启动
    ↓ 确认 Runtime data subsystem initialized
完成一次奖励或修改玩家状态
    ↓
进入出口
    ↓ 确认旧关卡发生 Player runtime data registered
进入 TestMap_1 / TestMap_2
    ↓ 确认 Skill runtime data applied
    ↓ 确认 Player runtime data applied
    ↓ 对比 HP、抗性、速度、技能槽和升级数量
```

如果只看到默认初始化日志而看不到 Apply 日志，先确认新 Pawn 是否是 `APlayerCharacter`，以及切图是否绕过了 `RoguelikeLevelFlowSubsystem`。

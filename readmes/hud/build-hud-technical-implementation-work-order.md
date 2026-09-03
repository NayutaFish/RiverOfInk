# 战斗内构筑 HUD｜技术实施工单（功能切片版）

> 状态：待功能审查，审查通过后再进入代码实施
>
> 本工单只覆盖战斗内常驻构筑 HUD。详细信息 HUD、按 B 打开的暂停查看界面和最终美术制作不属于本工单。

## 1. 本轮要实现的功能

战斗内构筑 HUD 是一个只读、非交互的屏幕空间显示层，目标行为如下：

- 锚定在屏幕右下角，随分辨率变化保持右下安全边距。
- 只显示最近获得的构筑和上一条构筑：最近构筑为主显示，上一构筑为次显示。
- 没有构筑记录时隐藏整个 HUD；只有一条记录时隐藏上一构筑槽位。
- 新构筑成功写入运行时数据后，最近/上一构筑立即滚动更新；不按 Modifier 类型合并历史，也不丢弃重复获得记录。
- 主 HUD 只显示图标和 B 键提示，不显示构筑说明、参数详情、完整历史列表或技能计算结果。
- 根节点保持 `HitTestInvisible`，不能阻断移动、攻击、技能和奖励 HUD 的输入。
- B 键在本轮只作为可配置键位提示和后续接口边界；不在本轮实现详细信息面板、暂停、鼠标焦点或 `UIOnly` 输入模式。

目标显示结构：

```text
右下角 HUD
┌──────────────────────────────┐
│  最近构筑（较大）  上一构筑（较小/弱化） │
│                  [ B ]        │
└──────────────────────────────┘
```

上图只表达技术层级和槽位关系，不规定最终纸张、墨迹、边框或动画素材。

## 2. 当前代码基线与复用决定

| 现有入口 | 本轮决定 |
| --- | --- |
| `USkillComponent::BuildHistory` | 继续作为唯一显示数据源，保留完整历史，不新建 HUD 数据副本 |
| `FBuildHistoryEntry` | 直接作为两个可见槽位的输入；按数组尾部取最近和上一条 |
| `USkillComponent::OnBuildHistoryChanged` | 作为新构筑到达的主要刷新事件 |
| `FPlayerRuntimeData::BuildHistory` | 继续负责跨房间/关卡保存；HUD 创建后主动刷新，不能只依赖事件顺序 |
| `APlayerCharacter::CreateCombatBuildHudWidget()` | 继续负责本地玩家 HUD 生命周期和视口层级，目标 ZOrder 保持高于普通 HUD、低于 Reward HUD |
| `UPlayerSkillWidget` | 保持独立，不把技能冷却、技能等级或 Resolver 计算移入构筑 HUD |
| `ARoguelikeRewardManager` / `URoguelikeRewardWidget` | 本轮不改奖励流程；奖励成功后已有 `RecordBuildAcquisition()` 事件链即可驱动 HUD |
| 当前 `UCombatBuildHudWidget` 的 `DetailsOverlay` | 不作为新实现基础。详细信息层应在后续独立 widget/切片中实现，不能继续和常驻主 HUD 耦合 |

当前 `UCombatBuildHudWidget` 已能读取历史并加载部分图标，但其详细信息层、主 HUD 布局、图标路径和占位策略混在同一类中。重新实施时以“纯显示型主 HUD”为边界重构，不在旧类上继续叠加详细信息功能。

## 3. 功能切片

### Slice 0｜冻结显示契约与拆分边界

**目的**：先把常驻主 HUD 与详细信息 HUD 的责任切开，避免实现过程中再次把两者混在一起。

**技术要求**：

- 将 `UCombatBuildHudWidget` 定义为“最近/上一构筑显示 widget”。
- `DetailsOverlay`、详细说明文本、详细历史列表、暂停和输入模式切换不属于该 widget 的验收内容。
- B 键提示可以保留在主 HUD；详细信息打开请求只预留接口，不在本切片实现接收方。
- 不修改 `FBuildHistoryEntry` 的持久化结构，不增加只为 HUD 服务的构筑状态。

**验收**：代码结构中不存在主 HUD 必须依赖详细信息 widget 才能显示的路径；主 HUD 在没有详细信息类时仍能独立显示。

### Slice 1｜构筑历史可见窗口

**目的**：将完整 `BuildHistory` 稳定映射为两个可见槽位。

**技术要求**：

- 在 HUD 内部生成只读的显示快照：`bHasLatest`、`LatestEntry`、`bHasPrevious`、`PreviousEntry`。
- `LatestEntry = History.Last()`；`PreviousEntry = History[History.Num() - 2]`。
- `History.Num() == 0` 时折叠根显示；`History.Num() == 1` 时折叠上一构筑槽位。
- 相同 Modifier 的重复记录也必须按时间顺序保留。`StackDelta` 和 `ResultingStackCount` 不是合并可见槽位的依据。
- 新旧记录判定必须同时考虑历史长度和尾部 entry 内容，以兼容历史达到 64 条上限后数组长度不再增长的情况。

**建议入口**：保留 `RefreshBuildHistory()`，但把“取窗口”和“更新控件”拆成两个私有步骤，避免刷新时重复计算或动画读到旧数据。

**验收**：用一条、两条、三条以及重复同一 Modifier 的记录测试，两个槽位始终显示历史尾部两条，顺序正确。

### Slice 2｜白盒主 HUD 布局

**目的**：先不用最终美术资源，完成可验证的屏幕位置、尺寸和层级。

**技术要求**：

- 根节点使用 `CanvasPanel` 或等价屏幕空间容器，右下锚点，保留固定安全边距。
- 用 `SizeBox`/`ScaleBox` 保证主 HUD 有明确期望尺寸，并在不同分辨率下保持比例。
- 主槽位比上一槽位大；上一槽位允许降低透明度，但不能通过颜色承担唯一语义。
- B 键提示固定在主 HUD 下方/底部，不参与构筑数据计算。
- 主 HUD 根节点和所有装饰层使用 `HitTestInvisible`；不能为显示层设置按钮或抢焦点控件。
- 不加入构筑标题、描述、当前参数、完整历史或详细信息页文本。

**占位实现**：

- 没有面板素材时，用普通 `UBorder`/矩形底和固定边距表示面板边界。
- 没有槽位装饰素材时，用矩形、圆形或线条几何形表示两个槽位边界。
- 图标缺失时由 Slice 3 的几何占位 widget 绘制语义轮廓。
- 这些几何形只用于布局和功能验收，不作为最终美术交付。

**验收**：在 `1280×720`、`1920×1080` 和窗口缩放状态下，主 HUD 仍位于右下安全区；显示层不影响角色和奖励 UI 输入。

### Slice 3｜图标解析与几何占位

**目的**：让主 HUD 在已有素材、未来重绘素材和无素材状态下都能稳定显示。

**解析优先级**：

1. 已配置/已导入的构筑 icon `.uasset`。
2. 同名重绘资产，例如 `T_UI_Build_<Key>_Redrawn`。
3. 当前已有的基础构筑 icon 或技能 icon。
4. 没有可加载纹理时，使用几何占位 widget，不显示空白槽位。

当前测试目标的解析约定：

| 构筑 | 当前可用素材 | 无素材时的几何占位 |
| --- | --- | --- |
| `TwoStageArc` | 当前没有同名构筑 `.uasset`；可暂用 `Icon_CircularSlash` | 两段错位弧线 |
| `TwinSlash` | `T_UI_Build_TwinSlash.uasset` | 两道交叉/错位斜线 |
| `Cooldown` | `T_UI_Build_Cooldown.uasset` | 断裂环形/重复弧段 |

**技术要求**：

- 由 `FBuildHistoryEntry` 解析稳定的 `BuildIconKey`，不要以显示文本反向判断图标。
- 图标路径解析和缓存集中在一个 resolver/私有辅助层，不在最近槽位、上一槽位各写一份分支。
- PNG 文件只有在 UE 导入为纹理资产后才能通过 `/Game/...` 对象路径运行时加载；`Content` 下的原始 PNG 不能被当作已导入 `.uasset` 使用。
- 几何占位建议使用独立的 `UWidget` 子类覆写 `NativePaint`，绘制少量抗锯齿线段/弧段；不要通过加载默认白色纹理再模拟复杂语义。
- 占位形状只表达宏观差异，不添加文字、数字或最终美术纹理。

**验收**：删除或暂时隐藏任一 icon `.uasset` 后，HUD 仍显示对应几何占位；恢复资产后自动优先显示纹理，其他布局不变。

### Slice 4｜事件驱动刷新与跨房间恢复

**目的**：确保奖励成功、角色重建和关卡切换后显示内容一致。

**技术要求**：

- `InitializeForPlayer()` 完成绑定后必须主动执行一次完整刷新，不能假设 `OnBuildHistoryChanged` 一定先于 HUD 创建触发。
- HUD 订阅 `OnBuildHistoryChanged`，在事件中重新生成两个槽位的显示快照。
- 如果 `ApplyRuntimeData()` 发生在 HUD 已存在的生命周期中，应让运行时数据替换也能触发构筑历史刷新事件；不能依赖技能施放事件间接刷新。
- 普通 Q/E 施放、冷却倒计时和技能状态变化不应触发最近构筑入场动画。
- `NativeDestruct()` 必须解除委托；重新绑定玩家时先解除旧组件委托，避免重复刷新或悬空回调。
- 奖励 UI 仍由其自己的 ZOrder 和输入模式管理；构筑 HUD 只能在其下方显示。

**验收**：

- 奖励选择完成后不需要手动调用刷新，最近槽位即可更新。
- 进入下一房间/关卡后，HUD 从 `FPlayerRuntimeData.BuildHistory` 恢复相同的最近两条记录。
- 重建 Pawn 或重新 Possess 不产生两个构筑 HUD，也不产生重复事件回调。

### Slice 5｜最近构筑反馈动画

**目的**：让玩家能识别“刚获得的构筑”，但不影响战斗逻辑。

**技术要求**：

- 仅当历史尾部确实发生变化时播放；初始化已有历史时不播放入场动画。
- 新记录写入后先原子更新“最近/上一”数据，再启动动画，禁止动画期间显示旧图标配新文字或反之。
- 建议沿用短时缩放/淡入反馈，动画时长保持在约 `0.2s` 量级；具体曲线不依赖最终美术资产。
- 新奖励在上一条动画尚未结束时到达，应停止旧动画并从当前数据重新开始，不能把两个历史状态叠加。
- 动画只修改 widget 的 RenderTransform/Opacity，不修改世界时间、技能冷却或输入状态。

**验收**：连续快速选择两次构筑时，最终显示尾部两条正确记录，动画不造成空槽、残影或重复播放。

### Slice 6｜角色生命周期与 B 键边界

**目的**：让主 HUD 只属于本地玩家，并为后续详细信息 HUD 留出清晰接入点。

**技术要求**：

- 继续由本地 `APlayerCharacter` 创建，非本地 Pawn 不创建该 HUD。
- `BeginPlay`、`PossessedBy` 和 `SetupPlayerInputComponent` 的调用顺序不能造成重复实例；创建函数必须幂等。
- `EndPlay` 移除 HUD 并清空引用。
- 保留 `BuildDetailsKey` 作为可配置键位来源，主 HUD 只读取它显示 B 提示。
- 后续详细信息实现可接收“请求打开/关闭构筑详情”的接口，但本轮不实现接收 widget、暂停游戏、`FInputModeUIOnly`、鼠标显示、焦点导航或详情内容。
- 本轮不改 `RoguelikeRewardManager` 的暂停/奖励选择流程，也不把 B 键复用为奖励交互。

**验收**：主 HUD 在本轮不弹出详细信息页、不暂停游戏、不改变输入模式；后续详情 HUD 可以在不重写构筑历史数据链路的前提下接入。

## 4. 计划修改的文件边界

### 必要修改

- `Source/RiverOfInk/Script/UI/CombatBuildHudWidget.h`
- `Source/RiverOfInk/Script/UI/CombatBuildHudWidget.cpp`

### 视实现选择修改

- `Source/RiverOfInk/Script/UI/CombatBuildIconPlaceholderWidget.h/.cpp`：若采用独立 `NativePaint` 几何占位控件。
- `Source/RiverOfInk/Script/Player/PlayerCharacter.h/.cpp`：仅调整主 HUD 生命周期、B 提示接口或移除对嵌入式详细层的依赖。
- `Source/RiverOfInk/Script/Player/Skill/SkillComponent.cpp`：仅在需要支持“HUD 已存在时 ApplyRuntimeData 刷新”时补发构筑历史替换事件。

### 明确不修改

- `RoguelikeRewardManager`、`RoguelikeRewardWidget` 的奖励生成、选择、暂停/输入流程。
- `PlayerSkillWidget` 的技能图标、技能冷却和技能槽逻辑。
- `FBuildHistoryEntry` 和 `FPlayerRuntimeData` 的字段结构，除非功能审查发现明确的数据缺口。
- 任何最终美术资源的重绘、导入、材质制作或 `.uasset` 美术验收。
- 详细信息 HUD 的布局、说明文本、完整历史列表、暂停和 UIOnly 输入。

## 5. 当前占位素材策略

- 先使用当前已经存在并能被 UE 解析的 `.uasset` 图标验证数据链路。
- `TwoStageArc` 优先暂用现有 `Icon_CircularSlash` 或几何双弧占位；不为了技术验收等待最终 icon。
- 当前生成目录中的 `T_UI_Build_*_Test_Generated.png` 只有在导入 UE 后才作为纹理候选；导入动作不属于本轮技术方案的必要前置条件。
- 面板、飞白、纸张和装饰层没有现有素材时，使用简单矩形/线段/弧段完成布局验收；技术实现不得把缺少美术素材当作空白或功能失败。

## 6. 功能验收矩阵

| 场景 | 操作 | 预期结果 |
| --- | --- | --- |
| 空历史 | 进入战斗但未获得构筑 | 主 HUD 完全隐藏 |
| 首条记录 | `DebugSelectSpecificReward Cooldown 1` | 显示一枚最近构筑，上一槽位隐藏 |
| 第二条记录 | 再选择 `TwinSlash 1` | `TwinSlash` 成为最近，`Cooldown` 成为上一构筑 |
| 第三条记录 | 再选择 `TwoStageArc 1` | 只显示 `TwoStageArc` 与 `TwinSlash`，顺序正确 |
| 重复 Modifier | 连续选择同一合法 Modifier | 历史继续追加，不能被 HUD 合并为一条 |
| 跨房间恢复 | 选择构筑后进入下一房间/重建 Pawn | 最近两条仍与离开前一致 |
| 缺少纹理 | 暂时让某个 icon 对象路径加载失败 | 对应槽位显示几何占位，HUD 不消失 |
| 输入穿透 | HUD 覆盖在战斗画面上时移动、普攻、Q/E | 所有游戏输入保持正常 |
| 奖励层级 | 打开奖励 HUD 并选择奖励 | Reward HUD 可点击，构筑 HUD 不抢输入 |
| B 边界 | 按 B | 本轮不实现详细信息面板、不暂停、不切换 UIOnly；只验证键位边界未破坏战斗输入 |

## 7. 实施顺序与完成定义

建议按 `Slice 0 → 1 → 2 → 3 → 4 → 5 → 6` 顺序实施。每个切片完成后都应能独立编译并进行对应验收，不把最终美术资源作为技术切片的前置条件。

代码实施完成定义：

- 功能矩阵全部通过，且没有把详细信息 HUD 混入主 HUD 验收。
- 构筑 HUD 不复制 `ResolveSkillSpec()` 的计算逻辑，只读取 `BuildHistory` 和稳定的枚举字段。
- 缺少 icon/装饰素材时仍能用几何占位完成验证。
- 关闭 Editor 和 Live Coding 后，使用项目规定的 UE bundled `Build.bat` 完成 Editor Target 构建，并检查 UBT 日志中的 `Result:`、`Exception`、`Unhandled` 和 `Fatal`。
- 完成后再单独讨论并生成详细信息 HUD 和美术实施方案。

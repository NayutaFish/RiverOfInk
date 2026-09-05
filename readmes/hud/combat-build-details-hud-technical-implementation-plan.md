# 构筑详情 HUD｜技术实施方案

> 状态：详情目录完整展示修复、Slice 7 美术替换、排版与字体接入已完成；最终 Editor Target 已编译，详情弹窗级视觉 PIE 仍需可交互编辑器窗口补验
>
> 目标组件：按配置键打开的开书式构筑详情 HUD
>
> 基线分支：`codex/combat-build-hud-integration`（基于 `origin/codex/combat-build-hud`）
>
> 本方案供负责技术对接的 Codex 执行。参考图和美术工单只约束视觉与交互验收，不要求把整张验收图作为一张运行时大贴图使用。

> 执行记录（2026-09-03）：Slice 1–5 已落地并通过 `RiverOfInkEditor` 编译验证。当前仓库已补齐 `readmes/build/build-list.md`，展示目录以该清单的稳定 BuildId、分类、排序和 IconKey 为准，并已与当前运行时奖励池核对。
>
> Slice 6 执行记录（2026-09-03）：详情 Widget 已在初始化时解除旧委托并绑定 `USkillComponent::OnBuildHistoryChanged`，关闭/销毁时解除绑定；该事件覆盖构筑获取与 `ApplyRuntimeData()` 快照恢复，未订阅会随施放/冷却变化触发的 `OnSkillStateChanged`。详情 Icon 继续通过共享 resolver 优先解析 `_Redrawn` 纹理，缺失时保留几何占位；当前 11 个 IconKey 的 PNG/`.uasset` 对象路径已核对。详情纸张与选中蓝墨保持独立图层，未把包含文字/Icon 的验收合成图误当作面板背景。

> 执行记录（2026-09-04，功能与最终视觉接入）：详情打开条件已移除“当局 `BuildHistory` 必须非空”的门槛；`BuildPresentationResolver::BuildViewModel()` 始终遍历完整构筑目录，未获得项保留在分类槽位中并以弱化墨迹显示。最终面板使用独立透明 RGBA 纸张/飞白资源，包含与视觉验收图一致的圆形构图、组间间隔线、边缘墨迹、挂饰、印章和左下角留白；选中墨蓝使用独立透明圆形水痕层，位于 Icon 后方，不再使用蓝色矩形回退，也不与 Icon 或面板烘焙。详情布局基准调整为 `1200 × 860`，五个分类行均匀分配垂直空间，文字启用自动换行；指定 TTF 已导入为 `FontFace` 与运行时 `UFont`，详情文字优先使用该字体。

> 暂停检查点（2026-09-04）：当前任务按用户要求暂停，工作区未运行 Unreal Editor、UnrealEditor-Cmd 或 Live Coding。功能修复、最终美术资源生成/导入、排版代码调整、指定字体导入和 Editor Target 编译均已完成；两张详情纹理已复核为 `sRGB=True`、`lod_group=TEXTUREGROUP_UI`、`compression_settings=TC_EDITOR_ICON`、`mip_gen_settings=TMGS_NO_MIPMAPS`、`never_stream=True`。最新 standalone 1280×720 日志已确认奖励应用、构筑历史追加、常驻构筑 HUD 收到事件以及奖励界面关闭后恢复战斗输入。
>
> 恢复时的唯一待办：在可交互的 Unreal Editor/PIE 窗口中打开详情 HUD，补做无构筑历史完整目录、B/Escape 开关、UI Only 焦点、箭头滚动和 1280×720 / 1920×1080 / 宽屏视觉无裁切验收，并保留截图。当前命令行环境不能可靠注入 B 键，因此不能把详情弹窗级视觉验收标记为已通过；不要据此回退已接入的功能或美术资产。

## 1. 目标与边界

### 1.1 本轮目标

- 按 `APlayerCharacter::BuildDetailsKey`（默认 B）打开/关闭独立的构筑详情 HUD。
- 打开时暂停战斗世界，将输入切换到 UI Only 或等价的 UI 输入模式，并把焦点交给详情 HUD。
- 使用开书式面板：左页显示构筑分类和构筑槽位，右页显示当前选中构筑的图标、标题和说明。
- 每个分类最多显示 5 个可见构筑槽位；超出部分使用独立的无框 `<` / `>` 控件滚动。
- 支持鼠标点击、键盘焦点和键盘滚动，不阻断奖励 HUD 的数据链路。
- 详情 HUD 只读取技能构筑运行时状态，不计算或修改技能效果。

### 1.2 明确不包含

- 不修改 `ResolveSkillSpec()`、技能施放、冷却、伤害或构筑效果计算。
- 不修改奖励生成、奖励选择、奖励暂停流程。
- 不把详情面板嵌回 `UCombatBuildHudWidget`，也不改变常驻 HUD 的最近/上一构筑职责。
- 不改造 `FBuildHistoryEntry` 的持久化字段来存储 UI 文案或纹理对象。
- 不把视觉验收图、低保真图或箭头 glyph 烘焙进最终运行时面板背景。

关联基线：

- [战斗内构筑 HUD 组件说明](./combat-build-hud-component-overview.md)
- [常驻构筑 HUD 技术工单](./build-hud-technical-implementation-work-order.md)
- [构筑 HUD 美术实施工单](./build-hud-art-implementation-work-order.md)

## 2. 当前代码基线

当前实现已经具备以下可复用入口：

| 现有能力 | 位置 | 方案处理 |
| --- | --- | --- |
| 常驻最近/上一构筑 HUD | `UCombatBuildHudWidget` | 保持独立，只负责两个历史槽位和 B 提示 |
| B 键配置 | `APlayerCharacter::BuildDetailsKey` | 继续作为详情 HUD 的打开键和 UI 显示来源 |
| B 键入口 | `APlayerCharacter::ToggleCombatBuildDetails()` | 改为转发给独立详情 Widget |
| 构筑状态 | `USkillComponent::SkillSlots`、`SkillUpgradeStates`、`BuildHistory` | 生成只读详情 ViewModel |
| 构筑变化事件 | `USkillComponent::OnBuildHistoryChanged` | 详情 HUD 打开期间刷新；打开时主动刷新一次 |
| Icon 解析 | `UCombatBuildHudWidget` 当前的稳定键映射和路径回退 | 抽取为共享的展示解析辅助层，禁止复制两套映射 |
| 玩家生命周期 | `BeginPlay`、`PossessedBy`、`SetupPlayerInputComponent`、`EndPlay` | 详情 Widget 创建与销毁必须幂等 |

当前 `UCombatBuildHudWidget::ToggleBuildDetails()` 仍是兼容占位，`IsBuildDetailsOpen()` 仍固定返回 `false`。完成详情 HUD 后，常驻 HUD 不再作为详情状态的所有者。

## 3. 建议运行时架构

### 3.1 Widget 分层

建议新增 `UCombatBuildDetailsWidget`，蓝图资产建议命名为：

```text
/Game/Blueprint/GamePlay/MyCombatBuildDetailsWidget
```

运行时层级建议如下：

```text
CombatBuildDetailsWidget                 独立 UUserWidget，负责状态、输入和生命周期
└─ DetailsInputRoot                      全屏透明输入承接层，不绘制暗色遮罩
   └─ DetailsSafeZone                    安全区容器
      └─ DetailsScaleBox                 按设计比例缩放
         └─ DetailsPanel                 开书式面板容器
            ├─ DetailsPanelBackground    仅纸张、边缘和中缝，不含文字/Icon/箭头
            ├─ LeftPage
            │  └─ CategoryList
            │     ├─ BasicAttackRow
            │     ├─ ProjectileRow
            │     ├─ QSkillRow
            │     ├─ ESkillRow
            │     └─ GeneralRow
            └─ RightPage
               ├─ SelectedBuildPreview
               ├─ SelectedBuildTitle
               └─ SelectedBuildDescription
```

每个分类行建议由同一套 `UCombatBuildCategoryRowWidget` 或等价的内部构建函数生成，避免为每一行复制滚动和焦点逻辑。

### 3.2 ViewModel 与数据源

详情 HUD 不直接把 `FBuildHistoryEntry` 当作最终显示对象，而是从稳定构筑键和当前运行时状态生成只读 ViewModel。建议字段如下：

```text
FCombatBuildDetailsItem
  BuildId                 稳定构筑 ID / FName
  CategoryId              分类 ID
  SkillId                 关联技能，可为空
  IconKey                 交给共享 Icon resolver
  Title                   外部化 FText
  Description             外部化 FText
  StackCount              当前层数
  SortOrder               build list 定义的排序
  bOwned                  是否已获得/激活
```

ViewModel 的来源约定：

1. 构筑目录负责定义稳定 ID、分类、排序、IconKey、标题键和说明键。
2. `USkillComponent::SkillSlots` 提供当前技能形态、等级和 Modifier 层数。
3. `SkillUpgradeStates` 提供 Mechanic、Cooldown、Damage 等升级层数。
4. `BuildHistory` 只用于需要显示获取顺序或确认新构筑的场景，不作为 UI 自己的持久化副本。
5. 若说明中需要显示最终数值，调用现有只读 Resolver/summary 接口，不在 Widget 内重新推导公式。

当前已确认的稳定 IconKey 至少包括：

```text
TwoStageArc, TwinSlash, Cooldown, Radius,
ProjectileCount, InkGrenade, ExtraExplosion,
ProjectileErase, ProjectileHoming
```

`InkGrenade` 的展示语义必须是“投掷物式范围攻击/范围落点”，不能出现炸弹、手雷或类似物体。技术层只使用其稳定 ID 和对应 Icon，不以名称推断视觉语义。

### 3.3 Icon 解析

将当前 `UCombatBuildHudWidget` 中的稳定键解析和加载回退抽取为共享辅助层，例如 `BuildPresentationResolver`；常驻 HUD 和详情 HUD 共用同一份逻辑。

解析优先级保持：

1. 蓝图或配置中的 `ConfiguredBuildIcons` 覆盖；
2. `/Game/RawContent/UI/Reward/Textures/T_UI_Build_<Key>_Redrawn`；
3. 同名旧版纹理；
4. `UCombatBuildIconPlaceholderWidget` 或详情专用几何占位。

PNG 必须先导入 UE 为纹理资产后才能通过 `/Game/...` 对象路径加载。缺少纹理时显示语义占位，不得让整行消失。

## 4. 面板布局与资源接入

### 4.1 响应式布局

- 根节点使用全屏 `CanvasPanel`/等价容器，详情面板使用 `SafeZone + ScaleBox` 居中显示。
- 以开书面板的宽高比例作为设计比例，不把生成图的像素尺寸当作屏幕像素。
- 设计区域应按可用安全区缩放；禁止依赖固定屏幕坐标。
- 1280×720、1920×1080 和更宽比例下，面板不能裁边，左右页内容不能互相覆盖。
- 全屏输入层保持透明，不绘制覆盖整个屏幕的黑色或深灰色遮罩。
- 常驻 `UCombatBuildHudWidget` 不销毁、不复用详情数据；详情 Widget 使用高于常驻 HUD 的独立 ZOrder。

### 4.2 分类行

每行由以下部分组成：

```text
分类标识/标题 | 分隔线 | < | 槽位0 | 槽位1 | 槽位2 | 槽位3 | 槽位4 | >
```

视觉和技术约束：

- 槽位数量上限为 5；箭头不占用五个构筑槽位。
- `<` / `>` 是独立可交互控件，不能属于任何 Build Icon，也不能烘焙进面板背景。
- 箭头使用无框、无填充的 charcoal/墨线 glyph；禁止方框、圆框、黑色底板或按钮边框。
- 鼠标悬停/键盘焦点可通过轻微墨蓝晕染、字形加深或短下划线提示，但不能重新加入框。
- 仅当该分类存在可移动窗口时显示对应箭头；隐藏状态使用 `Collapsed`，不保留空白按钮占位。
- Icon 缺失时只替换该槽位内容为占位，不改变箭头和其他槽位位置。

### 4.3 面板资源

当前生成图可作为布局和视觉验收参考：

- [低保真原型（无框箭头）](../../Content/RawContent/UI/BuildHUD/T_UI_BuildDetailsPanel_LowFidelityPrototype_WithScrollArrows_NoFrame.png)
- [视觉验收图（无框箭头）](../../Content/RawContent/UI/BuildHUD/T_UI_BuildDetailsPanel_Separated_NoRightInkLabel_SimplifiedTexture_OpenLowerLeft_WithScrollArrows.png)

这两张图不是技术层级的替代品。运行时应优先使用拆分后的纸张背景、Icon、文字和控件；不得把验收图整张作为一张不可编辑面板贴图交付。若美术层尚未拆齐，可暂时把验收图仅用于白盒对齐，最终仍需保证箭头、Icon、文字和背景可独立控制。

常驻 HUD 的面板、飞白、墨蓝衬托和 B 键帽资源继续由现有组件使用；不要把 `RecentFeibai`、`RecentWash` 或 B 字符烘焙进详情面板。

## 5. 五槽位滚动规则

### 5.1 状态模型

每个分类独立维护：

```text
TotalCount       当前分类 ViewModel 数量
StartIndex       当前可见窗口起点
VisibleCount     min(5, TotalCount - StartIndex)
SelectedIndex    当前选中项的全局索引
```

边界公式：

```text
MaxStartIndex = max(0, TotalCount - 5)
StartIndex = clamp(StartIndex, 0, MaxStartIndex)
ShowPrevious = StartIndex > 0
ShowNext = StartIndex + 5 < TotalCount
```

数据层若把 `build.count = 6` 表示最后索引为 6，必须先转换成索引范围 `0..6`；不要直接把 count 当数组长度，避免 off-by-one。

### 5.2 用户确认的示例

当该分类索引范围为 `0..6` 时，状态必须为：

```text
StartIndex = 0  →  0,1,2,3,4,>
StartIndex = 1  →  <,1,2,3,4,5,>
StartIndex = 2  →  <,2,3,4,5,6
```

实现时不要把箭头伪装成第 6 个槽位；箭头必须在槽位容器之外单独生成。`TotalCount <= 5` 时两侧箭头均隐藏。

### 5.3 操作行为

| 输入 | 条件 | 行为 |
| --- | --- | --- |
| 鼠标点击 `>` | `ShowNext` | `StartIndex += 1`，刷新该分类行 |
| 鼠标点击 `<` | `ShowPrevious` | `StartIndex -= 1`，刷新该分类行 |
| 键盘 `D` | 当前选中 Icon 位于当前窗口最后一位且 `ShowNext` | 向右移动一格 |
| 键盘 `A` | 当前选中 Icon 位于当前窗口第一位且 `ShowPrevious` | 向左移动一格 |
| 左/右方向键或手柄横向输入 | 映射到同一焦点模型 | 普通槽位间移动；到边界时执行对应滚动 |
| `B` / `Escape` | 详情 HUD 已打开 | 关闭详情、恢复暂停和输入状态 |

点击箭头只改变该分类的可见窗口，不改变其他分类的 `StartIndex`。选中状态以稳定 `BuildId` 保存；窗口移动后若选中项仍可见则保持选中，若数据刷新导致其不存在则选择当前窗口第一项。

## 6. 焦点、鼠标与输入模式

### 6.1 打开详情

`APlayerCharacter::ToggleCombatBuildDetails()` 只负责请求打开/关闭，不在 PlayerCharacter 内绘制或保存详情布局。

打开流程：

1. 确认本地技能组件存在；详情面板始终从完整构筑目录生成，未获得项以弱化态显示，不以当局构筑历史作为打开门槛。
2. 幂等创建 `UCombatBuildDetailsWidget`，加入视口并设置详情层 ZOrder。
3. 保存打开前的暂停状态、输入模式、鼠标可见状态和必要的焦点信息。
4. 通过本地 `APlayerController` 暂停游戏。
5. 设置 `FInputModeUIOnly` 或项目等价模式，显示鼠标，并把焦点交给第一个可用构筑槽位。
6. 主动从 `USkillComponent` 生成一次完整 ViewModel 并刷新。

关闭流程：

1. 先解除详情 Widget 的输入焦点和鼠标捕获。
2. 仅当暂停由详情 HUD 取得所有权时恢复未暂停；不能无条件打断外部已有暂停。
3. 恢复打开前的输入模式、鼠标状态和焦点。
4. 隐藏或移除详情 Widget，清空其观察对象和临时窗口状态。

### 6.2 UI Only 下的 B 键

切换到 UI Only 后，不能依赖已经暂停的 PlayerCharacter 继续收到 B 键。详情 Widget 自身必须：

- 设置为可聚焦；
- 在 `NativeOnKeyDown` 或等价 UI 输入入口处理配置的 `BuildDetailsKey`；
- 同时支持 `Escape` 作为桌面兼容关闭键；
- 关闭事件返回 `Handled`，防止 B 键重复触发打开逻辑。

箭头的视觉无框不等于不可交互。`UButton` 可使用透明 `FButtonStyle` 承载较大的命中区域，子节点只绘制 `>` / `<` glyph；焦点提示使用字形状态变化或独立墨蓝晕染，不绘制边框。

## 7. 数据刷新与生命周期

### 7.1 事件驱动刷新

- 详情 HUD 打开时主动刷新，不能假设构筑事件先于 Widget 创建发生。
- 订阅 `USkillComponent::OnBuildHistoryChanged`，构筑成功追加后刷新对应 ViewModel。
- `ApplyRuntimeData()` 恢复运行时数据后必须触发一次构筑状态刷新；若现有事件语义不足，新增或扩展一个构筑状态事件，不使用 Tick 轮询。
- 普通 Q/E 施放、技能冷却变化和战斗帧更新不得触发详情列表重建。
- 刷新时保留可复用的 `BuildId`、分类窗口和选中状态；数据项不存在时按第 5 节规则回退。

### 7.2 绑定与销毁

- `InitializeForPlayer()` 或等价初始化接口先解除旧的 SkillComponent 委托，再绑定新对象。
- `NativeDestruct()` / `EndPlay()` 必须解除委托、停止输入捕获并释放 Widget 引用。
- `BeginPlay`、`PossessedBy`、`SetupPlayerInputComponent` 多次调用不能创建重复详情 Widget。
- 非本地玩家不创建详情 HUD。
- 奖励 HUD 的暂停和输入管理保持原状；详情 HUD 不修改奖励选项或奖励数据。

## 8. 建议实施切片

### Slice 0｜确认数据契约与边界

- 确认 `readmes/build/` 中的稳定构筑 ID、分类和排序为唯一展示清单。
- 明确 `build.count` 的长度/最后索引语义，并在 ViewModel 层统一为数组长度。
- 确认常驻 HUD 与详情 HUD 的所有权、ZOrder 和 B 键边界。

### Slice 1｜展示解析层

- 抽取共享 IconKey resolver。
- 新增构筑目录/展示映射，提供分类、排序、标题键、说明键和 IconKey。
- 从 `SkillSlots`、`SkillUpgradeStates` 和 `BuildHistory` 生成只读 ViewModel。
- 为 `InkGrenade` 写明范围攻击语义，禁止任何炸弹/手雷图形回退。

### Slice 2｜详情 Widget 白盒树

- 新增 `UCombatBuildDetailsWidget` 及建议蓝图 `MyCombatBuildDetailsWidget`。
- 完成 SafeZone、ScaleBox、左右页、五个分类行、右页详情区。
- 先用几何占位和透明 arrow glyph 验证布局，不等待最终面板美术。

### Slice 3｜分类窗口与箭头

- 每行固定 5 个槽位。
- 实现 `StartIndex`、`ShowPrevious`、`ShowNext` 和逐格滚动。
- `<` / `>` 使用独立透明按钮承载，无边框、无底板、无 Icon 烘焙。
- 覆盖 `0..6` 的三种验收状态。

执行结果：每个分类独立保存 `StartIndex`，窗口上限为 `max(0, TotalCount - 5)`；箭头仅在对应方向仍可移动时显示。箭头与槽位均使用独立透明命中控件，隐藏箭头不保留布局占位。

### Slice 4｜选中、焦点和输入

- 实现默认焦点、上下分类导航、左右槽位导航、A/D 边界滚动。
- 实现鼠标悬停、点击槽位和点击箭头。
- 处理 UI Only 下 B/Escape 关闭，确认焦点不会落到隐藏箭头。

执行结果：详情 Widget 维护稳定 `SelectedBuildId`、分类和全局索引；默认焦点落到第一个可见构筑槽位，鼠标悬停/点击与键盘、方向键、手柄十字键共用同一选中模型。A/D 或左右方向键在五槽位边界自动逐格滚动，上下键跳过空分类；箭头命中区透明且箭头按钮不可获得键盘焦点。

### Slice 5｜打开/关闭与暂停恢复

- 从 `APlayerCharacter` 转发到详情 Widget。
- 保存并恢复暂停、输入模式、鼠标和焦点状态。
- 验证常驻 HUD、奖励 HUD 和详情 HUD 的层级不互相破坏。

执行结果：`APlayerCharacter` 创建独立详情 Widget 并以 ZOrder 110 加入视口；打开时保存暂停、鼠标、移动/视角输入忽略状态和 Slate 焦点，设置 `FInputModeUIOnly`、暂停世界并把焦点交给首个槽位。详情 Widget 在 UI Only 下处理配置 B 键和 Escape，关闭时清除焦点、恢复 `GameOnly` 与原状态，并拒绝在奖励/商店等已有暂停模态上叠加打开。

### Slice 6｜事件刷新与资源接入

- 接入构筑变化和 `ApplyRuntimeData()` 恢复后的刷新事件。
- 接入导入后的 `_Redrawn` Icon 与分离的面板图层。
- 删除或隐藏任一 Icon 纹理时验证几何占位回退。

执行结果：详情 Widget 仅绑定低频 `OnBuildHistoryChanged`，打开时主动构建 ViewModel，构筑获取或运行时快照恢复后刷新分类窗口、选中预览和 Icon；`OnSkillStateChanged` 保留给技能 HUD，普通 Q/E 施放、冷却和 TwoStageArc 阶段变化不会触发详情列表重建。Icon 使用共享 `_Redrawn` 路径并保留逐槽几何占位回退；面板和选中蓝墨保持独立可选纹理入口，最终详情面板合成资源按 Slice 7 工单接入。

### Slice 7｜美术资源替换与接入

- 以 `readmes/hud/combat-build-details-hud-art-implementation-work-order.md` 为美术交付边界，先完成资源核对、导入和对象路径确认，再替换几何占位与临时图层。
- 以 `readmes/build/` 中最新 build list 的稳定 ID、分类、排序和 IconKey 为唯一资源清单；若清单缺失或与当前运行时目录不一致，先报告差异，不在 Widget 或美术工单中自行发明构筑 ID。
- 面板纸张/飞白、可选墨蓝衬托、Build Icon、分类装饰和文字保持可独立控制；箭头保持无框、无填充，不把 `<`/`>`、B 字符、标题、描述或槽位烘焙进面板背景。
- 所有最终 Icon 使用 `_Redrawn` 资源并沿用共享 resolver；`InkGrenade` 只表达投掷物式范围攻击/范围落点，禁止炸弹、手雷或类似物体。
- 资源替换不得改变五槽位布局、箭头显示/滚动规则、选中状态、窗口生命周期或输入逻辑；缺失资源仍须保留几何占位回退。
- 用最新无框箭头低保真图和视觉验收图复核开书比例、左页五行分类、右页选中预览、右页蓝墨与 Icon 分层，以及左下角的留白；面板外保持真透明，不得出现黑色矩形底。
- 美术资源接入完成并记录资产路径后，进入 Slice 8 的跨分辨率 PIE 验收。

执行结果（2026-09-04）：已按最新视觉工单接入独立纸张/飞白与选中墨蓝层，两个 PNG 均为透明 RGBA，分别为 2048×1472 与 1024×1024；对应 `.uasset` 已位于 `/Game/RawContent/UI/BuildDetails/Textures/` 并通过 UE 内容校验。面板包含组间间隔线、边缘墨迹、挂饰、印章和左下角留白，选中层为不含黑色的圆形蓝墨水痕，且在槽位 Icon 后方独立渲染。当前清单中的 11 个 IconKey 均已生成 `_Redrawn` PNG/`.uasset`，包括本轮补齐的 `TripleProjectile` 与 `CircularSlash`，继续使用稳定 ID 与共享 resolver；未把验收合成图、箭头、文字或黑色底板接入运行时。

### Slice 8｜跨分辨率 PIE 验收

- 完成下表测试并保留日志/截图。
- 关闭 Unreal Editor 和 Live Coding 后，使用项目指定的 bundled `Build.bat` 构建 Editor Target。
- 构建后检查 `%LOCALAPPDATA%\UnrealBuildTool\Log.txt` 中的 `Result:`、`Exception`、`Unhandled` 和 `Fatal`。

执行结果（2026-09-04）：正式源码使用项目指定 bundled `Build.bat` 完成 `RiverOfInkEditor` 编译，UBT 记录为 `Result: Succeeded`，未发现项目源码导致的 `Exception`、`Unhandled` 或 `Fatal`；构建包装进程返回码为 1，但不影响 UBT 的成功结果。随后在 `TwoStageArcVFXTest` 以 standalone PIE 分别发起 1280×720、1920×1080 和 2560×1080 请求，三次均进入测试地图并生成玩家，常驻构筑 HUD 完成初始化/订阅，资源与构筑历史链路可运行。1280×720 触发 `TwoStageArc`，1920×1080 触发 `TwoStageArc`，宽屏请求触发 `TwinSlash`；最新 1280×720 日志再次确认 `Combat build HUD received build-history change`、奖励应用和奖励界面关闭后恢复战斗输入。1920×1080 的运行时分辨率记录为 1920×1080；宽屏命令行请求为 2560×1080，但本机窗口最终记录为 1536×864，属于测试环境显示尺寸限制，不能当作 2560×1080 视觉通过。当前 standalone 命令仍无法可靠注入 B 键并取得详情 HUD 实际展开截图，因此“详情打开/关闭、UI Only 焦点、箭头交互、三分辨率视觉无裁切”仍未标记为通过；本轮已确认完整目录代码路径、最终资源接入、布局/字体代码可编译以及战斗构筑事件链路，待可交互编辑器窗口环境补做详情弹窗视觉验收。日志中的 Niagara Toolset Python `NiagaraToolset_Info` 缺失为 UE 自带实验工具启动警告，与本项目 HUD 代码无关。

## 9. 文件边界

### 9.1 预计新增/修改

必要部分：

- `Source/RiverOfInk/Script/UI/CombatBuildDetailsWidget.h`
- `Source/RiverOfInk/Script/UI/CombatBuildDetailsWidget.cpp`
- `Source/RiverOfInk/Script/Player/PlayerCharacter.h`
- `Source/RiverOfInk/Script/Player/PlayerCharacter.cpp`
- `/Game/Blueprint/GamePlay/MyCombatBuildDetailsWidget`

按实现选择新增：

- `Source/RiverOfInk/Script/UI/CombatBuildCategoryRowWidget.h/.cpp`
- `Source/RiverOfInk/Script/UI/CombatBuildDetailsSlotWidget.h/.cpp`
- `Source/RiverOfInk/Script/UI/BuildPresentationResolver.h/.cpp`
- `Source/RiverOfInk/Script/Player/Skill/SkillComponent.cpp`：仅用于补发运行时数据恢复后的构筑状态事件。

### 9.2 明确禁止混入

- `RoguelikeRewardManager` 的奖励池、奖励选择和暂停流程。
- `PlayerSkillWidget` 的 Q/E 技能 HUD。
- 常驻 HUD 的最近/上一构筑数据语义和布局。
- 技能 Resolver、技能 Actor、Niagara/VFX 和伤害计算。
- 最终 Icon 重绘、面板绘制和超出 Slice 7 资源替换/接入范围的美术返工。
- 任何把箭头框、B 字符或详情文案烘焙进面板背景的处理。

## 10. 功能验收矩阵

| 场景 | 操作 | 预期 |
| --- | --- | --- |
| 无构筑历史 | 战斗中按 B | 详情 HUD 仍可打开并显示完整构筑目录；所有项为未获得弱化态，战斗输入切换到详情 UI |
| 打开详情 | 有构筑时按 B | 面板出现、游戏暂停、UI Only 生效、焦点落在首个可用槽位 |
| 关闭详情 | 按 B 或 Escape | 面板关闭，暂停、输入模式、鼠标和焦点恢复 |
| 五项以内 | 分类数量 `<= 5` | 五槽位规则成立，两侧箭头均隐藏 |
| 六项 | 分类索引 `0..5` | 初始 `0..4,>`；末页 `<,1..5` |
| 用户示例 | 分类索引 `0..6` | `0..4,>` → `<,1..5,>` → `<,2..6` |
| 鼠标滚动 | 点击 `<` / `>` | 只移动当前分类窗口一格，箭头状态即时更新 |
| 键盘滚动 | 末位按 `D`、首位按 `A` | 与鼠标箭头相同；非边界输入不越界 |
| 选中构筑 | 点击或键盘移动 Icon | 右页图标、标题、说明同步，选中态清晰 |
| 缺少 Icon | 移除一个 Icon `.uasset` | 对应槽位显示几何占位，其他内容和箭头不消失 |
| 重复构筑 | 重复获得同一合法 Modifier | 按展示目录/层数约定显示，不在 HUD 私自合并历史 |
| 数据恢复 | 跨房间或 `ApplyRuntimeData()` | 详情列表与技能运行时状态一致，窗口边界重新校正 |
| 战斗输入 | 详情打开期间尝试移动、攻击、Q/E | 世界输入被暂停/UI Only 截断，关闭后恢复 |
| 奖励层级 | 打开奖励界面并选择奖励 | 奖励流程仍由 Reward HUD 管理，详情 HUD 不抢奖励点击 |
| 分辨率 | 1280×720、1920×1080、宽屏窗口 | 面板保持安全区内、无裁切、无比例崩坏 |
| 美术约束 | 检查箭头和 `InkGrenade` | 箭头无框；`InkGrenade` 不出现炸弹/手雷图形；面板外无黑色矩形 |

## 11. 完成定义

- 详情 HUD 是独立 Widget，常驻 HUD 不再承担详情状态。
- B 打开/关闭、暂停、UI Only、鼠标和焦点状态均可恢复，且没有重复实例或重复委托。
- 所有分类使用统一 ViewModel 和统一五槽位窗口逻辑。
- `0..6` 示例的三种窗口状态与交互约定完全一致。
- `<` / `>` 为独立无框控件，不占用 Build Icon 槽位、不属于背景或 Icon 贴图。
- 构筑展示使用稳定 ID 和共享 resolver；缺少资源时可回退到几何占位。
- 详情 HUD 不改动构筑计算、奖励流程和常驻 HUD 数据链路。
- 通过 1280×720、1920×1080、宽屏窗口、鼠标和纯键盘焦点验收。
- 关闭 Unreal Editor/Live Coding 后完成规定的 Editor Target 构建，并完成 UBT 日志检查。

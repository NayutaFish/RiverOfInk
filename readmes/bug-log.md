# Bug Log

## 2026-08-08：Slice 6–7 PIE 基础启动验证

- **验证结果**：`RiverOfInkEditor` 冷编译成功；PIE 成功创建 `/Game/Level/UEDPIE_0_TestMap_0`，并在验证后正常停止。
- **已确认**：`Health HUD`、`Skill HUD`、Q/E 图标、Q/E 构筑摘要和准备房间跳过敌人生成均在 `LogRiverOfInk` / `LogSkill` / `LogRoguelike` 中完成初始化。
- **尚未覆盖**：本次从准备房间启动，未进入 Combat Room，因此奖励卡选择、Modifier 组合、跨房间 Capture/Apply 和技能实际行为矩阵仍需下一轮 PIE。
- **环境提示**：UE 5.8 默认 Installed DDC/Zen/Shader 工作目录不可写；本次通过 `-DDC-ForceMemoryCache -SkipZenStore -NoZenAutoLaunch` 并指定项目 `Saved` 工作目录完成启动。Turnkey 权限和 EOS SSL 警告属于本机环境噪声。
- **遗留资产警告**：旧 `WBP_RoguelikeReward.uasset` 仍引用 `/Script/Test_GamePlay`，加载时会出现 `Unknown structure`；未阻塞本次基础启动，但应在奖励 UI 专项 PIE 前迁移或重保存该 WBP。

## 2026-08-08：旧版技能 WBP 不显示构筑摘要

- **现象**：奖励已经写入 `USkillComponent`，实际施放参数发生变化，但旧版技能 HUD 仍只显示技能名、等级和冷却，玩家无法确认当前 Modifier 层数。
- **根本原因**：旧 WBP 没有 Slice 6 新增的 `BuildSummary` 绑定；同时 HUD 不应复制 Resolver 的计算逻辑，否则容易出现“显示值”和“实际施放值”分叉。
- **最终修复**：新增 `GetSkillBuildSummary()` 与 `GetResolvedSkillSummary()`，统一读取 `FResolvedSkillSpec`；`UPlayerSkillWidget` 订阅 `OnSkillStateChanged`，有新控件时写入摘要，没有新控件时把相同内容追加到等级文本；原生白盒树增加独立摘要区域。
- **如何验证**：代码静态检查和 `git diff --check` 已通过；PIE 需在 UnrealBuildTool 日志备份权限恢复后，按构筑矩阵确认奖励选择后 Q/E 卡片立即更新。
- **涉及的 C++/UE 知识点**：组件作为运行时数据唯一来源、Delegate 驱动 HUD、可选 `BindWidget` 兼容旧控件树、Resolver 作为不可变展示模型、Timer 只承担冷却倒计时。

## 2026-08-07：技能 HUD 已创建但游戏画面不可见

- **现象**：PIE 日志出现 `Skill HUD tree`、`Skill HUD bound` 和 `Skill HUD created`，但画面只有左上角血条，底部没有 Q/E 技能栏。
- **错误信息或可观察行为**：技能 widget 对象创建成功；原技能栏使用一组固定偏移，卡片没有明确背景容器，无法从日志确认最终可见尺寸和图标资源状态。
- **最初判断**：不是玩家控制权或 `CreateWidget` 失败，而是 widget 创建后的布局/视觉呈现链路不完整。
- **根本原因**：技能 HUD 缺少明确的尺寸约束和可读背景，且没有记录图标加载结果；在白盒地面和不同 PIE 分辨率下，HUD 的可见性无法被可靠确认。
- **定位过程**：确认 `APlayerCharacter::CreateSkillWidget` 已走到 `AddToViewport`；确认 native widget tree 的 Canvas、QIcon、EIcon 均有效；对比健康 HUD 可见结果后，将问题范围缩小到技能栏的布局与资源呈现层。
- **最终修复**：技能栏改为底部中心安全区锚点 + 固定尺寸 `USizeBox` 卡片 + `UBorder` 高对比背景；图标、标题、等级、冷却条和状态文本均加入明确容器；补充图标加载和最终布局诊断日志；HUD 根节点保持 `HitTestInvisible`，避免阻断玩家输入。
- **如何验证**：冷编译 `RiverOfInkEditor`；PIE 后检查 `Skill HUD layout` 中 Dock/Icon 尺寸和纹理状态；按 Q/E，确认冷却条与 `CD x.xs` 文本更新，同时移动与普通攻击仍可响应。
- **下次如何更快发现**：创建 HUD 后同时记录 `IsInViewport`、根节点可见性、关键子控件期望尺寸和纹理是否加载，不只记录 UObject 创建成功。
- **涉及的 C++/UE 知识点**：`UUserWidget` 生命周期、`WidgetTree` 原生构建、`UCanvasPanelSlot` 锚点与安全区、`TObjectPtr` 的 GC 跟踪、Delegate + Timer 的 UI 刷新、`ESlateVisibility::HitTestInvisible` 的输入穿透语义。

## 2026-08-07：奖励卡片只有背景，内容不可见

- **现象**：Room Clear 后能看到两张奖励卡片背景，但 Icon、Title 和 Description 没有显示。
- **定位过程**：运行时确认 `Button_0/1` 已绑定到 `VerticalBox`，其子节点包含 `Icon`、`Title`、`Description`，但在 `AddToViewport` 前检查到内容期望尺寸为 `(0,0)`。
- **根本原因**：奖励控件在加入视口前执行数据写入和布局预计算，Slate 尚未完成构建，导致首帧内容没有有效布局尺寸；Blueprint 展示事件还在 C++ 样式写入之后执行，存在覆盖运行时值的风险。
- **最终修复**：先 `AddToViewport` 再执行 `SetupRewardOptions`；将 Blueprint 展示事件提前，C++ 绑定、图标、文本和样式作为最终值；图标/文本写入后执行 `ForceLayoutPrepass`；新增 `FocusFirstOption` 让 UIOnly 输入默认聚焦第一个按钮。
- **如何验证**：冷编译成功；自动 PIE 日志显示按钮内容期望尺寸约为 `(372,243)` 与 `(392,219)`，Icon 尺寸为 `(112,112)`，两个图标资源加载成功，奖励 UI 正常创建。
- **涉及的 C++/UE 知识点**：`UUserWidget` 的 `AddToViewport` 生命周期、Slate layout prepass、`GetDesiredSize` 的时机、`UButton::GetContent`、`SetKeyboardFocus` 与 `FInputModeUIOnly` 的初始焦点管理。

## 2026-08-07：奖励界面鼠标点击无响应

- **现象**：奖励卡片可见，鼠标光标也显示，但点击两张卡片没有触发奖励选择。
- **最初判断**：怀疑 `FInputModeUIOnly`、按钮 `OnClicked` 绑定或按钮禁用状态。
- **定位过程**：运行时日志确认两个按钮均 `Enabled=true`、`Focusable=true`、`ClickBound=true`；检查视口层级后发现血量 HUD 以 `Z=10` 添加，奖励 UI 使用默认 `Z=0`，且血量 HUD 的全屏根 Canvas 仍参与命中测试。
- **根本原因**：显示用的血量 HUD 覆盖在奖励按钮上方，Slate 命中测试先被血量 HUD 消耗，鼠标事件无法到达奖励卡片。
- **最终修复**：将血量 HUD 根 Canvas 设置为 `HitTestInvisible`；奖励弹窗以 `Z=100` 加入视口，保证其处于模态 UI 顶层；同时保持按钮启用状态并记录交互诊断日志。
- **如何验证**：冷编译成功；自动 PIE 后通过真实系统鼠标点击第一张卡片，日志出现 `Reward applied`、`Player selected reward`，奖励 UI 关闭并激活出口。
- **下次如何更快发现**：调试 UI 输入时同时记录每层 HUD 的 ZOrder、根节点命中测试可见性、按钮 `OnClicked` 是否绑定，不只检查输入模式。
- **涉及的 C++/UE 知识点**：Slate hit testing、`ESlateVisibility::HitTestInvisible`、`AddToViewport` 的 ZOrder、`FInputModeUIOnly` 与模态 UI 层栈。

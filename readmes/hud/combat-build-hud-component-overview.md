# 战斗内构筑 HUD 组件说明

> 当前实施分支：`codex/combat-build-hud`
>
> 适用范围：战斗中常驻的最近/上一构筑显示。
> 不包含：按 B 打开的详细构筑信息 HUD、暂停逻辑、UI Only 输入模式和详细说明文案。

## 1. 组件职责

战斗内构筑 HUD 只显示构筑历史的两个可见条目：

- 最近获得的构筑作为主条目，显示较大的 Icon；
- 上一次获得的构筑作为次条目，显示较小且降低透明度的 Icon；
- 没有构筑历史时整体隐藏；
- 获得新构筑时，主条目播放一次缩放/淡入反馈；
- B 键提示作为常驻提示显示，但当前 B 键只保留给后续详细信息 HUD 的兼容入口。

该组件不负责构筑效果计算、不负责奖励选择、不修改构筑数据，也不负责详细信息展示。构筑数据由 `USkillComponent` 持有，HUD 只订阅数据变化并刷新显示。

## 2. 当前蓝图层级与运行时层级

蓝图资产：

`/Game/Blueprint/GamePlay/MyCombatBuildHudWidget`

当前蓝图 Designer 中只保存一个根节点：

```text
MyCombatBuildHudWidget                         Widget Blueprint
└─ CombatBuildHudCanvas                        CanvasPanel
   └─ [运行时由 C++ 创建的全部子控件]
```

这不是遗漏。`MyCombatBuildHudWidget` 是 `UCombatBuildHudWidget` 的 Blueprint 子类，蓝图主要提供可编辑的根节点和外层布局属性；具体子控件由 `BuildDefaultWidgetTree()` 创建。因此，运行时层级大致如下：

```text
CombatBuildHudCanvas
└─ CombatBuildHudSize                         SizeBox，根面板尺寸
   └─ CombatBuildHudPanel                     Border，缺少面板贴图时的回退背景
      └─ CombatBuildHudOverlay                 Overlay
         ├─ CombatBuildHudPanelImage           宣纸/撕纸面板贴图
         ├─ CombatBuildContentRow               横向排列最近与上一构筑
         │  ├─ RecentBuildSlotSize              最近构筑槽位
         │  │  └─ RecentBuildSlot               Overlay
         │  │     ├─ RecentBuildFallback        透明回退层
         │  │     ├─ RecentBuildFeibai           飞白背景层
         │  │     ├─ RecentBuildWash             墨蓝背景层
         │  │     ├─ RecentBuildIcon             最近构筑贴图
         │  │     └─ RecentBuildIconPlaceholder  无贴图时的几何占位
         │  └─ PreviousBuildSlotSize            上一构筑槽位
         │     └─ PreviousBuildSlot              Overlay
         │        ├─ PreviousBuildFallback       透明回退层
         │        ├─ PreviousBuildIcon            上一构筑贴图
         │        └─ PreviousBuildIconPlaceholder 无贴图时的几何占位
         └─ BuildDetailsPromptRow                B 键帽与键位文字
```

最近构筑槽位的背景叠加顺序固定为：

```text
Panel → RecentFeibai → RecentWash → Build Icon → KeyCap → 键位文字
```

`RecentFeibai` 与 `RecentWash` 使用独立的 `USizeBox`，当前绘制尺寸为 `178×178`。这两个层不应烘焙进面板或构筑 Icon，否则无法独立调整透明度、尺寸和叠加关系。

## 3. 参与资产与职责

### 3.1 直接参与的蓝图

| 资产 | 作用 |
| --- | --- |
| `/Game/Blueprint/GamePlay/MyCombatBuildHudWidget` | 战斗内构筑 HUD 的 Blueprint 子类。当前保留 `CombatBuildHudCanvas` 根节点，内部显示树由 C++ 生成。 |
| `/Game/Blueprint/GamePlay/Player/BP_PlayerCharacter` | 实际玩家蓝图，继承 `APlayerCharacter`。玩家被本地控制后创建 HUD，并绑定 B 键。 |

`/Game/Blueprint/GamePlay/Player/PlayerCharacter` 是指向 `BP_PlayerCharacter` 的旧 Redirector，不是需要继续编辑的 HUD 蓝图。

### 3.2 并存但不属于构筑 HUD 的蓝图

以下 HUD 与构筑 HUD 同时存在，但不要把它们的布局修改混入本组件：

- `/Game/Blueprint/GameSystem/UI/Skill/WBP_SkillHUD`：Q/E 技能 HUD；
- `/Game/Blueprint/GamePlay/Player/WBP_PlayerHealthWidget`：生命值 HUD。

当前默认 GameMode 为 C++ 的 `ARiverOfInkGameMode`，它选择 `BP_PlayerCharacter` 作为默认玩家；HUD 蓝图的选择由 `APlayerCharacter` 的 `CombatBuildHudWidgetClass` 完成。

## 4. 运行时数据链路

```text
ARiverOfInkGameMode
└─ 生成 BP_PlayerCharacter
   └─ APlayerCharacter::SetupPlayerInputComponent
      ├─ CreateCombatBuildHudWidget()
      │  ├─ 创建 MyCombatBuildHudWidget
      │  ├─ AddToViewport(30)
      │  └─ InitializeForPlayer(this)
      └─ 绑定 B → ToggleCombatBuildDetails()

USkillComponent
└─ BuildHistory
   └─ OnBuildHistoryChanged
      └─ UCombatBuildHudWidget::RefreshBuildHistory()
```

HUD 创建路径位于：

- `Source/RiverOfInk/Script/Player/PlayerCharacter.cpp`
- `Source/RiverOfInk/Script/UI/CombatBuildHudWidget.cpp`

`InitializeForPlayer()` 会保存玩家和 `USkillComponent`，订阅 `OnBuildHistoryChanged`，然后立即刷新一次。HUD 销毁时解除订阅，避免 PIE 多次运行后重复回调。

## 5. Icon 与背景资源解析

### 5.1 构筑 Icon

Icon 的稳定键由构筑枚举数据解析，不读取展示文本。解析优先级为：

1. `ConfiguredBuildIcons` 中配置的 Icon；
2. `/Game/RawContent/UI/Reward/Textures/T_UI_Build_<Key>_Redrawn`；
3. 同名旧版 Icon；
4. 没有可用贴图时使用 `UCombatBuildIconPlaceholderWidget` 几何占位。

当前最终重绘 Icon 的资源目录为：

`/Game/RawContent/UI/Reward/Textures/`

代表性稳定键包括 `TwoStageArc`、`TwinSlash`、`Cooldown`、`Radius`、`ProjectileCount`、`InkGrenade`、`ExtraExplosion`、`ProjectileErase` 和 `ProjectileHoming`。

### 5.2 HUD 背景资源

| 用途 | UE 对象路径 |
| --- | --- |
| 面板 | `/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_Panel` |
| 最近构筑飞白层 | `/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_RecentFeibai` |
| 最近构筑墨蓝层 | `/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_RecentWash` |
| B 键帽底图 | `/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_KeyCap` |

背景贴图通过可重试的可选贴图加载路径解析。`RefreshBuildHistory()` 会再次刷新背景层，避免 Widget 首次重建早于新导入 UI 资源挂载时将空贴图结果永久缓存。

## 6. 布局与可调整项

外层 HUD 使用 `ApplyViewportLayout()` 根据当前 Game Viewport 计算像素位置，避免编辑器嵌入 PIE 和独立 PIE 的锚点解释差异导致整体偏移。

当前默认参数：

| 参数 | 默认值 | 调整位置 |
| --- | ---: | --- |
| `PanelWidth` | 500 | `MyCombatBuildHudWidget` 的 Appearance 分类 |
| `PanelHeight` | 250 | `MyCombatBuildHudWidget` 的 Appearance 分类 |
| `ViewportMargin.Right` | 150 | `MyCombatBuildHudWidget` 的 Appearance/Layout 分类 |
| `ViewportMargin.Bottom` | 300 | `MyCombatBuildHudWidget` 的 Appearance/Layout 分类 |
| 最近背景层尺寸 | 178×178 | `CombatBuildHudWidget.cpp` |
| 最近 Icon 尺寸 | 126×126 | `CombatBuildHudWidget.cpp` |
| 上一 Icon 尺寸 | 78×78 | `CombatBuildHudWidget.cpp` |
| 视口 ZOrder | 30 | `APlayerCharacter::CreateCombatBuildHudWidget()` |

只调整整体位置时，优先修改 `MyCombatBuildHudWidget` 的 `ViewportMargin`。如果要改变内部槽位比例、背景层尺寸、Icon 间距或 B 键提示位置，目前需要修改 C++；仅在 Blueprint Designer 中拖动根 Canvas 不会暴露这些运行时子控件。

## 7. 当前未实现的详细信息 HUD 边界

`ToggleBuildDetails()` 目前只是 B 键兼容入口，并记录延后日志。以下内容明确留给独立的详细构筑信息 HUD：

- 按 B 弹出独立详细信息面板；
- 暂停游戏；
- 切换为 UI Only 或等价的查看输入模式；
- 展示完整构筑分类、说明、等级和数值；
- 关闭面板并恢复游戏输入。

后续实现详细信息 HUD 时，不要把暂停、详细文案或大面板直接塞进 `UCombatBuildHudWidget`，应保持常驻 HUD 与详细 HUD 两个独立组件。

## 8. PIE 验证方式

建议在 `/Game/Level/TwoStageArcVFXTest` 中验证：

1. 启动 PIE，确认玩家进入测试场地；
2. 在 UE 控制台执行：

   ```text
   DebugSelectSpecificReward TwoStageArc 1
   DebugSelectSpecificReward Cooldown 1
   ```

3. 确认最近构筑与上一构筑按大小层级显示；
4. 确认飞白、墨蓝背景层位于最近 Icon 后方；
5. 确认新构筑出现时主条目有一次反馈动画；
6. 确认没有构筑历史时 HUD 隐藏；
7. 确认 B 键仅产生兼容日志，不误弹出详细信息 HUD。

重点日志包括：

- `Combat build HUD created`
- `Combat build HUD bound`
- `Combat build HUD subscribed to build-history changes`
- `Combat build HUD received build-history change`
- `Combat build HUD history refreshed`

如出现 `Combat build HUD asset unavailable`，先检查 UE 对象路径、同名 `.uasset` 是否存在，以及资源是否已被编辑器完成导入。

## 9. 跨设备交接约定

- 继续开发前先切换到远程 `codex/combat-build-hud` 分支；
- 调整常驻 HUD 时优先修改 `MyCombatBuildHudWidget` 的外层布局属性；
- 不要把详细构筑信息 HUD 的实现混入本组件；
- 不要通过旧 Redirector `PlayerCharacter.uasset` 修改玩家蓝图；
- 新增构筑 Icon 时沿用稳定构筑键和 `_Redrawn` 资源命名；
- 修改 C++ 后按项目约定关闭 UE Editor/Live Coding，再使用项目指定的 `Build.bat` 构建；
- PIE 验收后保留当前编辑器状态，不执行关机操作。

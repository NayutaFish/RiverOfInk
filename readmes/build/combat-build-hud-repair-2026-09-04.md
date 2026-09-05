# 战斗内构筑 HUD 修复记录

日期：2026-09-04

## 问题记录

- 图 1–4 是游戏内实际截图；图 5 是组件验收图，仅作为目标布局参考。
- HUD 与右边、下边的边距过大。
- F11 全屏/沉浸模式下，HUD 的位置计算错误，面板被推到屏幕右下边界之外。
- 最新构筑 icon 过大，超出飞白圆形背景。
- 飞白与水墨背景过大，超出对应 slot/panel。
- 上一个构筑 slot 没有显示飞白背景；验收效果要求每个可见构筑 slot 都有对应背景层。

## 根因

1. 旧布局通过渲染 viewport 的绝对尺寸计算位置，无法稳定适配 F11、DPI 和编辑器嵌入 viewport。
2. `SetPositionInViewport` 会重置 viewport anchors；旧调用顺序最后设置 position，导致右下锚点失效。
3. 贴图 brush 使用原始贴图尺寸作为 desired size，和 overlay/ScaleBox 的布局约束叠加后造成视觉溢出。
4. 背景层只挂在最新构筑 slot，上一个 slot 没有独立的飞白 image。

## 本次修复

- 将外层 HUD 改为右下锚定布局：先设置 size 和 position，最后设置右下 anchors/alignment；运行时将右下安全边距限制在 32px 以内，默认使用 20px。
- 保持面板逻辑尺寸为 500×250，收紧内容边距。
- 使用明确的绘制尺寸，避免 1024×1024 原始贴图参与 slot desired-size 计算：
  - 最新 slot 飞白/水墨背景：148×148px。
  - 最新 icon：76×76px。
  - 上一个 slot 独立飞白背景：104×104px，降低明度和透明度。
  - 上一个 icon：58×58px，透明度 0.82。
- 为上一个构筑 slot 增加飞白背景 image，并让背景与 icon 各自受 slot 尺寸约束。
- 将最新 icon 尺寸和上一个 icon 尺寸暴露为 Blueprint 参数，路径为 `Appearance → Build Icons`；当前默认值分别为 85px 和 68px。

## 恢复与验证

- 主要实现：[CombatBuildHudWidget.cpp](../../Source/RiverOfInk/Script/UI/CombatBuildHudWidget.cpp) 和 [CombatBuildHudWidget.h](../../Source/RiverOfInk/Script/UI/CombatBuildHudWidget.h)。
- 若工作记录丢失，先查看本记录，再从上述两个文件恢复布局常量和调用顺序。
- 构建时必须先关闭 UnrealEditor 和 Live Coding；构建完成后同时检查 `Saved/Logs/UBT-CodexHudRepair-2026.09.04.log` 与 UnrealBuildTool 日志中的 `Result:`、`Exception`、`Unhandled`、`Fatal`。
- 运行验证应覆盖普通窗口、F11/沉浸模式、只有一个构筑记录和至少两个构筑记录四种状态，并以图 5 的双 slot 背景关系为验收基准。

### 2026-09-04 构建结果

- 已关闭 UnrealEditor 和 Live Coding 后使用 UE 5.8 bundled `Build.bat` 完成 `RiverOfInkEditor Win64 Development` 构建。
- 结果：`Succeeded`；6 个动作完成。仅出现项目既有的 `APlayerCharacter::TakeDamage` 基类函数隐藏警告，没有本次 HUD 修改引入的编译错误。
- 项目构建日志：[UBT-CodexHudRepair-2026.09.04.log](../../Saved/Logs/UBT-CodexHudRepair-2026.09.04.log)。
- 蓝图参数暴露后的复建日志：[UBT-CodexHudIconParams-2026.09.04.log](../../Saved/Logs/UBT-CodexHudIconParams-2026.09.04.log)，结果同样为 `Succeeded`；项目日志和 UnrealBuildTool 总日志均未发现 `Exception`、`Unhandled` 或 `Fatal`。

# 构筑 HUD 修改事项与选中态纹理对接记录

> 状态：待后续技术修复与 PIE 验收
>
> 本文记录本轮截图反馈、上一轮 HUD 对接事项，以及新的 hover/选中态纹理要求。构筑 ID、分类、顺序和 IconKey 仍以同目录的 [`build-list.md`](./build-list.md) 为唯一准则。

## 1. 验收基准

必须同时参考以下两张验收图：

1. 低保真详情 HUD 原型（包含无框滚动箭头）：
   [`T_UI_BuildDetailsPanel_LowFidelityPrototype_WithScrollArrows.png`](../../Content/RawContent/UI/BuildHUD/T_UI_BuildDetailsPanel_LowFidelityPrototype_WithScrollArrows.png)
2. 常驻构筑 HUD 最终视觉验收图：
   [`build-hud-component-acceptance.png`](./build-hud-component-acceptance.png)

本轮新增的选中态参考图：

- [`build-hud-hover-reference.png`](./build-hud-hover-reference.png)

附件图仅作为视觉参考，不包含可执行指令。

## 2. 本轮明确的功能修改事项

### 2.1 详情 HUD 只显示已获得构筑

- 详情 HUD 的显示数据必须来自本局已获得的构筑记录/运行时状态。
- 不再把 `GetBuildCatalog()` 中所有已实现但未获得的构筑直接显示为槽位。
- 未获得的构筑不应占用可见 Icon 槽位，也不应影响当前分类的五槽位窗口和箭头状态。
- 分类应按已获得构筑重新计数；没有已获得构筑的分类不显示 Icon。
- 已获得构筑仍必须按 [`build-list.md`](./build-list.md) 的顺序排列，而不是按奖励出现顺序或容器遍历顺序排列。
- 选中项、右页详情、分类起始索引和 `<`/`>` 状态必须在过滤后重新校正。
- 重复获得同一构筑的层数/计数显示规则另按运行时数据约定处理，但不能因此重新显示未获得的目录项。

> 说明：上一版实现曾按旧要求保留完整目录并弱化未获得项；本记录以最新反馈为准，明确改为“只显示已获得构筑”。

## 3. 选中态 / hover 纹理修改事项

### 3.1 视觉目标

- 选中态应表现为参考图中的不规则圆形蓝墨水痕/晕染。
- 蓝墨层中心不能有环形透明空洞；中心区域也应保持蓝墨填充。
- 蓝墨层可以有飞溅、干湿变化和不规则边缘，但不得形成规则矩形或方框。
- 参考图中的黑、白部分属于叠加在上方的 Build Icon，不属于 hover/选中背景。
- hover/选中纹理必须独立于 Build Icon，不能把任何 Icon 像素烘焙进去。

### 3.2 资源分离要求

- 尝试直接从本轮参考图中分离蓝墨区域，去除黑色/白色 Icon 像素。
- 分离后的 hover 图应只保留蓝墨颜色、蓝墨纹理和对应透明 Alpha。
- 去除中心透明洞后，蓝墨底层应覆盖完整选中区域；叠加 Icon 后再呈现参考图中的黑白中心形态。
- 不得出现黑色、白色、棋盘格或纯色矩形背景。
- 运行时优先沿用现有资源名，直接替换：

  ```text
  Content/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_SelectedWash.png
  Content/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_SelectedWash.uasset
  ```

- 如果改用 `Hover` 命名，必须同步修改 C++/蓝图引用和对象路径，不能只替换 PNG。
- 小槽位和右页大预览应共用同一份透明蓝墨层，按容器缩放，不制作第二份带 Icon 的合成图。

### 3.3 运行时叠加顺序

```text
纸张/飞白面板
└─ 分类行或右页预览
   ├─ 独立蓝墨 hover/selected image
   ├─ Build Icon（黑白像素只来自 Icon）
   └─ 透明按钮命中区
```

- 按钮 Focused/Hovered/Pressed 状态不得绘制蓝色矩形。
- 选中态只显示独立圆形蓝墨层，不使用默认 Button focus brush 代替。

## 4. 上一轮尚未解决的排版事项

### 4.1 左页内容安全区

- 分类文字必须完全位于左页宣纸内容区内，不能落在面板透明边缘外。
- 文字必须不超出面板边缘；标题和描述也必须在右页内容区内自动换行。
- 左页五个分类行要对应面板背景的五个分隔区域，不能与间隔线重叠。
- 保留面板左下角留白，不用额外墨迹或装饰填满。

### 4.2 Icon 顺序与槽位

- 所有已获得 Icon 按 `build-list.md` 的 BuildId/SortOrder 顺序排布。
- 过滤未获得项后，剩余 Icon 应从该分类第一个槽位连续排列，不留下由未获得项造成的空洞。
- 每个分类最多显示五个已获得槽位。

### 4.3 箭头位置

- `<` 和 `>` 必须留在左页对应分类行内，不能越过中缝进入右页。
- 箭头不加框、不填充、不占用 Build Icon 槽位。
- 只有该分类过滤后的已获得数量大于五时显示箭头。
- 六项示例：

  ```text
  0,1,2,3,4,>
  <,1,2,3,4,5,>
  <,2,3,4,5,6
  ```

## 5. 既有美术约束继续有效

- 面板纸张/飞白、蓝墨选中层、Build Icon、文字和箭头必须分层，不能使用整张验收合成图作为运行时背景。
- 面板背景可以有验收图中的边缘墨迹、挂饰、印章和组间分隔线；槽位选中背景不得使用黑色。
- Icon 本体以 `build-list.md` 的 IconKey 为准，全部使用 `_Redrawn` 资源。
- `InkGrenade` 是投掷物式范围攻击，Icon 禁止使用炸弹、手雷或类似物体，应表现范围、落点和区域覆盖。
- Icon 的黑色只能来自 Icon 本体；蓝墨 hover 层不含黑色或白色 Icon 像素。
- 所有 PNG 必须保留真实透明 Alpha，不得使用棋盘格模拟透明。

## 6. 后续实施顺序

1. 修改详情 ViewModel/分类列表，只保留本局已获得构筑。
2. 依据 [`build-hud-hover-reference.png`](./build-hud-hover-reference.png) 分离并重做中心填充的蓝墨 hover 层。
3. 清除 Button Focused/Hovered 的矩形背景，确认圆形蓝墨层位于 Icon 后方。
4. 按面板五个分隔区域重排左页内容，修正文字、Icon 和箭头的页内边界。
5. 按 `build-list.md` 校正过滤后的 Icon 顺序和箭头窗口。
6. 在 1280×720、1920×1080 和宽屏窗口执行 PIE，至少检查：无构筑、单分类构筑、六项滚动、选中态、右页详情和文字不越界。

## 7. 完成定义

- 详情 HUD 只显示本局已获得构筑。
- 选中态为中心填充的独立圆形蓝墨，不出现蓝色矩形。
- 黑白 Icon 与蓝墨 hover 层完全分离。
- 文字、Icon、分隔线和箭头均位于正确页面和内容带内。
- Icon 顺序与 `build-list.md` 一致，箭头只在过滤后的数量超过五项时出现。
- 两张验收图和本轮 hover 参考图均通过组合画面复核。

# 构筑详情 HUD｜美术资源实施工单

> 状态：已整理为跨设备美术交接版，待另一设备执行  
> 适用组件：战斗中按 B 打开的开书式构筑详情 HUD  
> 对接阶段：技术实施方案 Slice 7｜美术资源替换与接入  
> 执行对象：另一位 Codex / 美术资产执行者

本工单只覆盖构筑详情 HUD 的美术资源整理、重绘、拆分、透明通道处理、UE 纹理导入和资产交付。美术执行者不修改 C++、蓝图逻辑、输入映射、暂停流程、构筑数据、HUD 锚点、窗口尺寸规则或详细信息交互逻辑；完成后由主 Codex 负责技术接入和 Slice 8 的 PIE 验收。

## 1. 验收参考与工作范围

### 1.1 两张验收图是强制基准

美术执行者必须同时打开并对照下面两张图，不能只参考其中一张，也不能用一张图替代另一张。两张图的职责不同：

1. [低保真原型图（无框箭头）](../../Content/RawContent/UI/BuildHUD/T_UI_BuildDetailsPanel_LowFidelityPrototype_WithScrollArrows_NoFrame.png)
   - 只验收结构和交互留位：开书比例、左页五个分类行、每行最多五个可见槽位、`<`/`>` 无框箭头、右页预览区、左下角留白。
   - 不把低保真图中的临时 Icon、文字或灰阶表现当作最终画风；箭头不能被制作成带框按钮。

2. [最终视觉验收图（无框箭头）](../../Content/RawContent/UI/BuildHUD/T_UI_BuildDetailsPanel_Separated_NoRightInkLabel_SimplifiedTexture_OpenLowerLeft_WithScrollArrows.png)
   - 只验收最终视觉方向：宣纸与飞白、独立圆形/近圆形墨蓝衬托、飞白墨印 Icon 风、简化纹理、右页留白和左下角开放构图。
   - 图中的文字、标题、Icon、蓝墨和箭头仅作为视觉位置/层级参考，必须按本工单拆分交付，不能整图烘焙成运行时面板。

验收优先级固定为：低保真图决定布局和控件占位，最终视觉图决定材质、色彩、墨迹和构图。任何只满足其中一张图的交付均不算完成。

### 1.2 参考图

- [低保真原型（无框箭头）](../../Content/RawContent/UI/BuildHUD/T_UI_BuildDetailsPanel_LowFidelityPrototype_WithScrollArrows_NoFrame.png)：只确认布局、五槽位滚动和无框箭头的构成。
- [视觉验收图（无框箭头）](../../Content/RawContent/UI/BuildHUD/T_UI_BuildDetailsPanel_Separated_NoRightInkLabel_SimplifiedTexture_OpenLowerLeft_WithScrollArrows.png)：确认宣纸/飞白、蓝墨、开书面板、选中构筑预览和整体视觉方向。
- [构筑详情 HUD 技术实施方案](./combat-build-details-hud-technical-implementation-plan.md)：确认运行时层级、Build list 来源、槽位规则和交互边界。
- `readmes/build/` 下最新 build list：确认本次必须覆盖的构筑稳定 ID、分类、排序和 IconKey。

参考图仅用于对齐，不得将整张验收图作为运行时唯一贴图交付。面板、Icon、文字、蓝墨衬托和箭头必须能独立替换或控制。

### 1.3 本工单包含

- 构筑详情 HUD 的开书式宣纸/纸页面板及其可复用背景层。
- 面板的飞白、纸张纹理、页边和中缝等背景层，按下表拆分交付。
- `readmes/build/` build list 中所有构筑 Icon 的重绘/生成和 `_Redrawn` 纹理交付。
- 选中构筑使用的独立墨蓝衬托层（如单独制作，必须与 Icon 分离）。
- 分类标识的透明图层（仅在当前项目没有可复用资源且技术对接确认需要时制作）。
- 所有 PNG 的 RGBA 透明通道、命名、UE `.uasset` 导入和路径对照表。
- 黑白轮廓验收图、分层验收图和开书面板组合验收图。

### 1.4 本工单不包含

- B 键打开/关闭、暂停游戏、UI Only、鼠标焦点、键盘焦点和 A/D 输入。
- 构筑目录、Build list、构筑计数、层数计算、滚动窗口或选中状态逻辑。
- `UCombatBuildDetailsWidget`、Widget Blueprint、Slate/UMG 控件、锚点、SafeZone、ScaleBox、ZOrder 和屏幕适配。
- 详细信息文案、属性数值和技能计算结果的运行时逻辑。
- 常驻战斗内构筑 HUD 的面板、最近/上一构筑语义和 B 键帽；该组件继续遵循现有的战斗内 HUD 美术工单。
- 把验收图整张导入并替代运行时的独立图层。

## 2. 总体美术方向

构筑详情 HUD 延续项目已确认的飞白墨印图标风和宣纸开书风。它是机制信息面板，不是技能 VFX 截图，也不是科技风菜单。

### 2.1 统一视觉语言

- 宣纸白、灰白、浅灰褐和低饱和墨色构成面板的主要关系。
- 飞白、断笔、干湿变化和少量墨点服务于轮廓与层次，不覆盖文字和 Icon 安全区。
- 墨蓝只作为选中构筑/最近焦点的独立衬托层；去掉墨蓝后，纸张、Icon 和布局仍然成立。
- 所有背景图层禁止使用纯黑或近黑的大块填充。Icon 区域的黑色只能来自 Build Icon 本体；背景不得用黑色底板帮助“托住”Icon。
- 面板外必须是真透明 alpha，不得使用黑色、深灰色、白色或棋盘格模拟透明。
- 左页底部左侧保留明显空白，只使用宣纸原有的浅纹理；不要在该区域增加墨团、山水、飞溅或装饰物。
- 视觉应轻量、可缩小、可复用；不要把开书面板做成遮挡战斗画面的厚重全屏窗口。

### 2.2 必须保留的结构感

- 横向开书/双页构图，左右页面有清晰但轻量的中缝关系。
- 左页承载分类列表和最多五个可见槽位；右页承载当前选中构筑的放大 Icon、标题和说明区域。
- 视觉验收图中的左右页比例、右页预览区域、左页分类分隔线和底部左侧留白作为基准。
- 右页选中构筑的墨蓝衬托必须是圆形或近圆形的独立水痕/薄墨轮廓，不得改成矩形、方块或黑色底板。

### 2.3 必须移除或禁止新增的内容

- 不恢复左侧纵向“墨迹图鉴”文字、图章或其黑色挂签。
- 不恢复右页蓝色纵向“右键弹幕”标签、文字和装饰墨条。
- 不在面板背景中烘焙标题、分类文字、Icon、构筑说明、属性数字、B 字符或 `<`/`>`。
- 不增加稀有度边框、科技面板、发光描边、全屏暗色遮罩或矩形黑底。
- 不为 `InkGrenade` 绘制炸弹、手雷、引信、爆炸弹体或类似物体；该 ID 表示投掷物式范围攻击/范围落点，应突出范围和落点关系。

## 3. 图层拆分与交付原则

运行时建议使用以下层级。若多个层合并交付，必须先得到主 Codex 确认；默认不得合并：

```text
L0  真透明区域
L1  宣纸底与浅纸纹理
L2  飞白、页边、中缝和轻量纸张装饰
L3  选中构筑的独立墨蓝圆形水痕/薄墨
L4  运行时 Build Icon
L5  运行时分类文字、标题、说明和属性文本
L6  运行时 < / > 无框箭头控件
```

### 3.1 面板与飞白

- L1/L2 可以作为一个面板背景资产，但必须不包含 Icon、文字、箭头、选中蓝墨和按钮底。
- 若飞白和纸张纹理可独立复用，优先分别交付；若不得不合并，文件名和说明中必须标明合并内容。
- 纸张边缘可以不规则，但要为两个页面的分类行、Icon 和右页预览保留安全区。
- 背景边缘只允许低对比灰褐/浅墨线条和透明过渡，不得形成纯黑外框或黑色矩形。

### 3.2 墨蓝衬托

- 选中衬托独立于面板、Icon 和文字，透明背景，整体轮廓为圆形或近圆形。
- 中心空缺比例参考最新视觉验收图：不能只剩一个薄环，需用同一水痕/薄墨笔触自然填充中心，使其在缩小后仍形成完整圆形焦点。
- 蓝墨只做低饱和焦点，不得成为整张页面底色，不得覆盖 Icon 主轮廓。
- 去掉该层后不得留下黑色、灰色或棋盘格残边。

### 3.3 箭头控件

- `<` 与 `>` 默认由 UMG/Slate 运行时绘制，工单不要求制作带框按钮图。
- 如技术对接需要独立箭头 glyph，必须提供无框、无填充、无底板的透明 PNG；只包含箭头笔触及少量飞白，不能包含方框、圆框或按钮阴影。
- 箭头视觉可以使用炭灰/浅墨线，焦点态可由技术层用墨蓝晕染或字形加深表现；不得通过加框解决焦点识别。
- 箭头不属于五个构筑槽位，也不得烘焙进面板或分类行背景。

## 4. 构筑 Icon 资源要求

### 4.1 清单来源与命名

`readmes/build/` 最新 build list 是唯一权威清单。美术执行前逐条读取其中的稳定 ID 和 IconKey，按以下格式交付：

```text
T_UI_Build_<IconKey>_Redrawn.png
T_UI_Build_<IconKey>_Redrawn.uasset
```

目标 UE 对象路径：

```text
/Game/RawContent/UI/Reward/Textures/T_UI_Build_<IconKey>_Redrawn
```

当前技术方案已确认的 IconKey 至少包括：

```text
TwoStageArc, TwinSlash, Cooldown, Radius,
ProjectileCount, InkGrenade, ExtraExplosion,
ProjectileErase, ProjectileHoming
```

以上列表不是对 build list 的替代。若最新 build list 新增、删除或重命名 ID，必须以 build list 为准，并在交付表中记录变更；不得因为当前目录中已有旧资源就擅自保留或新增构筑。

### 4.2 Icon 共通规范

- 1024 × 1024、1:1 画布，RGBA 透明 PNG，主体四周保留约 8%～12% 安全边距。
- 沿用飞白墨印图标风：弱具象、强轮廓、清晰负形、明显动势和组内差异。
- Icon 本体可使用墨黑、炭灰、宣纸白及必要的少量机制墨蓝；Icon 外部不得有黑色方底、白色方底、棋盘格或稀有度边框。
- 不使用文字、数字、字母、键位、百分比或技能名称。
- 同一组不得全部使用“圆环 + 中央旋涡”模板；差异必须来自轮廓、负形和动势，而不是只靠颜色。
- 缩小到左页槽位实际尺寸后，主轮廓仍需可读；放大到右页预览后，飞白不会抢过机制主体。

### 4.3 语义检查重点

执行者必须将每个 Icon 的画面语义与 build list 的功能说明核对，至少检查以下项目：

| IconKey | 视觉语义 | 禁止误读 |
| --- | --- | --- |
| `TwoStageArc` | 两道有先后关系、留有间隙的开放弧线 | 完整圆环、圆盘、单一旋涡、完整圆锥 |
| `TwinSlash` | 两道方向明确、相互错位的斜向切割笔触 | 单道大斜线、圆环、旋涡 |
| `Cooldown` | 有清晰缺口和回转节奏的断裂循环 | 钟表、数字、百分比、普通实心圆 |
| `Radius` | 范围扩张、外扩边界或覆盖半径 | 炸弹、具体数字、单纯圆点 |
| `ProjectileCount` | 多个投掷物/发射轨迹的数量增加关系 | 炸弹堆、文字数字、单一弹体 |
| `InkGrenade` | 投掷物式范围攻击、范围落点或扩散区域 | 炸弹、手雷、引信、爆炸弹体 |
| `ExtraExplosion` | 命中后额外扩散/二次冲击的范围关系 | 单一爆炸球、炸弹主体 |
| `ProjectileErase` | 投掷物被抹除、断开或消散的轨迹关系 | 橡皮擦、文字、单纯斜线 |
| `ProjectileHoming` | 投掷物转向目标的弧形追踪轨迹 | 准星 UI、数字、完整旋涡 |

若 build list 对某个 IconKey 的实际功能已有更新，以上表格只作为画面审查辅助，功能说明和稳定 ID 优先。

## 5. 面板资源清单与路径

### 5.1 必交素材

以下为详情 HUD 的建议交付命名。源图母版尺寸可由美术保持更高分辨率，但比例、透明边界和对象路径必须一致；如调整尺寸，交付表必须同步记录。

| 用途 | 源图建议尺寸 | 文件名 | UE 资产对象路径 | 说明 |
| --- | ---: | --- | --- | --- |
| 开书面板纸张/飞白 | 2048 × 1472 | `T_UI_BuildDetailsPanel_PaperFeibai.png` | `/Game/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_PaperFeibai` | 纸张、飞白、页边和中缝；真透明；无文字/Icon/箭头/蓝墨 |
| 选中构筑墨蓝衬托 | 1024 × 1024 | `T_UI_BuildDetailsPanel_SelectedWash.png` | `/Game/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_SelectedWash` | 圆形或近圆形独立蓝墨水痕；无 Icon、边框和黑底 |
| 分类标识（按需） | 256 × 256 | `T_UI_BuildDetailsPanel_Category_<Key>.png` | `/Game/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_Category_<Key>` | 仅透明分类符号；分类名称由运行时文字绘制 |
| `<` 箭头 glyph（按需） | 256 × 256 | `T_UI_BuildDetailsPanel_ArrowPrevious.png` | `/Game/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_ArrowPrevious` | 仅无框箭头笔触；默认可不制作，由技术层绘制 |
| `>` 箭头 glyph（按需） | 256 × 256 | `T_UI_BuildDetailsPanel_ArrowNext.png` | `/Game/RawContent/UI/BuildDetails/Textures/T_UI_BuildDetailsPanel_ArrowNext` | 仅无框箭头笔触；默认可不制作，由技术层绘制 |

### 5.2 构筑 Icon

每个 build list 条目交付一枚 1024 × 1024 的 `_Redrawn` Icon PNG 和同名 `.uasset`。文件名、IconKey 和对象路径按照第 4.1 节生成，不接受把多个构筑合并成一张图集后只交付一个对象路径。

### 5.3 不应制作的独立素材

- B 键帽、B 字符和暂停提示；它们属于常驻 HUD 或技术层，不属于详情面板。
- 详情标题、分类名称、构筑标题、说明、数值和键位文字贴图；这些内容由运行时文字绘制。
- 带框箭头、按钮底、鼠标图形和科技风焦点框。
- 覆盖整个屏幕的暗色遮罩。
- 包含左侧纵向“墨迹图鉴”或右侧“右键弹幕”文字/图章的复合面板图。

## 6. 导入与 UE 资产规范

- 所有源图必须为 RGBA PNG；透明区域是真 alpha，不得带黑边、白边、棋盘格或合成预览底色。
- Icon 使用 1:1 画布；面板纸张保持横向开书比例；衬托和分类符号保持 1:1 画布。
- UI 纹理使用项目现有 UI 纹理导入约定，至少确认 Alpha 保留、Texture Group 为 UI、颜色空间与现有 HUD 纹理一致。
- 每个 PNG 导入为同名 `.uasset`；在 Content Browser 中逐一确认对象路径与交付表一致。
- 技术层通过 `/Game/RawContent/UI/Reward/Textures/T_UI_Build_<Key>_Redrawn` 解析构筑 Icon；只存在源 PNG、不存在 UE 纹理对象视为未交付。
- 不覆盖旧版奖励 Icon，不删除 `TestIcons/Generated` 下的回退测试素材，不移动常驻 HUD 已有资源。
- 若生成的是分层源图或工作文件，保留图层命名并在交付说明中标注哪些层已导入、哪些层仅作为源文件保存。

## 7. 验收矩阵

### 7.1 单个资源

- [ ] PNG 与同名 `.uasset` 均存在，RGBA alpha 正确。
- [ ] 透明区域无黑底、白底、棋盘格、黑边或不应有的背景色。
- [ ] 面板背景不含文字、数字、Icon、B 字符、箭头或黑色矩形。
- [ ] 蓝墨衬托与飞白/纸张分离；衬托为圆形或近圆形，中心不会只剩空洞薄环。
- [ ] Icon 在实际槽位尺寸下仍可读，且不依赖蓝墨或背景颜色才能识别。
- [ ] `InkGrenade` 没有炸弹、手雷或类似物体，画面能表达范围攻击/范围落点。
- [ ] 左下角保持浅宣纸纹理和留白，不被墨迹或装饰抢占。

### 7.2 组内区分

- [ ] build list 中每个稳定 ID 都有且仅有一枚对应 `_Redrawn` Icon。
- [ ] `TwoStageArc`、`TwinSlash`、`Cooldown` 的轮廓、负形和动势可快速区分。
- [ ] 范围、投掷物数量、追踪、抹除、额外扩散等构筑之间不会因同一圆环/旋涡模板而混淆。
- [ ] 去掉墨蓝层后，黑白 Icon 仍保留机制差异。

### 7.3 组合画面

- [ ] 使用低保真图复核左页五行分类、最多五个可见槽位、`<`/`>` 位置和开书比例。
- [ ] `TotalCount > 5` 时，箭头作为独立无框控件出现；箭头不占用五个 Icon 槽位。
- [ ] `TotalCount <= 5` 时，不依赖空箭头图占位；显示/隐藏由技术层控制。
- [ ] 右页选中 Icon、蓝墨衬托、标题和说明可分别替换。
- [ ] 面板放入 1280×720、1920×1080 和更宽比例的安全区后，外部真透明，无黑色或白色矩形块。
- [ ] 不恢复已删除的左侧纵向标签、右页蓝色纵向标签、标题文字/图章或其他运行时文案。

## 8. 交付给主 Codex 的内容

1. `readmes/build/` build list 对应的全部 `_Redrawn` Icon PNG 和 UE `.uasset`。
2. 面板纸张/飞白、独立墨蓝衬托、分类符号和（如需要）无框箭头 glyph 的 PNG 与 `.uasset`。
3. 源文件或分层工作文件，并说明最终导出的图层关系。
4. PNG 文件名、源尺寸、UE 对象路径和用途对照表；特别标出缺失、改名或暂未制作的条目。
5. 三枚以上代表性 Icon 的黑白验收图、全部 Icon 并排验收图、拆分图层验收图，以及面板组合验收图。
6. 透明通道、Texture Group、颜色空间和 Content Browser 对象路径的检查结果。
7. 资产导入/保存后的提交记录；不要混入 C++、蓝图逻辑、Player Blueprint、输入、暂停或详细信息 HUD 代码改动。

## 9. 与原战斗内 HUD 美术工单的边界

- 本工单只服务于按 B 打开的构筑详情 HUD，不能替代 `readmes/hud/build-hud-art-implementation-work-order.md` 中的常驻战斗内构筑 HUD 工单。
- 常驻 HUD 的横向小面板、最近/上一构筑、B 键帽和相关 `RecentFeibai`/`RecentWash` 资源继续按原工单维护。
- 详情 HUD 可以复用已经导入的 `_Redrawn` Icon，但必须保持详情面板的背景、选中蓝墨、文字和箭头独立。
- 两个工单都遵守同一条语义约束：`InkGrenade` 是范围攻击型投掷物表现，禁止用炸弹或手雷作为 Icon。

## 10. 完成定义

本工单完成的标准是：最新 build list 中的全部构筑均有可解析的 `_Redrawn` Icon；面板纸张/飞白与墨蓝衬托分离且透明正确；箭头为独立无框控件或无框 glyph；已删除的标签、文字和图章没有被恢复；所有资源均按对象路径导入并通过单项、组内和组合验收，交付后可直接进入技术实施方案的 Slice 7 资源替换与 Slice 8 PIE 验收。

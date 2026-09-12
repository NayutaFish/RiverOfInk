# 主菜单 HUD 纹理更新记录

> 日期：2026-09-12
> 分支：`codex/mainmenu-hud-texture-reapply`

## 本次策略

- 保留本机当前版本的主菜单关卡、蓝图和四个 HUD 纹理 `.uasset` 作为资产基底。
- 不用另一台设备的二进制 `.uasset` 覆盖本机对象，避免导入元数据、引用或编辑器版本差异造成冲突。
- 将已交付的 PNG 源图重新导入到本机同路径 Texture2D 资产中；对象路径和现有运行时引用保持不变。
- 本次不修改 `MainMenu.umap`、`BP_MainMenuGameMode`、开场 Sequencer、场景摆放或演出逻辑。

## 重新导入清单

| 用途 | PNG 源图 | 保留的 UE 对象路径 |
| --- | --- | --- |
| 标题艺术字 | `Content/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Title_MoranKaifeng.png` | `/Game/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Title_MoranKaifeng` |
| 按钮默认态 | `Content/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Button_Normal.png` | `/Game/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Button_Normal` |
| 按钮聚焦态 | `Content/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Button_Focus.png` | `/Game/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_Button_Focus` |
| 左侧水墨 panel | `Content/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_InkPanel.png` | `/Game/RawContent/UI/MainMenuHUD/T_UI_MainMenuHUD_InkPanel` |

## 运行时接入

`Stage01IntroDirector` 继续通过原有对象路径加载标题、按钮默认/聚焦态和左侧水墨 panel。重新导入只更新 Texture2D 的源像素，不改变 HUD 的控件层级、锚点、点击事件或开场前的 HUD 渐隐逻辑。

## 执行结果

- 已使用 UE 5.8 的 Interchange 重新导入管理器完成四张 PNG 的原位导入并保存。
- 命令行验证结果：`Success - 0 error(s)`。
- 导入后的纹理尺寸分别为：标题 `1999 × 787`、两个按钮底图均为 `2172 × 724`、左侧水墨 panel 为 `941 × 1672`。
- 本次工作区只产生上述四个 HUD Texture2D、此说明和可复用的导入脚本差异；主菜单地图、GameMode 和开场演出资产均未被本操作改写。
## 验证标准

- 四个 PNG 均存在并成功原位重新导入。
- 四个 Texture2D 均可保存，UE 对象路径不变。
- 不产生地图、蓝图或开场演出资产的改动。
- PIE 中确认标题、三枚按钮与左侧水墨 panel 正常显示；点击“开始游戏”后仍先完成 HUD 渐隐，再进入既有开场运镜。

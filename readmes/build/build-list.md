# 当前构筑列表

> 构筑统计基线：本地 `main` 分支，源代码提交 `6f9bf6e90ed7a4244c08f601a7d4639f08c51afb`
>
> 更新时间：2026-09-02
>
> 本次 HUD 美术交付提交：`0c9959c`（完整提交号可由 Git 历史查看）
>
> 最近构筑衬托视觉修订提交：`bf73bf6`（圆形墨印与飞白层）
>
> 最近构筑背景分层修订提交：`68ade93`（蓝墨与飞白独立，背景层无黑色）
>
> 墨蓝层中心负形修订提交：`6c045bc`（复用现有笔触补足中心）
>
> 统计范围：`RoguelikeRewardManager::GenerateRewardOptions()` 当前奖励池中的技能构筑候选，不代表某个具体存档已经拥有的构筑。

## 统计结论

- 固定技能槽：2 个，Q 和 E。
- 当前可奖励构筑：10 个。
- 其中 Modifier 构筑：9 个；技能形态构筑：1 个。
- 唯一的非 `None` `ESkillModifierID`：8 个。
- `CooldownDown` 同时作用于 Q/E，因此构筑 ID 必须带技能前缀，不能只保存 Modifier 名称。
- 每次奖励只从当前合法候选中随机生成约 2～3 个选项。
- 当前已生成 9 个唯一视觉语义的重绘 PNG；`Cooldown` 一张图同时服务 Q/E 两个构筑。

## ID 约定

当前代码没有独立的 `EBuildID`。构筑由以下组合表示：

- Modifier 构筑：`技能 ID + ESkillModifierID`。
- 形态构筑：`技能 ID + EPlayerSkillForm`。
- 本文使用技能前缀构成稳定的复合 ID：`Q.<ModifierID>`、`E.<ModifierID>`、`E.TwoStageArc`。

基础技能：

- `Q` = `EPlayerSkillID::TripleProjectile`
- `E` = `EPlayerSkillID::CircularSlash`

## 当前可奖励构筑

| 构筑 ID | 原始代码 ID | 类型 | 功能理解 | 上限 / 前置 |
| --- | --- | --- | --- | --- |
| `Q.AddProjectile` | `TripleProjectile` + `AddProjectile` | Modifier | Q 增加 1 枚投射物；基础 3 枚，构筑最多增加到 7 枚。墨迹范围攻击形态同样使用该数量解析。 | 最多 3 层 |
| `Q.InkGrenade` | `TripleProjectile` + `InkGrenade` | Modifier | Q 投射物变为带延迟触发的墨迹范围攻击。`InkGrenade` 是代码标识，不代表真正的手雷或炸弹。 | 最多 1 层 |
| `Q.ExtraExplosion` | `TripleProjectile` + `ExtraExplosion` | Modifier | 每个 Q 墨迹范围攻击在原位置追加一次范围爆发。 | 最多 1 层；必须先拥有 `Q.InkGrenade` |
| `Q.CooldownDown` | `TripleProjectile` + `CooldownDown` | Modifier | Q 冷却时间每层减少 0.5 秒。 | 最多 4 层；最低 2.0 秒 |
| `Q.ProjectileHoming` | `TripleProjectile` + `ProjectileHoming` | Modifier | Q 投射物在标记有效期间修正飞行方向，追踪当前被标记的敌人。 | 最多 1 层 |
| `E.TwoStageArc` | `CircularSlash` + `TwoStageArc` | 技能形态 | E 变为两段近距离弧形斩击；第一段命中后解锁第二段，第二段需在约 2 秒内释放，未命中或超时则进入冷却。当前基准半径 200cm、半角 65°、每段伤害倍率 0.8。 | 形态最多 1 个 |
| `E.TwinSlash` | `CircularSlash` + `TwinSlash` | Modifier | E 每个攻击阶段增加一次独立伤害判定；当前两次判定均使用 0.65 倍伤害倍率。可与 `E.NullRing`、`E.TwoStageArc` 共存。 | 最多 1 层 |
| `E.NullRing` | `CircularSlash` + `NullRing` | Modifier | E 斩击区域会清除其中的敌方投射物；与两段形态组合时两段均保留该能力。 | 最多 1 层 |
| `E.RadiusUp` | `CircularSlash` + `RadiusUp` | Modifier | E 斩击半径每层增加 60；两段形态以其专用半径为基础继续增加。 | 最多 3 层；半径上限 440 |
| `E.CooldownDown` | `CircularSlash` + `CooldownDown` | Modifier | E 冷却时间每层减少 0.4 秒。 | 最多 3 层；最低 1.6 秒 |

## 图标与视觉语义约束

### `InkGrenade` 强制约束

`InkGrenade` 的视觉语义必须是“投射物触发的墨迹范围攻击”，不得按炸弹理解。

禁止使用以下元素作为该构筑的主要图形：

- 炸弹、手雷、引信、弹药或军事爆炸物轮廓；
- 明显的炸弹拟人化造型或类似爆炸物容器；
- 让玩家第一眼联想到传统投掷炸弹的图形符号。

建议突出：

- 墨迹投射物核心；
- 墨液向周围扩散的圆形范围；
- 墨水冲击波、涟漪、落点或区域标记；
- 延迟触发可以用收缩/展开的范围环表达，而不是用引信表达。

## Icon 资产交付

本地 `main` 分支已保存全量构筑 Icon 的透明 PNG 源图。文件位于：

`Content/RawContent/UI/Reward/Textures/`

| 构筑 ID | 资产文件 | 状态 |
| --- | --- | --- |
| `Q.AddProjectile` | `T_UI_Build_ProjectileCount_Redrawn.png` | ✅ 已生成；投射物数量语义 |
| `Q.InkGrenade` | `T_UI_Build_InkGrenade_Redrawn.png` | ✅ 已生成；墨迹范围攻击，禁止炸弹语义 |
| `Q.ExtraExplosion` | `T_UI_Build_ExtraExplosion_Redrawn.png` | ✅ 已生成；重复范围脉冲 |
| `Q.CooldownDown` | `T_UI_Build_Cooldown_Redrawn.png` | ✅ 已生成；与 E 共用 |
| `Q.ProjectileHoming` | `T_UI_Build_ProjectileHoming_Redrawn.png` | ✅ 已生成；弯曲轨迹追踪标记目标 |
| `E.TwoStageArc` | `T_UI_Build_TwoStageArc_Redrawn.png` | ✅ 已生成；两道有先后关系的宽弧斩 |
| `E.TwinSlash` | `T_UI_Build_TwinSlash_Redrawn.png` | ✅ 已生成；两道分离的斜向斩痕 |
| `E.NullRing` | `T_UI_Build_ProjectileErase_Redrawn.png` | ✅ 已生成；范围消除敌方投射物 |
| `E.RadiusUp` | `T_UI_Build_Radius_Redrawn.png` | ✅ 已生成；内外层级表达范围扩大 |
| `E.CooldownDown` | `T_UI_Build_Cooldown_Redrawn.png` | ✅ 已生成；与 Q 共用 |

统一规格：RGBA PNG、`1024 × 1024`、真实透明背景、无文字/数字/键位/稀有度边框/水印。

上述 9 枚 Icon 已同时导入为同名 `.uasset`，对象位于 `/Game/RawContent/UI/Reward/Textures/`；未覆盖现有未加 `_Redrawn` 后缀的旧图标。当前只完成素材与纹理资产交付，HUD/奖励界面的运行时绑定仍由负责技术对接的 Codex 后续处理。

## 战斗内 HUD 视觉资产交付

以下素材已按工单保存 PNG 源图并导入为同名 `.uasset`：

| 用途 | 源图 / 资产文件 | 尺寸 | UE 对象路径 | 状态 |
| --- | --- | ---: | --- | --- |
| HUD 纸张面板 | `T_UI_BuildHUD_Panel.png` / `.uasset` | `2048 × 1024` | `/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_Panel` | ✅ 已生成、已导入；横向撕纸/宣纸底，外部透明 |
| B 键帽底图 | `T_UI_BuildHUD_KeyCap.png` / `.uasset` | `256 × 256` | `/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_KeyCap` | ✅ 已生成、已导入；无 `B` 字符及其他文字 |
| 最近构筑墨蓝衬托 | `T_UI_BuildHUD_RecentWash.png` / `.uasset` | `1024 × 1024` | `/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_RecentWash` | ✅ 已再次修订并导入；沿用原有蓝墨笔触向中心补足，缩小中心负形；不含黑色、飞白、Icon 和边框 |
| 最近构筑飞白衬托 | `T_UI_BuildHUD_RecentFeibai.png` / `.uasset` | `1024 × 1024` | `/Game/RawContent/UI/BuildHUD/T_UI_BuildHUD_RecentFeibai` | ✅ 已从验收图分离并导入；只含白/灰白飞白，不含黑色、墨蓝、Icon 和边框 |

HUD 四项纹理均已核对为 RGBA、真实透明、`Texture Group=UI`、无 mip、sRGB 开启；没有把 Icon、槽位、键位文字或详细信息 HUD 烘焙进面板。最近构筑衬托现拆为独立的墨蓝层与飞白层；本次仅收窄墨蓝层中心负形并复用原有笔触，两个圆形衬托层均不提供黑色识别信息，icon 区域的黑色仅来自上层 build icon；图 3 按工单忽略。

验收预览已保存到当前目录：

- `build-icon-acceptance.png`：`TwoStageArc`、`TwinSlash`、`Cooldown` 三 Icon 黑白并排验收图。
- `build-hud-component-acceptance.png`：面板、最近构筑大图标、上一构筑小图标、独立飞白/墨蓝衬托和键帽的组件合成图；其中 `B` 是仅用于预览的叠加字，不属于键帽或面板源图。

## 不计入当前构筑列表的内容

- `Currency`、`Health`：即时资源/生命奖励，不是技能构筑。
- `ESkillUpgradeType::Mechanic`、`Cooldown`、`Damage`：旧版升级路径，当前新奖励池不直接生成。
- `EPlayerSkillForm::ThrownGrenade`、旧版 `TwinSlash`、旧版 `NullRing`：用于旧快照、旧蓝图和迁移兼容，不应作为新的独立构筑条目重复统计。
- `EPlayerSkillForm::TwoStageArc`：虽然是形态枚举，但目前确实由新奖励池作为 `E.TwoStageArc` 生成，因此计入当前列表。

## 后续更新规则

新增或移除构筑时同步更新：

1. “统计结论”中的数量；
2. “当前可奖励构筑”表格；
3. 上限、前置条件和可共存关系；
4. 图标语义与专用资源状态；
5. 顶部维护基线和更新时间。

如果底层枚举名称发生变化，应保留旧 ID 到新 ID 的迁移说明，避免 HUD、奖励数据和 RuntimeData 使用歧义。

## 当前权威代码入口

- `Source/RiverOfInk/Script/Player/Skill/PlayerSkillTypes.h`
- `Source/RiverOfInk/Script/Player/Skill/SkillComponent.cpp`
- `Source/RiverOfInk/Script/RoguelikeSystem/RoguelikeRewardManager.cpp`
- `Source/RiverOfInk/Script/Player/ProjectileTargetingComponent.cpp`

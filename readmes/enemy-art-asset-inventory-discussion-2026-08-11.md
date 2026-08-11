# RiverOfInk 敌人美术资源清单（组内讨论稿）

日期：2026-08-11

工程：`RiverOfInk`

用途：下一阶段 15–20 天开发的敌人美术资源讨论与需求收集
状态：讨论稿，暂不代表最终优先级、工期或采购结论

> [!summary]
> 敌人资源调整为 P0。第一目标不是制作大量高细节敌人，而是完成一套可复用的“物件 → 墨汁污染 → 墨水坑生成 → 战斗 → 消除/修复”表现模板。低级敌人通过模型细节、材质参数、动画幅度和状态分支降级获得。

## 1. 本轮共识

1. 敌人美术资源优先级高于 Shop/Trader 美术资源，Shop 暂列 P1。
2. 敌人以物件为主，优先选择工作量小、轮廓清晰、污染前后反差明显的物件。
3. 先做一套完整物件敌人，后续低级敌人尽量复用模型、材质、动画和 VFX。
4. 动画优先使用 Transform/材质参数/Niagara 表现，不以骨骼动画为前置条件。
5. Scale Pulse 可作为远程攻击的蓄力和发射表现；低级敌人通过降低幅度或关闭分支降级。
6. VFX 的常规顺序为：角色战斗 VFX → 战斗反馈 VFX → 敌人攻击 VFX。
7. “EnemySpawnPoint 墨水坑”“敌人从墨水坑出现”“污染物件生成污染怪物”是破格 P0，不能因为敌人攻击 VFX 后置而延后。

## 2. 当前已盘点资产与可复用基础

| 资产/家族 | 类型 | 当前用途/状态 | 本轮处理建议 |
| --- | --- | --- | --- |
| `Content/Blueprint/GamePlay/Enemy/EnemyTest1/BP_EnemyTest1` | 敌人 Blueprint | 已有敌人逻辑入口 | 作为第一套完整敌人的接入对象，核对最终状态事件 |
| `Content/RawContent/Character/Enemy/EnemyTest/Mesh/SM_TargetBaseMesh` | Static Mesh | 当前物件敌人白盒/原型基础 | 可用于碰撞和占位；是否作为正式敌人模型需组内确认 |
| `Content/RawContent/Character/Enemy/EnemyTest/Material/Blue` | 材质 | 敌人颜色变体 | 可作为低级敌人或状态颜色基础 |
| `Content/RawContent/Character/Enemy/EnemyTest/Material/DarkRed` | 材质 | 敌人颜色变体 | 可作为高威胁/攻击状态颜色基础 |
| `Content/RawContent/Character/Enemy/EnemyTest/Material/Red` | 材质 | 敌人颜色变体 | 可作为基础污染状态颜色基础 |
| `RawContent/VFX/NiagaraSystem/NS/FireBall` | VFX 家族 | 已有角色投射物相关资源 | 优先用于角色战斗 VFX 整理，也作为敌人远程攻击改编源候选 |
| `RawContent/VFX/NiagaraSystem/NS/PlayerCircleSlash` | VFX 家族 | 已有角色圆形斩击资源 | 优先完成角色技能表现；不建议马上拆成独立敌人 VFX |
| `RawContent/VFX/NiagaraSystem/NS/CommonSlash` | VFX 家族 | 已有通用斩击资源 | 可作为敌人近战攻击或命中反馈的改编源 |
| `RawContent/VFX/Test/NS/NS_HitSpark_Test` | 测试反馈 VFX | 命中反馈测试资源 | 可作为敌人受击反馈原型，需统一风格后再入演示路径 |
| `Content/Level/TestMap_0–3` | 测试地图 | 战斗验证场景 | 用于验证敌人生成、攻击预警和死亡/修复流程 |

当前未发现或未确认的敌人专用资源：正式物件敌人模型、污染材质、EnemySpawnPoint 墨水坑、生成演出、物件污染转化演出、敌人专用攻击 VFX、敌人死亡/净化 VFX、敌人专用动画集。

## 3. 新增资源清单（讨论版）

### 3.1 破格 P0：墨汁生成与污染叙事

| 编号 | 建议资源名/ID | 资源类型 | 最小交付 | 复用方式 | 依赖/待讨论 |
| --- | --- | --- | --- | --- | --- |
| ENM-SP-001 | `EnemySpawnPoint_InkPool` | Mesh + Material | 1 套墨水坑主体，支持尺寸和颜色参数 | 所有敌人刷新点共用 | 是否需要不同房间尺寸 |
| ENM-SP-002 | `NS_InkPool_Idle` | Niagara/VFX | 待机流动、涟漪、轻微溢出 | 通过参数切换强度 | 与地图灯光、地面材质协调 |
| ENM-SP-003 | `NS_InkPool_Activate` | Niagara/VFX | 刷新点激活/蓄力效果 | 所有敌人共用 | 是否与房间 Encounter Tier 联动 |
| ENM-SP-004 | `NS_Enemy_EmergeFromInk` | Niagara/VFX + Transform | 敌人从墨水坑升起、抖动、回弹 | 替换敌人 Mesh/颜色即可复用 | 生成时长和摄像机可读性 |
| ENM-SP-005 | `NS_Object_ContaminationStart` | Niagara/VFX | 墨汁从物件表面蔓延 | 作为污染转化通用层 | 污染起点、方向、是否需要局部遮罩 |
| ENM-SP-006 | `MI_Object_Contaminated` | Material/Material Instance | 干净→污染的覆盖率、流动、发光参数 | 适配不同物件材质 | 是否需要独立污染贴图 |
| ENM-SP-007 | `FX_ObjectToInkMonster` | Sequence/Transform/Material 参数组合 | 物件抖动、Scale Pulse、污染完成、进入战斗 | 第一套完整敌人为模板 | 是否需要保留短暂的干净物件轮廓 |
| ENM-SP-008 | `NS_Ink_Overflow` | Niagara/VFX | 污染完成时的飞溅、滴落、黑墨丝 | 复用于生成和死亡 | 可作为 P1 细节降级 |

### 3.2 P0：第一套完整物件敌人

| 编号 | 建议资源名/ID | 资源类型 | 最小交付 | 低级敌人降级方式 |
| --- | --- | --- | --- | --- |
| ENM-HERO-001 | `SM_EnemyObject_Hero` | Static Mesh | 1 个正式物件敌人模型，俯视角轮廓清晰 | 复用同一 Mesh 或减少材质/细节 |
| ENM-HERO-002 | `M_EnemyObject_Clean` | Material | 干净物件材质 | 直接复用 |
| ENM-HERO-003 | `MI_EnemyObject_Corrupted` | Material Instance | 墨汁覆盖、颜色、发光、污染程度参数 | 降低污染覆盖率和发光强度 |
| ENM-HERO-004 | `T_EnemyObject_Mask` | Texture/Mask，可选 | 污染区域或裂纹遮罩 | 没有时间时使用程序化噪声/材质参数 |
| ENM-HERO-005 | `Curve_Enemy_ScalePulse` | Curve/Transform 参数 | 攻击蓄力和发射时的 Scale Pulse | 降低幅度、缩短持续时间 |
| ENM-HERO-006 | `NS_Enemy_IdleInk` | Niagara/VFX | 待机墨汁流动、滴落或局部脉动 | 关闭或降低粒子数量 |
| ENM-HERO-007 | `NS_Enemy_AttackTelegraph` | Niagara/VFX | 攻击前摇、目标提示、发射前蓄力 | 低级敌人关闭或使用 Scale Pulse |
| ENM-HERO-008 | `NS_Enemy_Hit` | Niagara/VFX | 受击溅墨、短暂闪白/高亮 | 复用基础命中反馈 |
| ENM-HERO-009 | `NS_Enemy_DeathCleanse` | Niagara/VFX | 墨汁剥离、消散、净化或修复前置效果 | 降低持续时间和细节层数 |
| ENM-HERO-010 | `NS_Repair_Object` | Niagara/VFX | 敌人被消除后物件恢复/修复表现 | 可与房间清除效果复用 |

第一套完整敌人不要求骨骼动画。只要能稳定表现生成、移动、攻击前摇、攻击、受击、死亡/净化即可进入 PIE 验收。

### 3.3 P0：低级敌人派生资源

低级敌人优先通过参数派生，不单独制作完整资产包。

| 派生层级 | 建议内容 | 保留状态 | 关闭/降低内容 |
| --- | --- | --- | --- |
| 完整敌人 | Hero Mesh + 完整污染材质 + 完整 VFX | 生成、移动、远程攻击、受击、死亡 | 无 |
| 普通敌人 | 复用 Mesh，替换颜色/尺寸/污染参数 | 生成、移动、基础攻击、受击、死亡 | 特殊攻击、部分待机粒子 |
| 低级敌人 | 复用 Mesh 或简化 Mesh | 生成、移动、基础攻击、死亡 | 远程攻击、复杂蓄力、局部污染动画 |
| 高威胁/Boss 占位 | Hero Mesh 放大、独立材质参数和攻击预警 | 完整核心状态 | 独立高模、专属骨骼动画可后置 |

## 4. 动画与状态表现清单

动画不预设为骨骼动画；优先使用 Actor/Component Transform、材质参数和 Niagara 时间线组合。

| 状态/事件 | 视觉表现 | 推荐实现 | 是否第一套必须有 |
| --- | --- | --- | --- |
| SpawnStart | 墨水坑激活、涟漪增强 | Niagara 参数切换 | 是 |
| Emerge | 物件从墨水坑升起、缩放回弹 | Location/Scale/Rotation + 墨汁 VFX | 是 |
| Contamination | 墨汁覆盖物件，轮廓逐步改变 | Material 参数 + Ink Overflow | 是 |
| Idle | 轻微脉动、滴墨或呼吸感 | Scale Pulse + 少量 Niagara | 是 |
| Move | 物件移动时保持重量感/弹性 | 根节点位移、上下浮动或滚动 | 是 |
| AttackTelegraph | Scale Pulse、蓄力、目标提示 | Transform Curve + 预警 VFX | 是 |
| Attack | 发射或冲撞 | 复用角色攻击 VFX 的改编版 | 是 |
| Hit/Stagger | 闪白、溅墨、短暂失衡 | 材质参数 + Hit VFX | 是 |
| Death/Cleanse | 墨汁剥离、消散、恢复前置 | Niagara + Dissolve/Opacity | 是 |
| RepairComplete | 画面或物件恢复正常 | 修复 VFX + 状态切换 | 建议有 |

## 5. VFX 制作顺序

### P0-0：特殊生成/污染 VFX（并行优先）

墨水坑、敌人生成和物件污染是核心设定演出，独立于敌人攻击 VFX 的排期，优先建立可用版本。

### P0-A：角色战斗 VFX

- 整理 FireBall、PlayerCircleSlash、CommonSlash 等现有资源；
- 统一玩家攻击的颜色、亮度、尺寸和持续时间；
- 确保角色攻击能清晰命中敌人并触发反馈；
- 将稳定的投射物、斩击和命中资源登记为敌人攻击的改编源。

### P0-B：战斗反馈 VFX

- 敌人受击；
- 敌人死亡/净化；
- Pure Ink 获取；
- 房间清除；
- 物件修复完成。

### P0-C：敌人攻击 VFX

- 优先从角色 FireBall 或斩击 VFX 派生；
- 用污染色、材质反转、方向和拖尾区别敌我；
- 只在复用效果无法表达攻击类型时新增专用效果；
- 工期不足时，保留基础攻击预警和命中效果，减少复杂尾效。

## 6. 脚本状态机与美术资源的对应关系

第一套完整敌人使用完整状态机，低级敌人通过能力配置关闭分支。美术资源应绑定状态事件，而不是绑定具体敌人类。

| 状态事件 | 脚本触发点 | 美术响应 |
| --- | --- | --- |
| `OnSpawnStarted` | SpawnPoint 开始生成 | 墨水坑激活 |
| `OnEmergenceStarted` | 敌人开始出现 | 生成动画、升起、回弹 |
| `OnContaminationCompleted` | 污染转化完成 | 污染材质达到最终状态、进入战斗 |
| `OnAttackTelegraph` | 进入攻击前摇 | Scale Pulse、目标提示、蓄力 VFX |
| `OnAttackReleased` | 攻击释放 | 攻击 VFX 发射/播放 |
| `OnHit` | 受到有效攻击 | 闪白、溅墨、受击反馈 |
| `OnDeath` | 敌人被消除 | 墨汁剥离、消散、净化 |
| `OnRepairCompleted` | 物件恢复完成 | 修复完成 VFX、恢复干净材质 |

建议使用“能力开关/状态掩码/敌人配置”控制低级敌人，而不是复制一份新的状态机和一套新的美术资源。

## 7. 组内需要讨论并最终确认的事项

- [ ] 第一套完整物件敌人的具体物件类型；选择标准为轮廓清晰、污染前后反差大、制作成本可控。
- [ ] `SM_TargetBaseMesh` 是继续作为原型，还是升级为第一套正式敌人模型。
- [ ] 污染视觉主语言：表面覆盖、裂纹、触手/突起、滴落，或其中的组合。
- [ ] 墨水坑是否只作为 SpawnPoint，还是同时代表污染区域和房间危险区域。
- [ ] 敌人生成动画时长、镜头可读性和是否允许生成期间被攻击。
- [ ] 远程攻击的 Scale Pulse 幅度、蓄力时长和目标提示形式。
- [ ] 第一套完整敌人采用纯 Static Mesh，还是为 Hero 敌人预留骨骼/简单绑定。
- [ ] 低级敌人允许关闭哪些状态分支，以及哪些反馈必须保留。
- [ ] 角色攻击 VFX 中优先改编哪一组作为敌人远程攻击源。
- [ ] 敌人死亡是“墨汁消散后物件恢复”，还是“先消散再进入修复流程”。
- [ ] 外部模型、贴图和 VFX 的来源、许可证和可商用范围。

## 8. 初步验收标准（讨论版）

1. PIE 中可以完整演示：墨水坑激活 → 敌人生成/污染 → 进入战斗 → 攻击前摇 → 玩家命中 → 敌人净化/物件修复。
2. 俯视镜头下，玩家能快速区分干净物件、污染物件和已进入攻击状态的敌人。
3. 第一套敌人不依赖骨骼动画也能完成完整状态表现。
4. 普通和低级敌人可以通过配置关闭技能分支，不需要复制状态机或重新制作整套资源。
5. 敌人攻击 VFX 至少有一套可运行的角色 VFX 改编版本；敌人生成和污染 VFX 必须是独立可识别的表现。
6. 关键演示路径中不出现 Engine DefaultTexture、缺失贴图或未处理的白盒敌人占位。

## 9. 后续整合字段

本清单后续并入总资产清单时，建议为每项补充以下字段：

`AssetId / 最终优先级 / 资源类型 / 目标敌人层级 / 来源 / 授权状态 / 负责人 / 预估工时 / 依赖 / 截止日 / 当前状态 / 引用路径 / PIE 验收结果`

最终优先级暂不在本文件冻结，待组内确定第一套物件敌人和视觉方向后，再与环境、角色战斗 VFX、反馈 VFX、Shop/Trader 资源统一排序。

相关文档：[美术资产总清单与 15–20 天计划](art-asset-inventory-and-15-20day-plan-2026-08-11.md)

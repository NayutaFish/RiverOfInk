# Result HUD 技术实施方案

状态：设计交付，尚未实现。代码核查基线为本地 `main` 的 `828e269d`；实施前需核对届时工作区与分支差异。本方案不要求合入其他分支已有的 Result Widget 实现。

## 1. 验收基准与确定范围

美术基准为用户确认的[市井宣纸版验收图](result-hud-reference/result-hud-final-acceptance.png)，拆分规范见[美术实施方案](result-hud-art-implementation-plan.md)。它是独立全屏 Result UI，背景完全覆盖视口，界面逻辑独立于 Build HUD 和其他局内 HUD。

用户已确定的行为：

1. 统计按击败敌人数、累计伤害、获得纯墨、购买次数顺序逐项出现。
2. 每项出现时播放一次 pulse，最大 Scale 暂定1.5。
3. 增益历程只显示前六项，按原始顺序从左到右，到行末换行；对应验收图为三列两行。
4. 每个 Build icon 出现时播放一次渐显。

本方案建议将统计和增益串行播放，数值立即显示最终值，使用 `1.0→1.5→1.0` 单次pulse。下文时长、曲线和按钮启用时机是首版可调默认值，不属于已确认的美术细节。

## 2. 现有能力与待补接口

| 模块 | 已核查能力 | 实施工作 |
| --- | --- | --- |
| `USettlementDataRecorder` | 五项累计指标及按选卡先后追加的 `RewardPicks`；新会话/新局清空 | 增加本局统计终止边界与不可变快照，避免结束后持续累计 |
| `URoguelikeRunFlowSubsystem` | 通关设置Victory并进入Result；有RestartRun、LoadPreparationRoom | 补齐死亡到Defeat/Result；统一胜负结束与幂等保护 |
| `ARiverOfInkGameMode` | 监听RunState变化；目前未处理Result界面 | 将Result通知转交界面控制者，协调局内HUD退出 |
| `APlayerCharacter` | 发布 `FPlayerDiedEvent` | 由流程侧订阅并判定所属玩家/有效局次；UI不直接决定输赢 |
| `FBuildPresentationResolver` | 正式图标键、目录、显示定义 | 增加结算记录适配；不能直接把RewardId当作BuildId |

建议新增 `FRoguelikeRunResultSnapshot`、`URoguelikeRunResultWidget` 和独立的统计项/增益项组件，命名在实施时按现有目录约定微调。快照存于GameInstance生命周期的流程数据中；PlayerController或专用界面控制者管理创建、显示、输入和销毁。RunFlow保持流程职责，Widget只读取快照并发送按钮意图。

“独立界面”首版采用完全不透明的全屏UMG页面，不要求额外加载结算地图。局内HUD经各自关闭/停用入口退出，不采用全局RemoveAllWidgets，以免误删过场或加载界面。

## 3. 数据契约

| 画面字段 | 现有来源 | 口径 |
| --- | --- | --- |
| 击败敌人 | GetNonPlayerDeathCount | 非玩家死亡计数，含精英；当前并非严格最后一击归属统计 |
| 其中精英 | GetEliteEnemyDeathCount | 总击败的子集，不相加；随第一项显示 |
| 累计伤害 | GetTotalDamageDealtToNonPlayer | 当前玩家来源伤害事件累计；验收须核对代理攻击和过量伤害口径 |
| 获得纯墨 | GetTotalShopCurrencyGained | 累计收入，不是余额，不把退款视为新收入 |
| 购买次数 | GetShopPurchaseCount | 已完成购买事件数 |
| 增益历程 | GetRewardPicks | 按事件完成顺序追加的清场选卡记录，不代表所有来源的最终构筑 |

快照建议包含：局次标识RunId、Outcome、五个原始指标、完整RewardPicks副本、快照完成标志。UI派生VisiblePicks，不修改或截断原始记录。

`VisiblePicks = RewardPicks[0 : min(6, Count)]`。保持重复项，不排序、不合并、不取最近六条、不补位到六条；不提供滚动、翻页或第七项入口。超过六项可将现有说明替换为“按获取顺序展示前6项”，避免增加新的区域。0项显示“本局未获得增益”，1～5项保持原槽位顺序，空槽不显示占位图。

成绩使用本地化数字格式；累计伤害显示取整后的值但保留原始精度。0必须真实显示，不能用示例值替代。正常数据使用千位分隔；超长数值以保留1.5倍动效空间为前提调整基础字号，不擅自截断。

`FSettlementRewardPick.StackCount` 当前是本次增加量，显示为 `+N`，不能当作当前总层数。Currency/Health若出现在原始记录中，不静默跳过后再取六项：保留原顺序并使用对应类型图标；层数不适用时不显示虚假的“+1层”，扩展记录以提供正确数值/单位。

## 4. 图标解析与记录补充

现有RewardId示例为 `Modifier.ProjectileHoming`，而Build展示系统按技能类别和升级定义构造BuildId，两个命名空间不同。当前UpgradeSkill记录只有奖励类型和SkillID，不能唯一识别UpgradeType；UI不能通过中文标题猜图标。

建议在清场奖励完成时同步记录完整的类型信息：RewardType、SkillID、ModifierID、UpgradeType、TargetSkillForm，以及StackDelta；或通过共享解析器生成规范BuildId/IconKey并持久化到记录。优先从实际选中项生成，兼容保留原有RewardId、DisplayName和StackCount读取接口。

将类型信息适配给共享 `FBuildPresentationResolver`，复用正式图标目录与对象路径。资源缺失时使用明确的占位图标且保留文本，记录一次诊断信息；不使用不相关图标掩盖失败。载入最多六项所需图标后再启动入场序列，避免异步纹理突然出现绕过渐显。数据展示不得依赖结算时仍存活的PlayerCharacter。

现有 `FinishRewardSelection` 先广播OnRewardApplied，再发布选卡记录事件。实施时必须保证若该次选择导致结束，快照包含最后一次奖励：调整为奖励生效、完成记录、再发布允许推进的完成通知，并检查原订阅者行为。

## 5. 结束、冻结与重开边界

建议统一 `FinalizeRun(Outcome)` 入口：

1. 校验当前局处于可结束状态和RunId，重复调用直接忽略；已有Outcome不被后到事件覆盖。
2. 终止新的战斗输入、房间出口推进和奖励/商店交互；完成已经确认成功的当前奖励或购买事务。
3. 在本次事件链收尾后固定统计快照；为事件记录增加局次/采集状态限制，结束后的残留弹丸、怪物死亡和迟到奖励不再计入。若延迟到帧末捕获，必须同时阻止新事务，不能简单延时后任意读取。
4. 存储Outcome与快照，进入Result并通知界面；快照就绪是播放的前置条件。
5. 关闭局内HUD、切换UI输入、冻结战斗模拟，显示全屏结果页。

统计开始于新局初始化，Result与准备阶段不继续采集上局。新局成功建立前保留旧快照；同帧胜负均触发时建议采用首次有效FinalizeRun结果，不重复创建页面。胜负优先级若后续另有玩法要求，仅修改流程侧。

再来一局调用 `RestartRun()`，返回准备区调用 `LoadPreparationRoom()`。调用期间锁定两个按钮并显示加载反馈。现有BeginNewRun会在地图请求之前清理部分数据，实施时需增加预校验与失败恢复：先确认地图、房间序列和依赖，再提交重置与跳转；同步失败恢复Result可操作状态与旧快照。异步TravelFailure同样需要错误处理与可重试出口，不能永远停留在黑屏/按钮禁用状态。

成功跳转后清理旧Widget和动画回调，在目的地图恢复游戏输入、正确HUD与模拟状态；新局重置玩家、经济、流程与记录器。失败重开尤其要核对角色重生和生命值。返回准备区不自动重播上一局结算。

## 6. UMG结构与适配

建议层级：ResultRoot → PaperBackground / TownDecorations / ContentRoot；ContentRoot包含Title、StatsColumn、RewardGrid、ActionBar。PaperBackground必须全视口不透明；装饰不接收鼠标事件。

以1920×1080逻辑画布对齐验收图。背景自适应全视口，主要内容等比缩放，左右装饰保持宽高比并锚定底边。RewardGrid使用固定三列，`Row = Index / 3`、`Column = Index % 3`；不用随屏宽自动换列的WrapBox。

统计行和六个网格槽提前占位，仅调透明度/RenderTransform，不使用Collapsed导致逐项出现时重新排版。所有动画以组件局部坐标运行；pulse仅作用于数值文本容器，不放大整个左栏。最大Scale必须在安全区内，父层裁切策略与布局留白共同保证完整显示。

## 7. 动效时序与参数

### 7.1 播放顺序

背景与标题入场 → 击败敌人数pulse → 累计伤害pulse → 获得纯墨pulse → 购买次数pulse → 增益01渐显 → 02 → 03 → 04 → 05 → 06 → 操作区启用。

其中精英作为第一组附注随击败信息显示，不单独pulse。已经出现的数据保留至结束。图标按行优先排序，不采用列优先；每项图标、序号、名称与增量同步渐显，视觉主目标为Build icon。

### 7.2 单次pulse

| 局部时间 | 数值Scale | 该统计行透明度 | 行为 |
| --- | --- | --- | --- |
| 0.00s | 1.0 | 0 | 填入最终数值，开始显示 |
| 0.08s | 插值放大中 | 1 | 标签及附注完成短渐显 |
| 0.16s | 1.5 | 1 | 数值达到唯一峰值 |
| 0.40s | 1.0 | 1 | 回落完成并保持 |

数值Pivot为中心(0.5,0.5)，采用平滑EaseInOut曲线，禁止弹性曲线额外超过1.5。一次完整放大回落算一次pulse，无循环、反复弹跳、计数滚动或闪白。每项完成后间隔0.08s再开始下一项。

### 7.3 图标渐显与总时长

单项RenderOpacity从0到1，默认0.24s；Scale固定1.0，渐显结束后间隔0.06s再显示下一项。六项串行、不重叠，总计约1.74s。不足六项按实际数量播放，不等待空槽。

背景与标题入场默认0.25s，四项统计总计1.84s，统计与增益之间间隔0.12s，操作区最后渐显0.20s。六项完整流程约4.15s。时长仅为首次调试起点，保持用户指定顺序与1.5峰值。

建议配置项：`IntroDuration=0.25`、`StatPulseMaxScale=1.5`、`StatFadeDuration=0.08`、`StatPulsePeakTime=0.16`、`StatPulseDuration=0.40`、`StatGap=0.08`、`RewardStartDelay=0.12`、`RewardFadeDuration=0.24`、`RewardGap=0.06`、`ActionsFadeDuration=0.20`。

### 7.4 播放管理

使用单一序列控制器管理Intro、Stats、Rewards、Ready、Leaving状态；每次播放绑定RunId与播放令牌。Widget重复Construct、输入焦点变化或资源完成回调不得重复播放同一局。退出时取消动画与异步回调，使所有数值Scale恢复1、清理旧实例引用。

世界冻结期间使用经验证可在暂停时推进的UMG动画，或基于UI实时时钟的驱动；不用依赖受世界暂停影响的普通Timer驱动关键时序。低帧率按绝对已过时间推进，不按帧累加固定步长；最终强制写入Scale=1与Opacity=1。

首版不提供点击空白跳过动效；动作区在Ready前不可交互，进入Ready后设置可见键盘/手柄焦点。进入页面前清理战斗按键状态，必须由新的按下事件触发操作，长按/旧按键释放不能误触重开。若后续增加跳过功能，跳过与确认操作必须使用两次独立输入。

## 8. 实施顺序与验证

1. 核对最新代码与其他分支改动；补齐记录类型信息、图标映射、胜负统一结束和快照边界。
2. 制作纯数据驱动的全屏白盒，验证按钮出口、失败重开与HUD互斥。
3. 对接已验收美术拆分件与正式Build icon，校验静态完成态。
4. 接入四次pulse与最多六次渐显，测试暂停、低帧率及中途退出。
5. 执行与改动相称的构建和PIE验收，保存截图、动效录屏和结果记录。

| 测试 | 通过条件 |
| --- | --- |
| 通关/死亡 | 均只进入一次Result，胜负标题正确，战斗不继续，局内HUD不可见/不可交互 |
| 四项统计 | 顺序严格一致；每项仅一次pulse，峰值1.5、结束1.0；精英附注不多触发一次 |
| 0/1/3/4/6/7+条记录 | 只取最早六条，左到右再换行；不足不补；无第七项入口 |
| 重复增益 | 保留原顺序和每次增量，不合并、不显示错误总层数 |
| 最后一次奖励 | 结束快照包含已确认奖励；迟到事件不污染结果或下一局 |
| 图标与类型 | Modifier、ChangeSkillForm、GainSkill、UpgradeSkill映射正确；Currency/Health有合理回退 |
| 暂停/15fps/60fps | 动画仍结束，顺序不乱、无永久缩放残留，Ready必达 |
| 最大数值与DPI | 1.5峰值不裁切、不碰撞；最终值准确可读 |
| 重复打开/中途销毁 | 无重复绑定、动画重播或旧回调访问新页面 |
| 快速双击/按键长按 | 每次离开只发一次请求，入场无战斗输入穿透 |
| 地图缺失/旅行失败 | 保留旧结果，可见错误与重试/返回出口，不永久禁用按钮 |
| 重开及返回后再开局 | 角色、输入、世界模拟、局内HUD恢复正确；统计从零开始 |

数据截取、排序、映射与幂等结束可做小范围自动化检查；视觉与动效必须在PIE录屏中核验，不能仅靠编译通过。

如实施涉及C++构建，遵守仓库AGENTS：先关闭UnrealEditor与Live Coding；使用实际UE路径对应的Build.bat和项目文件；同时仅运行一个构建，等待完整退出；检查UBT日志Result/Exception/Unhandled/Fatal。示例历史路径不能直接照抄。本轮只交付文档，不启动构建。

# Bug Log

## 2026-08-09：敌人状态机 ESM-2/3 远程站位与死亡收尾

- **现象**：敌人 Chase 只按近战距离追击；玩家目标失效后状态仍可能停留在 Chase/Attack；死亡事件直接销毁敌人，掉落与房间计数没有独立的死亡生命周期入口。
- **根因**：距离策略没有区分移动型攻击区域；状态机缺少 TargetLost 和 Dead 状态；`Die()` 同时承担标记、事件广播和销毁，后续加入死亡表现或掉落时容易重复调用。
- **修复**：远程敌人以 `MinimumAttackRange/MaximumAttackRange` 形成距离带，超出外圈靠近、进入内圈后退、带内停留并攻击；新增运行时创建的 `EnemyState_TargetLost` 与 `EnemyState_Dead`；`OnDead → GenerateDropOnDead → OnEnemyDeath/EventBus` 只执行一次，Pure Ink 仍由经济子系统消费 `FNonPlayerDiedEvent`；默认延迟销毁 0.5 秒并在 EndPlay 清理计时器。
- **验证计划**：PIE 中分别配置远程敌人的 `bAttackAreaIsMelee=false`、非零 `AttackAreaSpeed` 和距离带，观察远程站位/投射物方向；使用 `bDebugKillAllEnemiesOnNextTick` 检查 `Dead → OnDead → EnemyDrop → 延迟销毁`，再让玩家失效检查 `TargetLost`。
- **涉及知识点**：状态组件运行时注册、状态转换中的一次性副作用、Timer 生命周期清理、状态策略与共享状态拆分、UE `TObjectPtr`/Actor 销毁前事件顺序。

## 2026-08-09：敌人状态机 ESM-0 生命周期修复

- **现象**：敌人状态组件同时具备原生 Component Tick 与 `AEnemyBase::Tick → CurrentState::Update` 两条更新路径；重复进入 Attack 后，旧的攻击阶段标记可能导致前摇期间移动；攻击区域类缺失时，敌人可能卡在 Attack 状态。
- **根因**：状态组件的 Tick 开关没有由状态机统一管理；`UEnemyState_Attack::bAttackExecuted` 只在构造时初始化；`ExecuteAttack()` 对缺失类直接返回，没有恢复路径；玩家 Pawn 只在 Idle 初始化时缓存。
- **修复**：由 `AEnemyBase` 统一驱动状态更新并关闭状态组件原生 Tick；每次进入 Attack 重置阶段标记；攻击类缺失或生成失败时记录警告并回到 Chase；新增目标刷新、死亡目标失效处理、状态切换日志和 EndPlay 计时器清理。ESM-1 默认攻击区域改为近战基线（`bAttackAreaIsMelee=true`、速度为 0），远程蓝图后续覆盖配置。
- **PIE 验证**：`TestMap_1` 中观察到 `None → EnemyState_Idle → EnemyState_Chase → EnemyState_Attack`，攻击日志显示 `Style=Melee` 与 `AttackArea` 生成，后摇后回到 Chase；目标失效后回到 Idle。完整 UBT `Result: Succeeded`、`BUILD_EXIT=0`。
- **涉及知识点**：`UActorComponent` 生命周期、`TObjectPtr` 的 `.Get()`、Timer/Delegate 在状态退出与 Actor EndPlay 时清理、状态驱动器唯一性，以及状态对象与攻击区域对象的职责分离。

## 2026-08-09：投掷技能穿过白盒地面

- **现象**：`Ink Grenade` 的球体下沉穿过 `TestMap_1/Floor_0`；未直接撞到敌人时，随后在地面下引信爆炸，表现为“只有直接触碰敌人才生效”。
- **根因**：`APlayerSkill_ThrownGrenade::SweepForImpact` 原先使用 `SweepSingleByObjectType`。该 UE API 使用默认 `WorldStatic` 查询通道，而白盒 `Floor_0` 的 `BodyInstance` 是 `WorldDynamic + BlockAllDynamic`，因此对象类型列表即使加入 `WorldDynamic` 也不会得到地面命中。
- **修复**：地面碰撞改用 `ECC_Visibility` 球体扫掠；敌人仍用 `EnemyHitbox (GameTraceChannel2)` 对象查询，两个结果按 `FHitResult::Time` 取最近命中。新增 `LogSkill: ThrownGrenade impact` 记录 Actor、Component、球心和接触点。
- **PIE 验证**：`TestMap_1` 中连续投掷三枚手雷，日志确认 `Actor=Floor_0`、`Location.Z=31.500`、`ImpactPoint.Z=-0.500`，随后立即输出 `ThrownGrenade explosion`；球心保持在地面上方约一个碰撞半径，不再下沉。历史同一构筑日志已确认爆炸范围可命中 `Hits=1/2`，本修复未改变范围查询。
- **构建**：以正常权限运行 bundled .NET 10 的完整 UBT，`DOTNET_EXIT=0`、`Result: Succeeded`、5/5 actions；仅保留既有 `TakeDamage` C4263/C4264 警告及缺失 NETFXSDK 目录警告。沙箱内曾因 UBT 日志轮转目录不可写触发 `UnauthorizedAccessException`，旧日志已备份并改用正常权限验证，未修改系统 ACL、EventLog 或 UE bundled .NET。

## 2026-08-09：P1 运行数据与 Pure Ink PIE 验证

- **P1.4 跨场景数据流**：奖励选择后，`SkillComponent` 先记录 `TwinSlash` 的 Modifier（`Stack=0->1`、解析后 `HitCount=2`），RunFlow 随后进入 `RewardApplied → Exiting`。玩家进入出口后推进到 `TestMap_2`，日志确认 `Player runtime data captured`（`E=TwinSlash x1`）以及新 Pawn 的 `Player runtime data applied`（`E=TwinSlash x1`）；技能 HUD 同步解析为 `Build: Twin Slash x1`。这证明 Capture → map travel → Apply → HUD 的链路已在 PIE 跑通。
- **P1.5 Pure Ink**：敌人死亡事件发放 `Amount=1`，Combat 房间结算发放固定 `Amount=20`，同一房间最终余额为 `21`；房间结算仍以 RunFlow 的 `MajorStage=0 RoomIndex=0` 记录，重复事件不会重复结算。
- **P1.5 商店基础实现**：新增 `ERoguelikeRoomType::Shop`、`ARoguelikeShopManager` 与一次性购买校验（余额、房间类型、同一物品重复购买）；默认提供恢复生命和即时额外奖励两个固定报价。`DemoRoomManager` 在非 Combat 房间跳过敌人刷怪。当前默认地图序列仍全为 Combat，因此商店购买事务尚未在默认 PIE 序列中触发，待 Shop Room 接入关卡配置后再做购买点击验证。
- **PIE 驱动辅助**：远程 Slate 当前只点击到编辑器视口图片，无法稳定把 Q/E 或奖励按钮事件路由到 PIE；为完成可重复验证，`ARiverOfInkPlayerController` 增加了开发调试属性 `bDebugKillAllEnemiesOnNextTick` / `bDebugSelectFirstRewardAfterKill`。属性只在 PIE 中手动置 `true` 时生效，执行一次后自动复位；不改变正常游戏默认行为。
- **构建结果**：关闭 Editor 后以正常用户权限执行完整 `Build.bat RiverOfInkEditor Win64 Development -Project=... -WaitMutex -NoHotReload`，`Result: Succeeded`、`Exit code 0`、5/5 actions；本轮没有 dotnet 弹窗、JIT 弹窗或 managed exception。保留的 C4263/C4264 是旧版 `TakeDamage` 隐藏基类虚函数警告，未在本切片修改。

## 2026-08-09：P0 奖励控件迁移与 dotnet 构建复核

- **P0 修复**：旧奖励 WBP 的 C++ 模块引用已在 `Config/DefaultEngine.ini` 增加 `/Script/Test_GamePlay → /Script/RiverOfInk` 的 `PackageRedirect`；原有的 WBP 包路径和 `RoguelikeRewardWidget` 类重定向继续保留。此前编辑器 UMG 检查确认 `WBP_RoguelikeReward` 已解析为 `/Script/RiverOfInk.RoguelikeRewardWidget`，两张卡片的 `Button_0/1`、`Icon_0/1`、`Title_0/1`、`Description_0/1` 绑定存在，旧控件树可走 C++ fallback。
- **WBP 重保存结果**：回滚误删的三个蓝图后，仅针对目标包再次执行单包 `ResavePackages`。日志报告 `1/1 packages were resaved`、`0/1 packages were deleted`，资产哈希已变化；目标包内已不再出现 `/Script/Test_GamePlay`，并保留 `/Script/RiverOfInk.RoguelikeRewardWidget`。命令行总退出码仍为 1（同次加载摘要有 3 个错误/7 个警告），所以不能把它描述为“全量无错误”，但 P0.2 的旧模块引用迁移已落盘。
- **技能构筑矩阵（P1.1）**：修复 Q `CooldownDown` 的最大层数遗漏（4 层，对应 4.0s → 2.0s）；奖励池现在能生成 Quick Reload，且仍受上限和前置条件约束。静态审计确认 `InkGrenade → AddProjectile → ExtraExplosion`、`TwinSlash` 延迟二段/角度/单次冷却、`NullRing` 施放时消除标记敌弹以及 `TwinSlash + NullRing` 组合均走同一 `ResolveSkillSpec`。本轮未新增测试专用 Gameplay 代码。
- **构建复核**：在 Codex 沙箱身份下，UBT 在备份 `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`/`Trace.uba` 时记录了 `System.UnauthorizedAccessException`（`EpicGames.Core.Log.BackupLogFile → FileSystem.MoveFile`），这正是 `0xe0434352` 的 CLR 未处理异常根因证据；不是 Gameplay C++ 或 bundled runtime 损坏。以正常用户权限再次执行完整 `Build.bat RiverOfInkEditor Win64 Development -Project=... -WaitMutex -NoHotReload`，结果为 `EXIT_CODE=0`、UBT `Result: Succeeded`、`5/5 actions`，无 dotnet 弹窗、JIT 或 managed exception。未修改系统 ACL、EventLog 或 UE bundled .NET。
- **编辑器/PIE 复核**：正常权限启动编辑器并开放 MCP；准备房间跳过刷怪，出口推进到 `TestMap_1` Combat Room，日志确认 `RoomState Initializing → Entering → Ready → Combat`、Q 基线 `ProjectileCount=3/Payload=NormalProjectile/Cooldown=4.00`、E 基线 `HitCount=1/Radius=260/Cooldown=3.00`。本轮未通过自动 Slate 输入完成击杀，因此没有把“奖励点击、变体施放”虚报为已通过；仍需在真实可聚焦游戏视口手工完成 Room Clear 及奖励选择。
- **当前结论**：根因是沙箱运行身份无法访问 UBT 的用户日志轮转目录；通过正常用户权限执行构建可稳定通过。两个同参数 UBT 进程不是本轮正常用户构建的必然行为，当前成功验证只观察到单条 UBT 链。为防复发，Codex 的 UE 构建应运行在正常用户/主机权限环境，避免在沙箱内直接轮转 `%LOCALAPPDATA%` 日志。

## 2026-08-08：UE5.8 bundled dotnet/UBT 异常诊断

- **现象**：Codex 构建期间偶发 `dotnet.exe - 应用程序错误`，异常码为 `0xe0434352`；有时同时出现 Visual Studio JIT Debugger 提示。
- **环境检查**：`EventLog` 当前为 `Running/Automatic`，`Get-WinEvent -LogName Application` 可用；未发现本轮构建对应的 `.NET Runtime`、`Application Error` 或 `dotnet.exe` WER 事件，因此没有修改 EventLog 注册表。
- **历史根因线索**：2026-08-08 18:37 的 UBT 日志显示 `Intermediate/Build/Win64/x64/RiverOfInkEditor/Development/Makefile.bin` 读取失败，异常为 `System.ArgumentException`，调用栈为 `BinaryArchiveReader.ReadBulkData → ReadPrimitiveArray → ReadLogEvent → TargetMakefile.Load`。UBT 捕获该异常后重建 makefile，最终 `Result: Succeeded`；这是可恢复的 makefile 缓存读取错误，不等同于 dotnet 未处理异常。
- **并发调查**：人工执行 `Build.bat` 时只观察到 `cmd.exe → bundled dotnet.exe → UnrealBuildTool.dll`，没有 UBT 自调用或第二个同参数 dotnet。UE 的 `Build.bat` 本身包含临时锁；两个同参数 dotnet 更可能来自 Codex/编辑器/直接 UBT 的重叠调用。历史 UBT 日志还记录过 `BuildException: Unable to build while Live Coding is active`。
- **bundled .NET 检查**：UE 5.8 bundled .NET 10.0.203 SDK、10.0.7 runtime、`hostfxr.dll`、`hostpolicy.dll` 和 `runtimeconfig.json` 均存在；`dotnet --info`、`--list-sdks`、`--list-runtimes` 均正常。`Build.bat` 会设置 `DOTNET_ROOT` 并将 bundled runtime 放到 PATH 首位，同时关闭 multilevel lookup。
- **修复/预防**：未修改 Gameplay C++、EventLog 或 UE bundled .NET；现有 makefile 已可正常读取。新增项目级 `AGENTS.md`，要求关闭 Editor/Live Coding、串行调用唯一的 `Build.bat`，并检查完整 UBT 日志。
- **验证**：人工命令 `RiverOfInkEditor Win64 Development -Project=... -WaitMutex -NoHotReload` 返回 `EXIT_CODE=0`、`Result: Succeeded`；冷编译记录为 17/17 actions 成功。本轮未出现 dotnet 弹窗，构建后无残留 `UnrealEditor`/`UnrealBuildTool`/`dotnet` 进程。
- **剩余不确定性**：没有捕获到 0xe0434352 对应的 dump 或 WER 事件，故不能证明该弹窗的唯一根因。若再次出现，应保留弹窗期间的进程树和 dotnet dump，再区分并发调用、makefile 竞争与其他 CLR 异常。
- **涉及知识点**：批处理锁与进程树、.NET hostfxr/runtimeconfig 加载、UBT TargetMakefile 二进制缓存、Live Coding 与外部构建互斥、可恢复异常与未处理异常的区别。

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

# Enemy Health Widget 第一阶段：功能技术实施工单（修订版）

> 本文基于原始 Enemy Health Widget Phase 1 工单修订。
> 原始附件保留不变；本文是后续实施与验收的准据版本。

## 1. 审核结论与实施门槛

方案整体方向通过，但必须先落实以下五项硬约束：

```text
Enemy Health 事件契约
Normal / Elite 类型来源
Screen-space WidgetComponent Billboard
Damage Trail 状态算法
Widget 初始化与委托生命周期
```

在这五项没有明确实现前，不得进入最终美术阶段，也不得以“暂时轮询”替代事件契约。

本阶段允许的 Gameplay 修改仅限于：

```text
读取 Enemy Health 的 Getter
必要的 HealthChanged / Damage / Heal 通知
满足本阶段验收所需的最小 Heal API
```

不得重构敌人伤害计算、AI、状态机、死亡掉落、移动或攻击流程。

---

## 2. Goal

为 Normal Enemy 与 Elite Enemy 建立一个统一、可复用、实例独立的 Enemy Health Widget 系统，完成：

```text
真实 Enemy Health 数据绑定
Widget Component 头顶定位
Screen-space / View-facing 显示
Normal / Elite 类型接口
Current Health
Recent Damage Trail
连续伤害
Healing
死亡归零
多敌人实例独立运行
```

本阶段只使用 Placeholder UI，不导入最终敌人血条美术。

---

## 3. 当前项目事实（实施前已确认）

### 3.1 Enemy Health 数据源

当前敌人不是使用 `UHealthComponent`，而是由 `AEnemyBase` 直接持有：

```text
AEnemyBase::MaxHealth
AEnemyBase::CurrentHealth
AEnemyBase::TakeDamage()
AEnemyBase::Die()
```

对应文件：

```text
Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyBase.h
Source/RiverOfInk/Script/Enemy/EnemyBase/EnemyBase.cpp
```

当前 `AEnemyBase` 已有：

```text
OnTakeDirectDamage
OnHardBreak
OnDead
OnEnemyDeath
```

但 `OnTakeDirectDamage` 在当前血量扣除前广播，不能直接作为 Damage Trail 的数据源。

### 3.2 Player Health 不作为 Enemy Health 数据源

`UHealthComponent` 与 `UPlayerHealthWidget` 当前用于玩家 Health。Enemy Health Phase 1 不得给敌人再挂一套独立 `UHealthComponent`，避免出现两套 Gameplay Health。

玩家血条的比例计算可以参考，但不能复用玩家全局 EventBus 作为敌人血条事件源，否则多个敌人会互相更新。

### 3.3 Normal / Elite 资产状态

当前 Content 中已确认有：

```text
Content/Blueprint/GamePlay/Enemy/EnemyTest1/BP_EnemyTest1.uasset
```

不能假设 Normal 与 Elite 已经有完整独立 Blueprint。实施前必须确认或补充两个 Placeholder 测试实例，并明确它们的类型来源。

---

## 4. Phase 1 / Phase 2 边界

### Phase 1：本工单

只完成：

```text
数据事件
Widget Component
Screen-space Billboard
统一 Widget
Current Health
Empty Health
Recent Damage Trail
Healing 行为
Death 归零
Normal / Elite 尺寸接口
多实例测试
```

### Phase 2：后续美术工单

Phase 1 完成前禁止实施：

```text
最终 Normal Frame
最终 Elite Frame
宣纸纹理
墨锋与端点
Elite 朱色标识
正式材质与水墨贴图
最终像素级尺寸调整
```

---

## 5. Enemy Health 事件契约（P0）

### 5.1 最小类型

在 Gameplay 层建立最小变化原因枚举，不创建 `UIEnemyType`：

```cpp
UENUM(BlueprintType)
enum class EEnemyHealthChangeReason : uint8
{
    Initialize,
    Damage,
    Heal,
    Death,
    ExternalSet
};
```

### 5.2 事件数据

`AEnemyBase` 应提供统一的 HealthChanged 事件，至少包含：

```text
PreviousHealth
CurrentHealth
MaxHealth
ChangeReason
```

同时提供：

```text
GetCurrentHealth()
GetMaxHealth()
```

Widget 不得直接读取其他 Actor 的 protected 字段。

### 5.3 广播时机

必须遵守：

```text
BeginPlay 初始化后       → Initialize
Damage 结算并扣血后      → Damage
Heal 修改血量后          → Heal
直接死亡归零后           → Death
其他外部设置完成后       → ExternalSet
```

事件必须在 `CurrentHealth` 已经是最终值之后广播。

致死伤害不能产生两次相同血量事件：

```text
TakeDamage 将血量扣到 0
→ 广播一次 Damage
→ Die() 只处理死亡状态与生命周期
```

如果 `Die()` 被独立调用，则由 `Die()` 广播一次 Death。

### 5.4 现有事件的限制

不得将以下事件直接作为血条当前值更新源：

```text
OnTakeDirectDamage
OnHardBreak
```

它们分别表示攻击输入与硬值反应，不代表最终 Health 已经完成结算。

---

## 6. Healing 范围

当前 `AEnemyBase` 没有 `Heal()`。由于本阶段验收明确包含 Healing，允许增加最小接口：

```text
Heal(float Amount)
```

接口规则：

```text
CurrentHealth = Clamp(CurrentHealth + Amount, 0, MaxHealth)
```

只在实际发生增长时广播 `Heal` 事件。

不得在本阶段加入：

```text
治疗特效
Recovery Trail
绿色条
Healing Flash
Healing Fade
```

如果后续确认敌人没有任何治疗来源，仍保留该最小接口用于功能验证与后续 Gameplay 接入，不得用 Widget 内部伪造 Health。

---

## 7. Enemy 类型接口

在 Gameplay 层使用一个最小类型字段或现有敌人子类来区分：

```text
Normal
Elite
```

优先级：

```text
复用现有 Enemy 类型字段
>
在 AEnemyBase 增加 EEnemyRank
>
使用明确的 Normal / Elite Blueprint 默认值
```

禁止创建：

```text
UIEnemyType
EnemyHealthOnlyType
```

Phase 1 中类型只影响：

```text
Style Identifier
Placeholder Desired Size
可选 Padding
```

不影响伤害、AI 或死亡逻辑。

必须准备至少：

```text
1 个 Normal Placeholder Enemy
1 个 Elite Placeholder Enemy
```

---

## 8. Enemy Widget Component

### 8.1 所属关系

每个 `AEnemyBase` 实例拥有自己的：

```text
EnemyHealthWidgetComponent
```

推荐挂载：

```text
AEnemyBase Root
└─ SceneComponent_HealthAnchor
   └─ EnemyHealthWidgetComponent
```

也可以直接将 Widget Component 挂到 Root，但必须集中管理高度偏移。

### 8.2 必须使用 Screen-space

Widget Component 必须显式配置：

```text
WidgetSpace = Screen
```

并根据项目实际渲染结果配置：

```text
Pivot = (0.5, 1.0)
DrawSize 或 DrawAtDesiredSize
WidgetClass
Visibility
```

禁止每帧执行：

```text
Get Player Camera
Find Look At Rotation
Set World Rotation
```

`Screen` WidgetSpace 由 UE 负责视图朝向，满足：

```text
敌人移动 → Widget 跟随世界位置
敌人旋转 → Widget 不跟随 Enemy Facing
Camera 改变 → Widget 保持正面可读
```

### 8.3 高度偏移

增加集中管理的参数：

```text
HealthWidgetHeightOffset
```

禁止在多个 Enemy Blueprint 中散落不同的硬编码 Z 偏移。

“始终显示”的定义是：

```text
有效敌人存在时，不因受伤、脱战或治疗主动隐藏
```

不要求：

```text
穿墙显示
超出屏幕显示
无限距离显示
```

如果 Screen-space Widget 不满足遮挡需求，必须在实施报告中明确记录，而不是改回逐帧 LookAt。

---

## 9. 通用 Widget 结构

只创建一个通用 Widget：

```text
WBP_EnemyHealth
└─ SizeBox / Root
   └─ Overlay_Health
      ├─ EmptyHealth
      ├─ RecentDamage
      ├─ CurrentHealth
      └─ FramePlaceholder
```

推荐使用三个重叠的 `ProgressBar`：

```text
EmptyHealth       = 1.0
RecentDamage      = DamageGhostRatio
CurrentHealth     = CurrentHealthRatio
```

绘制顺序必须是：

```text
Empty 在底层
RecentDamage 在中层
CurrentHealth 在顶层
```

当前阶段允许使用：

```text
Rectangle
Border
Solid Color
FramePlaceholder
```

当前阶段不显示：

```text
HP 数字
Current / Max
Enemy Name
Level
Normal / Elite 文字
```

---

## 10. Widget 初始化与生命周期（P0）

`WBP_EnemyHealth` 必须提供：

```text
InitializeForEnemy(AEnemyBase* InEnemy)
```

初始化顺序：

```text
Enemy BeginPlay
→ Widget Component 创建 UserWidget
→ InitializeForEnemy
→ 读取 Current / Max
→ 初始化 CurrentHealthRatio
→ 初始化 DamageGhostRatio = CurrentHealthRatio
→ 绑定 Enemy HealthChanged
→ 设置 Visible
```

必须避免新敌人短暂显示：

```text
0% HP
错误的默认 100% 条
未绑定 Owner 的旧状态
```

销毁顺序：

```text
NativeDestruct / Owner EndPlay
→ 解绑 HealthChanged
→ 清除 Damage Trail Timer
→ 清理 Enemy 引用
```

不得使用静态变量、共享 Widget 状态或共享运行时 Material Instance 保存：

```text
CurrentHealthRatio
DamageGhostRatio
DamageTimer
```

---

## 11. Health 显示规则

### 11.1 Current Health

```text
CurrentHealthRatio = Clamp(CurrentHealth / MaxHealth, 0, 1)
```

受伤时 Current Health 必须同一事件立即更新。

禁止对 Current Health 使用：

```text
Lerp
Delay
缓慢缩短
```

### 11.2 Empty Health

```text
EmptyHealth.Percent = 1.0
```

不单独计算 LostHealth，损失区域由 Current/Recent 层覆盖关系形成。

### 11.3 始终可见

初始化完成后：

```text
Widget Visibility = Visible
```

本阶段不得增加：

```text
ShowOnDamage
HideAfterDelay
Combat State Visibility
Fade Out
Distance Fade
```

---

## 12. Recent Damage Trail 状态机

Widget 每个实例独立维护：

```text
CurrentHealthRatio
DamageGhostRatio
DamageTrailHoldTime
DamageTrailCollapseDuration
DamageTrailTimer / CollapseState
```

### 12.1 初始状态

```text
CurrentHealthRatio = Current / Max
DamageGhostRatio = CurrentHealthRatio
RecentDamage 隐藏或无差异显示
```

### 12.2 单次伤害

例如：

```text
100% → 70%
```

事件处理：

```text
OldRatio = PreviousHealth / Max
NewRatio = CurrentHealth / Max
CurrentHealthRatio = NewRatio
DamageGhostRatio = Max(DamageGhostRatio, OldRatio, NewRatio)
重置 Damage Hold
```

表现结果：

```text
Current 立即到 70%
Ghost 暂时保持在 100%
```

### 12.3 Trail 回收

Hold 完成后：

```text
DamageGhostRatio → CurrentHealthRatio
```

回收方式只能是：

```text
Progress / 长度缩短
```

禁止把 `RenderOpacity 1 → 0` 作为主要消失逻辑。

回收完成后：

```text
DamageGhostRatio = CurrentHealthRatio
RecentDamage 隐藏或无差异显示
```

### 12.4 连续伤害

例如：

```text
100% → 80% → 60% → 30%
```

再次受伤时必须：

```text
Current 立即更新
Ghost = Max(当前 Ghost, 本次 OldRatio, 本次 NewRatio)
不回跳到旧的 Current
不反向增长
不提前消失
重置 Hold Timer
```

始终满足：

```text
DamageGhostRatio >= CurrentHealthRatio
```

### 12.5 Healing

例如：

```text
40% → 65%
```

必须：

```text
CurrentHealthRatio 立即增长到 65%
DamageGhostRatio 不增加
不创建 Heal Trail
不刷新 Damage Hold Timer
```

若：

```text
CurrentHealthRatio >= DamageGhostRatio
```

则立即：

```text
DamageGhostRatio = CurrentHealthRatio
RecentDamage 消失
```

### 12.6 Death

```text
CurrentHealth <= 0
→ CurrentHealthRatio = 0
```

Widget 不负责：

```text
Destroy Enemy
死亡特效
掉落
尸体生命周期
```

只显示归零并随 Owner 生命周期销毁。

---

## 13. Normal / Elite Style 接口

Phase 1 使用最小的 Style Identifier：

```text
Normal
Elite
```

可影响：

```text
Placeholder Desired Size
Health Padding
Style Identifier
```

建议预留轻量接口：

```text
NormalDesiredSize
EliteDesiredSize
NormalPadding
ElitePadding
NormalFrameBrush
EliteFrameBrush
```

Phase 1 中：

```text
FrameBrush 可以为空或使用简单 Placeholder
```

不得将正式 Texture 路径写死到 Health 更新逻辑。

不要求本阶段创建复杂 DataAsset；若使用 Style DataAsset，必须保持轻量且不承载 Runtime Health 状态。

---

## 14. Tick、Timer 与性能

优先使用：

```text
Enemy HealthChanged Event
```

禁止 Idle 状态每帧执行：

```text
Get Owner
Find Component
Cast
读取 Current / Max
创建 Widget
Camera LookAt
```

Damage Trail 动画期间可以使用：

```text
短时 Timer
或仅在 Collapse 期间启用的 Tick
```

但必须：

```text
Trail 完成后停止
Widget 销毁时清除
不为每个 Idle Widget 持续查询 Health
```

---

## 15. 推荐实施切片

### Slice 0：项目事实确认

确认：

```text
AEnemyBase Health 数据源
Normal / Elite 类型来源
Widget Component 归属
现有 Enemy Blueprint
```

### Slice 1：Health Event Contract

完成：

```text
GetCurrentHealth
GetMaxHealth
OnEnemyHealthChanged
必要的 Heal API
初始化 / Damage / Heal / Death 广播
```

不得修改伤害计算与死亡状态机语义。

### Slice 2：Widget Component 与 Owner Binding

完成：

```text
EnemyHealthWidgetComponent
WidgetSpace = Screen
HealthWidgetHeightOffset
InitializeForEnemy
Delegate Bind / Unbind
```

### Slice 3：统一 Placeholder Widget

完成：

```text
WBP_EnemyHealth
Empty
RecentDamage
CurrentHealth
FramePlaceholder
Normal / Elite 尺寸接口
```

### Slice 4：Damage Trail 状态机

完成：

```text
单次 Damage
连续 Damage
Damage Hold
Ghost Collapse
Healing
Death 归零
```

### Slice 5：多实例与视图测试

完成：

```text
3 Normal + 2 Elite
Enemy 移动
Enemy Rotation
Camera View 改变
连续受伤
Healing
死亡
```

---

## 16. PIE 验收场景

### A. Normal 满血

```text
Widget Visible
位置在头顶
Current = 100%
```

### B. Elite 满血

```text
Widget Visible
Style = Elite
尺寸明显区别于 Normal
```

### C. 单次伤害

```text
100% → 70%
```

确认：

```text
Current 立即到 70%
Ghost 保留旧值
Ghost 随后通过长度回收
```

### D. 连续伤害

```text
100% → 80% → 55% → 30%
```

确认：

```text
Current 每次立即更新
Ghost 不回跳
Ghost 不反向增长
Trail 不错误消失
```

### E. Healing

```text
40% → 65%
```

确认：

```text
Current 直接增长
无 Heal Trail
不刷新 Damage Hold
```

### F. Damage + Healing

```text
100% → Damage 50% → Heal 65%
```

确认：

```text
Current 立即到 65%
Ghost 不被治疗重新抬高
已有 Damage Trail 不被错误覆盖
```

### G. Death

```text
Current = 0
```

确认：

```text
Health 正确归零
不产生重复事件
Widget 随 Enemy 生命周期处理
```

### H. 多敌人

至少：

```text
3 Normal + 2 Elite
```

确认任一敌人受伤时：

```text
其他 Enemy 的 Current 不变
其他 Enemy 的 Ghost 不变
Timer 不互相影响
```

### I. Enemy Rotation

确认：

```text
Enemy 旋转 360°
Widget 不跟随 Enemy Facing 翻转
Widget 不出现明显倾斜
```

### J. Camera / View

在允许的游戏 Camera 行为范围内改变视图，确认：

```text
Widget 始终正面可读
不出现侧面或背面
没有逐帧 LookAt Camera 逻辑
```

---

## 17. Phase 1 完成条件

```text
[ ] AEnemyBase 提供 Current / Max Health Getter
[ ] HealthChanged 事件在最终血量修改后广播
[ ] Damage / Heal / Death 语义可区分
[ ] 不使用第二套 Enemy Gameplay Health
[ ] Normal / Elite 类型来源明确
[ ] Normal 与 Elite Placeholder 实例可测试
[ ] 每个 Enemy 拥有独立 Widget Component
[ ] Widget Component 使用 Screen-space
[ ] Widget 位置跟随 Enemy 头顶 Anchor
[ ] Enemy Rotation 不影响正面可读性
[ ] 没有每帧 LookAt Camera Actor
[ ] WBP_EnemyHealth 为统一 Widget
[ ] Current Health 立即更新
[ ] Empty Health 为固定 100% 底槽
[ ] Recent Damage 使用 Ghost 长度回收
[ ] 没有使用透明度替代 Trail 回收
[ ] 连续伤害状态正确
[ ] Healing 只增长 Current
[ ] Healing 不创建或刷新 Damage Trail
[ ] DamageGhostRatio 始终不低于 CurrentHealthRatio
[ ] Death 时 Health 归零且不重复广播
[ ] 多敌人实例状态完全独立
[ ] Widget 委托正确解绑
[ ] Trail Timer / Tick 正确停止
[ ] 无永久 Idle Health 查询
[ ] 未导入最终美术
[ ] 已预留 Phase 2 Frame / Style 接口
[ ] PIE 无新增 Blueprint Error
[ ] Output Log 无新增关键错误
```

---

## 18. Phase Gate

只有第 17 节全部通过，才允许进入 Phase 2。

Phase 2 只负责：

```text
替换 Normal / Elite Frame
接入宣纸与墨锋资源
填充 Elite 标识
调整最终尺寸与 Padding
```

Phase 2 不得重新设计：

```text
Health 数据源
Damage Trail 状态机
Widget Owner Binding
Screen-space Billboard
多实例隔离逻辑
```

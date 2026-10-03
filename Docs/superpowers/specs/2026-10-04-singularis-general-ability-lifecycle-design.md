# 通用能力生命周期设计文档

## 概述

将 `SingularisGeneralAbility` 从"激活即结束"结构升级为完整的激活级（**Activation Level**）生命周期：授权（**Authorize**）→ 持续（**Sustain**）→ 撤销（**Revoke**），并引入状态标签（**State Tag**）驱动的声明式拦截与打断。目标用例：按住左键拖动物体（**Physics Handle**）、松开撤销；搬运状态下自动拒绝拾取；被打晕时自动打断搬运。

设计遵循两项原则：

1. **输入与逻辑分离**：能力（逻辑）不包含任何输入概念；输入相位（按下 / 释放）由意志组件（输入层）翻译为抽象的触发信号（**Trigger Signal**）。
2. **轻量化**：不引入属性集（**Attribute Set**）、游戏效果（**GameplayEffect**）、能力任务（**AbilityTask**）、游戏提示（**GameplayCue**）与预测回滚（**Prediction**）。

网络模型维持现状：服务器权威（**Server-Authoritative**）+ 复制（**Replication**）；逐帧逻辑在权威端执行，客户端通过复制状态与 **OnRep** 钩子响应，不引入本地客户端逐帧通道。

成功标准：

- 用例走查（第五节）全部成立。
- 瞬时能力行为语义不变（接口名称与配置随新语汇更新，不保留兼容层）。

---

## 术语表

| 术语 | 英文 | 含义 |
|---|---|---|
| 触发标签 | **Trigger Tag** | 输入层与逻辑层之间的抽象信号名；同时是触发管线映射的键 |
| 身份标签 | **Identity Tag** | 能力的稳定身份；用于撤销、打断与查询的寻址 |
| 状态标签 | **State Tag** | 通用能力组件状态容器中的标签 |
| 授权 | **Authorize** | 能力进入授权状态，构成生命周期起点 |
| 持续 | **Sustain** | 生命周期中段；持续能力的逐帧回调 |
| 撤销 | **Revoke** | 能力结束授权状态的统一流程（含正常结束与被打断） |
| 打断 | **Cancel** | 撤销的原因之一：冲突打断或状态打断 |
| 触发结束 | **TriggerEnded** | 输入层释放相位翻译出的撤销信号 |
| 门禁 | **Gates** | `RequiredTags` 与 `BlockedTags` 构成的声明式前置判定 |
| 能力策略 | **Policy** | 选择能力形态：瞬时（**Instant**）/ 持续（**Sustained**） |

---

## 架构概览

```
逻辑层（服务器权威）
  USingularisGeneralAbilityComponent（通用能力组件）
    ├─ 触发管线映射 TriggerPipelineMapping（触发标签 → 能力管线）
    ├─ 状态容器 StateTags（复制）
    ├─ 生命周期编排（授权 → 持续 → 撤销）
    ├─ 规则判定（必需 / 禁止 / 拥有 / 打断）
    └─ 逐帧调度（Tick）

  USingularisGeneralAbility（能力子对象，Instanced）
    ├─ 能力策略 Policy（Instant / Sustained）
    ├─ 身份标签 IdentityTag
    └─ 规则配置（RequiredTags / BlockedTags / OwnedTags / CancelAbilitiesWithTag / CanceledByTags）

输入层（本地）
  USingularisGeneralAbilityAnimusComponent（意志组件）
    ├─ 输入映射（InputAction → 触发标签，含按住相位声明 bWhileHeld）
    └─ RPC 上行（授权信号 / 触发结束信号）

数据流：意志组件 ──ServerTryAuthorizeAbility / ServerTryRevokeTrigger──▶ 通用能力组件
```

---

## 一、能力生命周期

### 1.1 能力策略（Policy）

```cpp
UENUM(BlueprintType)
enum class ESingularisGeneralAbilityPolicy : uint8
{
    /** 瞬时：Authorize 返回后立即结束，不进入授权状态。既有能力的默认取值。 */
    Instant   UMETA(DisplayName = "瞬时"),

    /** 持续：保持授权状态，直至被撤销。 */
    Sustained UMETA(DisplayName = "持续")
};
```

状态机：

```
瞬时（Instant）
  授权请求 → 门禁 → CanAuthorize → 冲突打断 → Authorize → 结束（无状态残留）

持续（Sustained）
  授权请求 → 门禁 → CanAuthorize → 冲突打断 → 授权（标记 + 授予 OwnedTags）
           → Authorize → [ Sustain … ] → 撤销流程
                                           ▲
  撤销触发源：触发结束信号 / CanceledByTags 命中 / CancelAbilitiesWithTag 命中 /
              Revoke 自调用 / 宿主销毁（EndPlay）
```

### 1.2 撤销原因

```cpp
UENUM(BlueprintType)
enum class ESingularisGeneralAbilityEndReason : uint8
{
    /** 完成：能力自行撤销。 */
    Completed     UMETA(DisplayName = "完成"),

    /** 触发结束：输入层触发结束信号（按住型输入的释放相位）。 */
    TriggerEnded  UMETA(DisplayName = "触发结束"),

    /** 被打断：冲突打断或状态打断。 */
    Canceled      UMETA(DisplayName = "被打断"),

    /** 销毁：宿主组件销毁（EndPlay）。 */
    Destroyed     UMETA(DisplayName = "销毁")
};
```

撤销原因不复制到客户端；客户端仅感知授权状态变化（第 1.6 节）。

### 1.3 能力基类配置

| 字段 | 类型 | 说明 |
|---|---|---|
| `IdentityTag` | `FGameplayTag` | 身份标签。状态层寻址键（撤销 / 打断 / 查询）。约定位于 `Singularis.General.Ability.Identity` 子树；允许无效，无效时仅状态层寻址不可用 |
| `Policy` | `ESingularisGeneralAbilityPolicy` | 能力策略：瞬时 / 持续，默认 `Instant` |
| `RequiredTags` | `FGameplayTagContainer` | 授权必需：状态容器须包含全部标签 |
| `BlockedTags` | `FGameplayTagContainer` | 授权禁止：状态容器命中任一标签即拒绝 |
| `OwnedTags` | `FGameplayTagContainer` | 授权授予：授权期间写入状态容器，撤销时清除；仅 `Sustained` 有效 |
| `CancelAbilitiesWithTag` | `FGameplayTagContainer` | 授权时打断：撤销身份标签命中的已授权能力（原因 `Canceled`）；对 `Instant` 同样生效 |
| `CanceledByTags` | `FGameplayTagContainer` | 状态打断：状态容器新增标签命中时撤销自身（原因 `Canceled`）；仅 `Sustained` 有效 |

字段类别：`IdentityTag` / `Policy` 归入"参数"，规则容器归入"行为规则"。

### 1.4 能力基类接口

| 语义 | 名称 | 类型 | 说明 |
|---|---|---|---|
| 前置检查 | `CanAuthorize` | `BlueprintNativeEvent` + `BlueprintCallable` | 声明式门禁通过后的命令式检查；返回 false 跳过授权。替代原 `CanActivate` |
| 授权 | `Authorize` | `BlueprintNativeEvent` + `BlueprintCallable` | 核心行为逻辑。替代原 `Activate` |
| 持续 | `Sustain` | `BlueprintNativeEvent` + `BlueprintCallable` | 持续能力逐帧回调（仅服务器、仅已授权） |
| 撤销请求 | `Revoke` | `BlueprintCallable` | 新增。请求撤销自身，幂等；仅服务器生效（内部权威卫语句） |
| 撤销钩子 | `OnRevoked` | `BlueprintImplementableEvent` | 撤销清理钩子（服务器）；用于释放物理柄、注销委托等 |
| 授权查询 | `IsAuthorized` | `BlueprintPure` | 是否处于授权状态 |
| 上下文查询 | `GetAuthorizationContext` | `BlueprintPure` | 返回本次授权的上下文（仅服务器、仅授权期间有效） |
| 触发标签查询 | `GetAuthorizationTriggerTag` | `BlueprintPure` | 本次授权的触发标签；程序化授权时为无效标签 |
| 客户端授权钩子 | `OnClientAuthorized` | `BlueprintImplementableEvent` | 客户端收到授权复制时触发 |
| 客户端撤销钩子 | `OnClientRevoked` | `BlueprintImplementableEvent` | 客户端收到撤销复制时触发 |
| 组件查询 | `GetOwningAbilityComponent` | `BlueprintPure` | 获取所属通用能力组件 |
| 状态写入（进入） | `EnterAuthorization` | C++（非反射） | 置授权标记并记录触发标签与上下文；仅由通用能力组件调用 |
| 状态写入（退出） | `LeaveAuthorization` | C++（非反射） | 仅清除授权标记；仅由通用能力组件在撤销流程起始调用 |
| 状态写入（清记录） | `ClearAuthorizationRecord` | C++（非反射） | 清空触发标签与上下文；仅由通用能力组件在撤销流程末尾调用 |

命名迁移说明：原 `CanActivate` / `Activate` 按上表改名为 `CanAuthorize` / `Authorize`；生命周期语汇为授权（**Authorize**）→ 持续（**Sustain**）→ 撤销（**Revoke**），全链路名称以此词根统一。原遗留测试参数 `SyncValue` 与 `OnRep_SyncValue` 已删除，能力复制面仅保留生命周期所需的 `bIsAuthorized`。

### 1.5 授权流程（服务器统一例程）

对每个候选能力实例依次执行：

1. 卫语句：能力有效且未授权；已授权的 `Sustained` 能力再次收到授权请求时忽略（非重入，**Non-Reentrant**）。
2. 门禁：`StateTags.HasAll(RequiredTags)` 且 `!StateTags.HasAny(BlockedTags)`；不通过则记录日志并跳过。
3. `CanAuthorize(Context)` 返回 false 则跳过。
4. 冲突打断：按 `CancelAbilitiesWithTag` 撤销身份标签命中的已授权能力（原因 `Canceled`）。
5. 仅 `Sustained`：置授权标记（`bIsAuthorized`）、记录 `AuthorizationTriggerTag`、缓存 `AuthorizationContext`、授予 `OwnedTags`（含状态变更扫描）、开启组件逐帧。
6. 调用 `Authorize(Context)`。
7. `Instant`：流程到此结束，无状态残留，不授予 `OwnedTags`、不进入逐帧；需要写入状态时由能力显式调用 `AddStateTag`。

行为与旧版一致性：管线内各能力独立判定、独立执行，单个能力失败不中断管线；触发标签的层级匹配语义维持不变。

### 1.6 撤销流程（服务器统一例程）

1. 卫语句：能力有效且已授权；否则无操作（幂等）。
2. 清除授权标记 `bIsAuthorized`（先行置位，保证清理期间的重入安全）。
3. 调用 `OnRevoked(Reason)`（服务器清理钩子）。
4. 撤销 `OwnedTags`（多拥有者检查，第 2.4 节）；撤销引起的状态变更广播状态事件，但不触发打断扫描。
5. 清空 `AuthorizationContext` 与 `AuthorizationTriggerTag`。
6. 若无其它已授权能力，关闭组件逐帧。

`Instant` 能力不进入本流程。

### 1.7 规则适用矩阵

| 配置 | `Instant` | `Sustained` |
|---|---|---|
| `RequiredTags` / `BlockedTags` | 生效 | 生效 |
| `CanAuthorize` | 生效 | 生效 |
| `CancelAbilitiesWithTag` | 生效（打断他人） | 生效 |
| `OwnedTags` | 不生效（不授予） | 生效 |
| `CanceledByTags` | 不生效（不授权，无可打断） | 生效 |
| `Sustain` | 不调用 | 逐帧调用 |

### 1.8 逐帧（Sustain）

- 组件 `bCanEverTick = true`、`bStartWithTickEnabled = false`。
- 首个 `Sustained` 能力授权时开启组件逐帧；最后一个撤销后关闭。仅在权威端调用。
- 逐帧遍历按实例去重（同一实例出现在多个管线条目时每帧仅 `Sustain` 一次）。

---

## 二、状态标签与拦截 / 打断

### 2.1 状态容器与 API

- 属性：`FGameplayTagContainer StateTags`（复制，`ReplicatedUsing = OnRep_StateTags`）。
- 语义：集合（**Set Semantics**），无计数、无堆叠；堆叠由游戏层实现。
- 权威 API（`BlueprintCallable` + `BlueprintAuthorityOnly`）：

| API | 说明 |
|---|---|
| `AddStateTag(const FGameplayTag& Tag)` | 幂等；新增后执行状态打断扫描 |
| `RemoveStateTag(const FGameplayTag& Tag)` | 幂等；移除不触发打断扫描 |
| `HasStateTag(const FGameplayTag& Tag) const` | 层级包含判定（`BlueprintPure`） |
| `GetStateTags() const` | 返回容器副本（`BlueprintPure`） |

- 事件：`FOnStateTagsChangedSignature(const FGameplayTagContainer& AddedTags, const FGameplayTagContainer& RemovedTags)`，实例 `OnStateTagsChangedEvent`（`BlueprintAssignable`）。服务器本地变更后与客户端 **OnRep** 到达时均广播。

### 2.2 判定语义

全系统统一采用层级包含（**Hierarchy-Aware Containment**）：声明父标签可被子标签满足。

| 判定 | 实现依据 |
|---|---|
| 门禁 | `StateTags.HasAll(RequiredTags)`；`!StateTags.HasAny(BlockedTags)` |
| 冲突打断 | 目标能力 `IdentityTag.MatchesTag(过滤标签)`，过滤器取 `CancelAbilitiesWithTag` 任一标签 |
| 状态打断 | 新增标签 `T.MatchesTag(过滤标签)`，过滤器取 `CanceledByTags` 任一标签 |
| 程序化撤销 | 目标能力 `IdentityTag.MatchesTag(请求标签)` |
| 触发层匹配 | 映射键 `Key.MatchesTag(触发标签)`（现状语义） |

### 2.3 GAS 概念对照

| GAS 概念 | 本设计对应 | 说明 |
|---|---|---|
| `AbilityTags` | `IdentityTag`（单一身份标签） | 状态层寻址键 |
| `ActivationRequiredTags` / `ActivationBlockedTags` | `RequiredTags` / `BlockedTags` | 直接对应 |
| `ActivationOwnedTags` | `OwnedTags` | 授权期间授予，撤销时清除 |
| `BlockAbilitiesWithTag` | 不新增字段 | 由"`OwnedTags` + 对方 `BlockedTags`"组合等价表达 |
| `CancelAbilitiesWithTag` | `CancelAbilitiesWithTag` | 授权时打断 |
| 效果（GE）通道的状态打断 | `CanceledByTags` | 无游戏效果，改由状态容器新增标签触发 |
| `Activate` / `EndAbility` | `Authorize` / `Revoke` | 生命周期对齐 |
| **AbilityTask** / 异步节点 | `Sustain` | 服务器逐帧替代 |
| **GameplayCue** / 表现通道 | 客户端钩子（`OnClientAuthorized` / `OnClientRevoked`）+ 自定义 **Multicast RPC** | 不新增表现系统 |
| Instancing / 预测 | 单实例、非重入、服务器权威 | 简化 |
| `WaitInputRelease` / Lyra `WhileInputActive` | 输入层 `bWhileHeld` 相位声明 | 输入语义不进入能力（第 3.5 节） |

### 2.4 多拥有者标签撤销

撤销某能力的 `OwnedTags` 时，对每个标签检查其余已授权能力的 `OwnedTags`；仍有其它拥有者则保留。不引入引用计数结构，避免并行状态失步。

约束：外部代码不应直接增删与 `OwnedTags` 同名的状态标签；`OwnedTags` 约定位于状态子命名空间（`...State`）。

### 2.5 可观测性

新增日志分类 `LogSingularisGeneralAbility`：

- 规则命中（门禁拒绝、冲突打断、状态打断）以 `Log` 级输出，包含能力名、原因与命中标签。
- 生命周期常规流转（授权、撤销、触发信号）以 `Verbose` 级输出。

---

## 三、标签、寻址与输入层

### 3.1 原生标签结构

```text
Singularis.General                                    （现有，根）
└─ Singularis.General.Ability                         （现有，能力根）
   ├─ Singularis.General.Ability.Trigger              （新增，触发标签约定根）
   │  └─ Singularis.General.Ability.Trigger.Default   （原 Singularis.General.Ability.Default，内置默认触发标签）
   ├─ Singularis.General.Ability.Identity             （新增，身份标签约定根）
   └─ Singularis.General.Ability.State                （新增，状态标签约定根）
```

- C++ 声明：`SingularisGeneral`、`SingularisGeneral_Ability`、`SingularisGeneral_Ability_Trigger`、`SingularisGeneral_Ability_Trigger_Default`、`SingularisGeneral_Ability_Identity`、`SingularisGeneral_Ability_State`。
- 各角色配置的标签选择器按约定根约束：触发标签限定 `...Ability.Trigger`；身份标签限定 `...Ability.Identity`；状态标签（`OwnedTags`、`CanceledByTags` 与状态 API 参数）限定 `...Ability.State`。组件公开 API 的标签参数（触发 / 身份 / 状态）同样按对应根附加选择器类别元数据（`UPARAM`）。
- 原 `Singularis.General.Ability.Default` 的值迁入 `...Trigger.Default`；宿主资产中的引用在实现阶段直接更新，不保留旧值。

### 3.2 寻址模型

| 寻址 | 方式 | 入口 |
|---|---|---|
| 触发寻址 | 触发标签 → 触发管线映射（授权）；触发标签 → `AuthorizationTriggerTag` 关联（撤销） | `TryAuthorizeAbility`、`TryRevokeTrigger` |
| 身份寻址 | 身份标签 → 能力集合（层级匹配） | `TryAuthorizeAbilitiesByTag`、`TryRevokeAbilitiesByTag` |
| 类寻址 | 类匹配（`IsA`） | `TryAuthorizeAbilityByClass` |

### 3.3 通用能力组件公开 API

| API | 说明 |
|---|---|
| `TryAuthorizeAbility(const FGameplayTag& TriggerTag, AController* Controller, const FInputActionValue& InputActionValue)` | 触发授权入口（原 `TryActivateAbility`）；服务器权威 |
| `TryAuthorizeAbilitiesByTag(const FGameplayTag& IdentityTag, AController* Controller)` | 程序化按身份标签授权；不记录触发关联（不受触发结束信号影响） |
| `TryAuthorizeAbilityByClass(TSubclassOf<USingularisGeneralAbility> AbilityClass, AController* Controller)` | 程序化按类授权；不记录触发关联（不受触发结束信号影响） |
| `TryRevokeTrigger(const FGameplayTag& TriggerTag)` | 触发结束信号入口：撤销 `AuthorizationTriggerTag` 精确等于该标签的全部已授权能力（原因 `TriggerEnded`） |
| `TryRevokeAbilitiesByTag(const FGameplayTag& IdentityTag, ESingularisGeneralAbilityEndReason Reason)` | 程序化按身份标签撤销；`Reason` 显式传入 |

组件属性改名：`AbilityPipelineMapping` → `TriggerPipelineMapping`（触发管线映射）。

### 3.4 意志组件（输入层）

- 输入条目 `FSingularisGeneralAbilityAnimusInput`：

| 字段 | 说明 |
|---|---|
| `InputAction` | 输入动作（不变） |
| `TriggerTag` | 触发标签（原字段名 `AbilityTag` 改名；选择器限定 `...Ability.Trigger`） |
| `bWhileHeld` | 按住相位声明，默认 false。为 true 时该输入为按住型：释放相位发送触发结束信号 |

- 绑定：`Started` → 授权信号（所有条目）；`Completed` 与 `Canceled` → 触发结束信号（仅 `bWhileHeld` 条目）。
- RPC：
  - `ServerTryAuthorizeAbility(USingularisGeneralAbilityComponent* AbilityComponent, const FGameplayTag& TriggerTag, const FInputActionValue& InputActionValue)`（原 `ServerTryActivateAbility` 改名）。
  - `ServerTryRevokeTrigger(USingularisGeneralAbilityComponent* AbilityComponent, const FGameplayTag& TriggerTag)`（新增）；均 `Server, Reliable, WithValidation`。
- 回调拆分：`HandleAbilityAuthorize`（原 `HandleAbilityInput`）/ `HandleAbilityRevoke`。
- 启动绑定：内置默认 IMC 与 IA 不变；默认触发标签改为 `Singularis.General.Ability.Trigger.Default`。

### 3.5 输入与逻辑分离契约

| 角色 | 职责 |
|---|---|
| 意志组件（输入层） | 解释输入相位：按下 → 授权信号；释放 / 中止（仅按住型）→ 触发结束信号。不涉及能力逻辑 |
| 通用能力组件（逻辑层） | 统一契约：触发结束信号 → 撤销该触发关联的持续能力。不解释输入设备与相位 |
| 能力（逻辑） | 不感知输入：无输入字段、无输入事件、无输入相位 |
| 非意志组件输入源 | 调用同一逻辑层 API 表达同一语义（`TryRevokeTrigger` 或 `TryRevokeAbilitiesByTag`） |

由此，按住型（拖动、瞄准）与非按住型（搬运、光环）由同一契约覆盖，且能力本身与输入无关。

---

## 四、复制与客户端钩子

- 权威边界：授权 / 持续 / 撤销的编排与状态标签写入仅服务器执行；客户端只读。
- 复制属性：
  - 组件：`StateTags`（`ReplicatedUsing = OnRep_StateTags`）。
  - 能力：`bIsAuthorized`（`ReplicatedUsing = OnRep_IsAuthorized`）。
  - 子对象复制沿用 `AddReplicatedSubObject` 管线。
- 客户端钩子：
  - 组件：`OnStateTagsChangedEvent`（差异标签，服务器与客户端均触发）。
  - 能力：`OnClientAuthorized` / `OnClientRevoked`（由 `OnRep_IsAuthorized` 驱动，仅客户端）。
- 瞬态数据（不复制）：`AuthorizationContext`、`AuthorizationTriggerTag`、撤销原因。
- 客户端加入中途：首次复制到达时 `OnRep_IsAuthorized` 正常触发，客户端据此建立本地表现。

---

## 五、用例走查：按住拖动 → 松开撤销

示例标签（实际以项目标签表为准）：

- 触发标签：`Singularis.General.Ability.Trigger.Grab`
- 身份标签：`Singularis.General.Ability.Identity.Grab`
- 状态标签：`Singularis.General.Ability.State.Dragging`、`Singularis.General.Ability.State.Stunned`

能力配置：

- `UGrabAbility`（`Sustained`）：`IdentityTag = ...Identity.Grab`；`OwnedTags = [...State.Dragging]`；`CanceledByTags = [...State.Stunned]`；`BlockedTags = [...State.Stunned]`。
- `UPickupAbility`（`Instant`）：`BlockedTags = [...State.Dragging]`。
- 意志组件输入条目：鼠标左键 IA，`TriggerTag = ...Trigger.Grab`，`bWhileHeld = true`。

流程：

| 步骤 | 流程 |
|---|---|
| 按住左键 | `Started` → `ServerTryAuthorizeAbility` → 门禁通过 → 授予 `State.Dragging` → `Authorize`（射线命中、抓取物理柄）→ 进入授权，组件逐帧开启 |
| 按住期间 | 服务器 `Sustain` 经 `GetAuthorizationContext().Controller` 的视图信息更新物理柄目标位姿；被拖物体的位移同步由引擎物理复制或游戏层策略呈现 |
| 松开左键 | `Completed` → `ServerTryRevokeTrigger` → 撤销（`TriggerEnded`）→ `OnRevoked` 释放物理柄 → 清除 `State.Dragging` → 客户端 `OnClientRevoked` |
| 拖动中拾取 | `UPickupAbility` 被 `BlockedTags` 命中 `State.Dragging` 拒绝，输入层无布尔分支 |
| 拖动中眩晕 | `AddStateTag(...State.Stunned)` → 命中 `CanceledByTags` → 撤销（`Canceled`），清理与清除路径同上 |
| 宿主销毁 | `EndPlay` → 全部已授权能力撤销（`Destroyed`）→ 注销子对象 |

---

## 六、文件级改动清单

| 文件 | 改动 |
|---|---|
| `Public/SingularisGeneralAbility.h` / `Private/SingularisGeneralAbility.cpp` | 新增日志分类 `LogSingularisGeneralAbility` |
| `Public/Types/SingularisGeneralAbilityGameplayTags.h` / `Private/Types/SingularisGeneralAbilityGameplayTags.cpp` | 标签树：新增 `Trigger` / `Identity` / `State` 根；`Default` 迁入 `Trigger` 根 |
| `Public/Types/SingularisGeneralAbilityType.h` | 新增 `ESingularisGeneralAbilityPolicy`、`ESingularisGeneralAbilityEndReason` |
| `Public/Types/SingularisGeneralAbilityAnimusComponentType.h` | `AbilityTag` → `TriggerTag`；新增 `bWhileHeld` |
| `Public/Objects/SingularisGeneralAbilityBase.h` / `Private/Objects/SingularisGeneralAbilityBase.cpp` | 生命周期配置与接口（授权 / 持续 / 撤销语汇）、授权状态复制、授权 / 撤销流程钩子、`Sustain` |
| `Public/Components/SingularisGeneralAbilityComponent.h` / `Private/Components/SingularisGeneralAbilityComponent.cpp` | `StateTags` 与状态 API、事件、授权 / 撤销编排、逐帧、寻址入口、`TriggerPipelineMapping` 改名、`EndPlay` 清理顺序调整（先撤销后注销） |
| `Public/Components/SingularisGeneralAbilityAnimusComponent.h` / `Private/Components/SingularisGeneralAbilityAnimusComponent.cpp` | 相位绑定、触发结束 RPC、回调拆分、默认触发标签更新 |
| 宿主项目：`InstallAbility` / `PlaceAbility`（C++）与 `GA_Install` / `GA_Place` / `GA_Seat` / `GA_UseItem`（蓝图） | 跟随改名与标签树更新；蓝图事件重写与配置直接迁移，不保留兼容层 |

实现注意：

- 组件 `EndPlay` 顺序：先对全部已授权能力执行撤销（`Destroyed`），再注销复制子对象，最后调用父类实现。
- 授权 / 撤销 / 逐帧 / 状态扫描的集合遍历均按实例去重。

---

## 七、非目标

- 运行时能力授予 / 撤销（**Ability Spec** 级动态增删）。
- 本地预测（**Local Prediction**）与客户端逐帧手感通道。
- 属性数值（**Attribute**）、**GameplayEffect**、**AbilityTask**、**GameplayCue**。
- 同一能力多实例与重入（重复授权请求在已授权时忽略）。
- 标签堆叠与计数（**Stacking**）。
- 输入 `Triggered` 相位的逐帧上行。
- 撤销原因的复制与客户端可见性。
- 具体物理柄实现（属游戏层能力逻辑）。

---

## 八、验证计划

实现阶段执行，当前环境不编译。

手动用例（PIE）：

1. 瞬时能力回归：`InstallAbility` / `PlaceAbility` 行为与旧版一致（接口改名后）。
2. 按住拖动：授权 → 持续 → 松开撤销（`TriggerEnded`）；物理柄释放、状态清除、客户端钩子依次成立。
3. 状态拦截：拖动期间拾取被拒绝。
4. 状态打断：眩晕标签命中 `CanceledByTags`，拖动被打断（`Canceled`）。
5. 冲突打断：`CancelAbilitiesWithTag` 命中的能力被撤销（`Canceled`）。
6. 多拥有者标签撤销：两个能力拥有同一标签时，一个撤销不误删标签。
7. 程序化入口：`TryAuthorizeAbilitiesByTag` / `TryAuthorizeAbilityByClass` / `TryRevokeAbilitiesByTag` 生效，且不参与触发结束信号。
8. `EndPlay`：宿主销毁时全部已授权能力撤销（`Destroyed`）。
9. 复制：监听服务器 + 客户端观察 `StateTags`、`bIsAuthorized` 与客户端钩子。

自动化（可选）：门禁与标签判定抽为纯函数（**Pure Function**），以 UE 自动化测试覆盖规则矩阵。

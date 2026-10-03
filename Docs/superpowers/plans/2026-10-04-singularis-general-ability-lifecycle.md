# 通用能力生命周期实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `SingularisGeneralAbility` 升级为具备授权（Authorize）/ 持续（Sustain）/ 撤销（Revoke）完整生命周期与状态标签拦截 / 打断能力的轻量化通用能力系统。

**Architecture:** 输入与逻辑分离——意志组件（输入层）将输入相位翻译为授权信号与触发结束信号；通用能力组件（逻辑层）以 `TriggerPipelineMapping` 与 `StateTags` 编排生命周期，全部服务器权威并以复制同步客户端。能力基类仅保留输入无关的策略（`Instant` / `Sustained`）与规则配置（`RequiredTags` / `BlockedTags` / `OwnedTags` / `CancelAbilitiesWithTag` / `CanceledByTags`）。

**Tech Stack:** Unreal Engine 5（C++ / UHT / EnhancedInput / GameplayTags），宿主项目 VehicleTour（UE C++ 模块 + 蓝图内容）。

**Spec:** `VehicleTour/Plugins/SingularisGeneralAbility/Docs/superpowers/specs/2026-10-04-singularis-general-ability-lifecycle-design.md`

> **执行修订（2026-10-04）**：按用户指令，全部代码依 `singularis-skeleton` 新骨架重写（Component region 顺序 Parameter → Event Dispatcher → State → Constructors → ActorComponent Interface → API → SPI → RPC → Response → Callback → Internal Function；类别命名 `引力奇点…`）；能力基类改为私有 State + SPI 写入（`EnterAuthorization` / `LeaveAuthorization` / `ClearAuthorizationRecord`），并新增只读 `GetAuthorizationTriggerTag`；删除遗留测试参数 `SyncValue`（含 `OnRep_SyncValue`）。**语汇修订（2026-10-04）**：生命周期语汇由“接合—维持—脱离”（`Engage` / `Sustain` / `Disengage`）改为“授权—持续—撤销”（`Authorize` / `Sustain` / `Revoke`），全链路接口按新词根统一。本计划内代码块为修订前版本（已同步新语汇与 `SyncValue` 删除），最终实现以仓库文件为准。

## Global Constraints

- 命名语汇：能力侧 `CanAuthorize` / `Authorize` / `Sustain` / `Revoke` / `OnRevoked` / `IsAuthorized` / `GetAuthorizationContext` / `OnClientAuthorized` / `OnClientRevoked` / `GetOwningAbilityComponent`；组件侧 `TryAuthorizeAbility` / `TryAuthorizeAbilitiesByTag` / `TryAuthorizeAbilityByClass` / `TryRevokeTrigger` / `TryRevokeAbilitiesByTag` / `AddStateTag` / `RemoveStateTag` / `HasStateTag` / `GetStateTags` / `OnStateTagsChangedEvent`；RPC 侧 `ServerTryAuthorizeAbility` / `ServerTryRevokeTrigger`。旧名称（`CanActivate` / `Activate` / `TryActivateAbility` / `AbilityPipelineMapping` / `AbilityTag` / `bIsContinuous` 及过渡期的 `CanEngage` / `Engage` / `Disengage` / `Engagement` 词族）一律移除，不保留兼容层。
- 输入与逻辑分离：能力不包含任何输入概念；输入相位仅存在于意志组件（`bWhileHeld`）。
- 服务器权威（`BlueprintAuthorityOnly`）与复制；不引入本地预测。
- 不引入 AttributeSet / GameplayEffect / AbilityTask / GameplayCue。
- 标签根：`Singularis.General.Ability.Trigger` / `.Identity` / `.State`；旧值 `Singularis.General.Ability.Default` 迁至 `...Trigger.Default`，不保留旧值。
- 编码规范：中文注释、卫语句先行、幂等设计、函数体内逻辑按 `1) 2) 3)` 分步注释；类与公开接口具备文档注释；region 结构沿用插件既有骨架。
- 自动化测试筛选名：`Singularis.GeneralAbility.Rules`。
- 验证环境：本仓库由执行者在 UE 编辑器中构建（Live Coding 或 IDE 构建 `VehicleTourEditor` 目标）；构建与 PIE 步骤均为必做验证。

## Review Focus

1. **层级包含方向反转**：子标签应满足父过滤器 / 父要求，父标签不得满足子过滤器——由 Task 2 步骤 1（测试先行）与步骤 13（运行转绿）固定（`MatchesAnyFilterTag` / `EvaluateAuthorizationGates` 正反例）。
2. **多拥有者标签误撤销**：两个持续能力拥有同一 `OwnedTags` 标签，其一撤销不得移除该标签——由 Task 4 清单项 7 验证。
3. **幂等与非重入**：已授权能力的重复授权请求必须忽略；未授权撤销必须无操作；重复状态标签增删不得广播——由 Task 4 清单项 8 验证。
4. **EndPlay 清理顺序**：宿主销毁时必须先撤销（`OnRevoked` 可见完整上下文）再注销复制子对象——由 Task 4 清单项 9 验证。
5. **触发关联精确性**：程序化授权（`TryAuthorizeAbilitiesByTag` / `TryAuthorizeAbilityByClass`）不记录触发关联，触发结束信号不得命中——由 Task 4 清单项 10 验证。

---

### Task 1: 基础类型、日志分类与原生标签

**Files:**

- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Types/SingularisGeneralAbilityType.h`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Types/SingularisGeneralAbilityGameplayTags.h`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Types/SingularisGeneralAbilityGameplayTags.cpp`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/SingularisGeneralAbility.h`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/SingularisGeneralAbility.cpp`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Components/SingularisGeneralAbilityAnimusComponent.cpp`

**Interfaces:**

- Consumes: 无。
- Produces:
  - `enum class ESingularisGeneralAbilityPolicy : uint8 { Instant, Sustained };`
  - `enum class ESingularisGeneralAbilityEndReason : uint8 { Completed, TriggerEnded, Canceled, Destroyed };`
  - `DECLARE_LOG_CATEGORY_EXTERN(LogSingularisGeneralAbility, Log, All);`
  - 原生标签：`SingularisGeneral_Ability_Trigger`、`SingularisGeneral_Ability_Trigger_Default`、`SingularisGeneral_Ability_Identity`、`SingularisGeneral_Ability_State`。

- [ ] **Step 1: 在 `SingularisGeneralAbilityType.h` 中新增两个枚举**

在 `#include "SingularisGeneralAbilityType.generated.h"` 之后、`FSingularisGeneralAbilityContext` 文档注释之前插入以下内容：

```cpp
/**
 * 引力奇点通用能力策略。
 * 决定能力授权后的生命周期形态。
 */
UENUM(BlueprintType)
enum class ESingularisGeneralAbilityPolicy : uint8
{
	/** 瞬时：Authorize 返回后立即结束，不进入授权状态。 */
	Instant UMETA(DisplayName = "瞬时"),

	/** 持续：保持授权状态，直至被撤销。 */
	Sustained UMETA(DisplayName = "持续")
};

/**
 * 撤销原因。
 * 描述能力结束授权状态的触发路径。
 */
UENUM(BlueprintType)
enum class ESingularisGeneralAbilityEndReason : uint8
{
	/** 完成：能力自行撤销。 */
	Completed UMETA(DisplayName = "完成"),

	/** 触发结束：输入层触发结束信号。 */
	TriggerEnded UMETA(DisplayName = "触发结束"),

	/** 被打断：冲突打断或状态打断。 */
	Canceled UMETA(DisplayName = "被打断"),

	/** 销毁：宿主组件销毁。 */
	Destroyed UMETA(DisplayName = "销毁")
};
```

- [ ] **Step 2: 整体替换 `SingularisGeneralAbilityGameplayTags.h`**

```cpp
#pragma once

#include <CoreMinimal.h>
#include <NativeGameplayTags.h>

/**
 * Singularis General Ability 插件使用的原生 GameplayTag 声明。
 *
 * 标签层级：
 *   Singularis.General
 *   Singularis.General.Ability
 *   Singularis.General.Ability.Trigger
 *   Singularis.General.Ability.Trigger.Default
 *   Singularis.General.Ability.Identity
 *   Singularis.General.Ability.State
 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_Trigger);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_Trigger_Default);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_Identity);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_State);
```

- [ ] **Step 3: 整体替换 `SingularisGeneralAbilityGameplayTags.cpp`**

```cpp
#include "Types/SingularisGeneralAbilityGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	SingularisGeneral,
	"Singularis.General",
	"Singularis General"
);

UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	SingularisGeneral_Ability,
	"Singularis.General.Ability",
	"Singularis General Ability"
);

UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	SingularisGeneral_Ability_Trigger,
	"Singularis.General.Ability.Trigger",
	"触发标签约定根"
);

UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	SingularisGeneral_Ability_Trigger_Default,
	"Singularis.General.Ability.Trigger.Default",
	"内置默认触发标签"
);

UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	SingularisGeneral_Ability_Identity,
	"Singularis.General.Ability.Identity",
	"身份标签约定根"
);

UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	SingularisGeneral_Ability_State,
	"Singularis.General.Ability.State",
	"状态标签约定根"
);
```

- [ ] **Step 4: 在模块头 `SingularisGeneralAbility.h` 中新增日志分类**

在 `#include <Modules/ModuleManager.h>` 之后插入：

```cpp
#include <Logging/LogMacros.h>

/** Singularis General Ability 插件日志分类。 */
DECLARE_LOG_CATEGORY_EXTERN(LogSingularisGeneralAbility, Log, All);
```

- [ ] **Step 5: 在模块实现 `SingularisGeneralAbility.cpp` 中定义日志分类**

在 `#include "SingularisGeneralAbility.h"` 之后插入：

```cpp
DEFINE_LOG_CATEGORY(LogSingularisGeneralAbility);
```

- [ ] **Step 6: 更新意志组件默认触发标签常量**

在 `SingularisGeneralAbilityAnimusComponent.cpp` 中，将默认输入条目的标签：

```cpp
			{
				DefaultAbilityActionFinder.Object,
				SingularisGeneral_Ability_Default
			}
```

替换为：

```cpp
			{
				DefaultAbilityActionFinder.Object,
				SingularisGeneral_Ability_Trigger_Default
			}
```

- [ ] **Step 7: 构建并验证**

在编辑器中触发 Live Coding 编译（Ctrl+Alt+F11），或通过 IDE 构建 `VehicleTourEditor` 目标。

预期：编译通过，无错误与新增警告。

注：项目内容中引用旧标签 `Singularis.General.Ability.Default` 的资产在本步骤后暂时出现无效标签告警，属预期，Task 3 统一处理。

- [ ] **Step 8: 提交**

```bash
git add VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Types/SingularisGeneralAbilityType.h \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Types/SingularisGeneralAbilityGameplayTags.h \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Types/SingularisGeneralAbilityGameplayTags.cpp \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/SingularisGeneralAbility.h \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/SingularisGeneralAbility.cpp \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Components/SingularisGeneralAbilityAnimusComponent.cpp
git commit -m "feat(SingularisGeneralAbility): add policy/end reason enums, log category and tag roots"
```

---

### Task 2: 规则判定测试与核心生命周期重构

本任务是单一构建单元：能力基类、通用能力组件、意志组件与宿主 C++ 能力必须在同一次构建内整体切换，无法拆分提交。

**Files:**

- Create: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Tests/SingularisGeneralAbilityRuleTests.cpp`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Objects/SingularisGeneralAbilityBase.h`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Objects/SingularisGeneralAbilityBase.cpp`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Components/SingularisGeneralAbilityComponent.h`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Components/SingularisGeneralAbilityComponent.cpp`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Types/SingularisGeneralAbilityAnimusComponentType.h`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Components/SingularisGeneralAbilityAnimusComponent.h`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Components/SingularisGeneralAbilityAnimusComponent.cpp`
- Modify: `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Types/SingularisGeneralAbilityComponentType.h`（文档注释）
- Modify: `VehicleTour/Source/VehicleTour/Private/GeneralAbilities/InstallAbility.h`
- Modify: `VehicleTour/Source/VehicleTour/Private/GeneralAbilities/InstallAbility.cpp`
- Modify: `VehicleTour/Source/VehicleTour/Private/GeneralAbilities/PlaceAbility.h`
- Modify: `VehicleTour/Source/VehicleTour/Private/GeneralAbilities/PlaceAbility.cpp`

**Interfaces:**

- Consumes:
  - `ESingularisGeneralAbilityPolicy`、`ESingularisGeneralAbilityEndReason`、`LogSingularisGeneralAbility`、新原生标签（Task 1）。
- Produces:
  - `USingularisGeneralAbility`：`IdentityTag` / `Policy` / `RequiredTags` / `BlockedTags` / `OwnedTags` / `CancelAbilitiesWithTag` / `CanceledByTags`；`bIsAuthorized` / `AuthorizationTriggerTag` / `AuthorizationContext`；`CanAuthorize` / `Authorize` / `Sustain` / `Revoke` / `OnRevoked` / `IsAuthorized` / `GetAuthorizationContext` / `GetOwningAbilityComponent` / `OnClientAuthorized` / `OnClientRevoked` / `OnRep_IsAuthorized`。
  - `USingularisGeneralAbilityComponent`：`TriggerPipelineMapping` / `OnStateTagsChangedEvent` / `TryAuthorizeAbility` / `TryAuthorizeAbilitiesByTag` / `TryAuthorizeAbilityByClass` / `TryRevokeTrigger` / `TryRevokeAbilitiesByTag` / `AddStateTag` / `RemoveStateTag` / `HasStateTag` / `GetStateTags` / `EvaluateAuthorizationGates` / `MatchesAnyFilterTag` / `RevokeAbility`。
  - `FSingularisGeneralAbilityAnimusInput`：`InputAction` / `TriggerTag` / `bWhileHeld`。
  - `USingularisGeneralAbilityAnimusComponent`：`ServerTryAuthorizeAbility` / `ServerTryRevokeTrigger` / `HandleAbilityAuthorize` / `HandleAbilityRevoke`。

- [ ] **Step 1: 创建规则判定自动化测试（先行，预期构建失败）**

创建 `VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Tests/SingularisGeneralAbilityRuleTests.cpp`：

```cpp
#include "Misc/AutomationTest.h"

#include "Components/SingularisGeneralAbilityComponent.h"
#include "Types/SingularisGeneralAbilityGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 规则判定纯函数测试：门禁与过滤匹配的层级包含语义。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSingularisGeneralAbilityRuleTest,
	"Singularis.GeneralAbility.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FSingularisGeneralAbilityRuleTest::RunTest(const FString& Parameters)
{
	// 1) 过滤匹配：子标签命中父过滤器，父标签不命中子过滤器
	TestTrue(
		TEXT("子标签应命中父过滤器"),
		USingularisGeneralAbilityComponent::MatchesAnyFilterTag(
			SingularisGeneral_Ability_Trigger_Default,
			SingularisGeneral_Ability_Trigger.GetSingleTagContainer()
		)
	);
	TestFalse(
		TEXT("父标签不应命中子过滤器"),
		USingularisGeneralAbilityComponent::MatchesAnyFilterTag(
			SingularisGeneral_Ability_Trigger,
			SingularisGeneral_Ability_Trigger_Default.GetSingleTagContainer()
		)
	);

	// 2) 过滤匹配：无关标签与非法输入
	TestFalse(
		TEXT("无关标签不应命中"),
		USingularisGeneralAbilityComponent::MatchesAnyFilterTag(
			SingularisGeneral_Ability_Identity,
			SingularisGeneral_Ability_State.GetSingleTagContainer()
		)
	);
	TestFalse(
		TEXT("无效标签不应命中"),
		USingularisGeneralAbilityComponent::MatchesAnyFilterTag(
			FGameplayTag(),
			SingularisGeneral_Ability_Trigger.GetSingleTagContainer()
		)
	);
	TestFalse(
		TEXT("空过滤集不应命中"),
		USingularisGeneralAbilityComponent::MatchesAnyFilterTag(
			SingularisGeneral_Ability_Trigger,
			FGameplayTagContainer()
		)
	);

	// 3) 门禁：必需由子标签满足，禁止由子标签触发
	FGameplayTagContainer ProbeState;
	ProbeState.AddTag(SingularisGeneral_Ability_Trigger_Default);

	TestTrue(
		TEXT("空要求应通过门禁"),
		USingularisGeneralAbilityComponent::EvaluateAuthorizationGates(
			ProbeState,
			FGameplayTagContainer(),
			FGameplayTagContainer()
		)
	);
	TestTrue(
		TEXT("必需标签应由子标签满足"),
		USingularisGeneralAbilityComponent::EvaluateAuthorizationGates(
			ProbeState,
			SingularisGeneral_Ability_Trigger.GetSingleTagContainer(),
			FGameplayTagContainer()
		)
	);
	TestFalse(
		TEXT("必需标签缺失应拒绝"),
		USingularisGeneralAbilityComponent::EvaluateAuthorizationGates(
			ProbeState,
			SingularisGeneral_Ability_Identity.GetSingleTagContainer(),
			FGameplayTagContainer()
		)
	);
	TestFalse(
		TEXT("禁止标签被命中应拒绝"),
		USingularisGeneralAbilityComponent::EvaluateAuthorizationGates(
			ProbeState,
			FGameplayTagContainer(),
			SingularisGeneral_Ability_Trigger.GetSingleTagContainer()
		)
	);
	TestTrue(
		TEXT("禁止标签无关应通过"),
		USingularisGeneralAbilityComponent::EvaluateAuthorizationGates(
			ProbeState,
			FGameplayTagContainer(),
			SingularisGeneral_Ability_Identity.GetSingleTagContainer()
		)
	);

	return true;
}

#endif
```

- [ ] **Step 2: 构建，确认测试因缺少纯函数而失败**

构建 `VehicleTourEditor`。

预期：编译失败，`EvaluateAuthorizationGates` 与 `MatchesAnyFilterTag` 未声明（这是本任务的失败测试基线）。

- [ ] **Step 3: 整体替换 `SingularisGeneralAbilityBase.h`**

```cpp
#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <UObject/Object.h>

#include "Types/SingularisGeneralAbilityType.h"
#include "SingularisGeneralAbilityBase.generated.h"

class USingularisGeneralAbilityComponent;
struct FSingularisGeneralAbilityContext;

/**
 * 引力奇点通用能力抽象基类。
 *
 * 作为 UObject 子对象挂载于 USingularisGeneralAbilityComponent，
 * 通过 AddReplicatedSubObject 注册至组件级复制列表，实现属性复制的网络同步。
 *
 * 生命周期为服务器权威：授权（Authorize）后进入授权状态，持续能力逐帧调用 Sustain，
 * 直至被撤销（Revoke）。撤销由通用能力组件统一编排，原因见 ESingularisGeneralAbilityEndReason。
 * 能力不感知输入；输入相位由意志组件翻译为触发信号。
 *
 * Blueprint 子类如需客户端反馈（视觉效果、音效、UI 变化等），可重写 OnClientAuthorized /
 * OnClientRevoked 驱动客户端表现逻辑。
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, CollapseCategories)
class SINGULARISGENERALABILITY_API USingularisGeneralAbility : public UObject
{
	GENERATED_BODY()

public:
#pragma region Parameter

	/**
	 * 身份标签。状态层寻址键：撤销、打断与查询均按此标签匹配（层级包含语义）。
	 * 约定位于 Singularis.General.Ability.Identity 子树；无效时仅状态层寻址不可用。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneral|引力奇点通用能力|参数",
		meta = (
			DisplayName = "身份标签",
			Categories = "Singularis.General.Ability.Identity",
			ForceSelection = "true"
		)
	)
	FGameplayTag IdentityTag{};

	/** 能力策略：瞬时在 Authorize 返回后立即结束；持续保持授权状态直至被撤销。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneral|引力奇点通用能力|参数",
		meta = (DisplayName = "能力策略")
	)
	ESingularisGeneralAbilityPolicy Policy = ESingularisGeneralAbilityPolicy::Instant;

	/** 授权必需：状态容器须包含全部标签（层级包含）。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneral|引力奇点通用能力|行为规则",
		meta = (
			DisplayName = "必需标签",
			Categories = "Singularis.General.Ability.State",
			ForceSelection = "true"
		)
	)
	FGameplayTagContainer RequiredTags{};

	/** 授权禁止：状态容器命中任一标签即拒绝授权（层级包含）。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneral|引力奇点通用能力|行为规则",
		meta = (
			DisplayName = "禁止标签",
			Categories = "Singularis.General.Ability.State",
			ForceSelection = "true"
		)
	)
	FGameplayTagContainer BlockedTags{};

	/** 授权授予：授权期间写入状态容器，撤销时清除；仅持续能力有效。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneral|引力奇点通用能力|行为规则",
		meta = (
			DisplayName = "拥有标签",
			Categories = "Singularis.General.Ability.State",
			ForceSelection = "true"
		)
	)
	FGameplayTagContainer OwnedTags{};

	/** 授权时打断：撤销身份标签命中的已授权能力（原因被打断）；瞬时能力同样生效。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneral|引力奇点通用能力|行为规则",
		meta = (
			DisplayName = "授权时打断标签",
			Categories = "Singularis.General.Ability.Identity",
			ForceSelection = "true"
		)
	)
	FGameplayTagContainer CancelAbilitiesWithTag{};

	/** 状态打断：状态容器新增标签命中时撤销自身（原因被打断）；仅持续能力有效。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneral|引力奇点通用能力|行为规则",
		meta = (
			DisplayName = "被状态打断标签",
			Categories = "Singularis.General.Ability.State",
			ForceSelection = "true"
		)
	)
	FGameplayTagContainer CanceledByTags{};

#pragma endregion

#pragma region State

	/** 授权状态标记。复制至客户端并触发 OnRep_IsAuthorized；仅由通用能力组件写入。 */
	UPROPERTY(ReplicatedUsing = OnRep_IsAuthorized)
	bool bIsAuthorized = false;

	/** 本次授权的触发标签。瞬态，仅服务器有效；程序化授权时为无效标签。 */
	UPROPERTY(Transient)
	FGameplayTag AuthorizationTriggerTag{};

	/** 本次授权的上下文。瞬态，仅服务器有效；授权期间由组件缓存。 */
	UPROPERTY(Transient)
	FSingularisGeneralAbilityContext AuthorizationContext{};

#pragma endregion

#pragma region UObject Interface

	virtual UWorld* GetWorld() const override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool IsSupportedForNetworking() const override;
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override;

#pragma endregion

#pragma region SPI

	/**
	 * 前置检查。声明式门禁通过后执行；返回 false 跳过授权。该方法仅在服务器执行。
	 *
	 * @param Context 包含 Controller、Instigator、Avatar 等运行时上下文信息
	 * @return 是否满足授权条件
	 */
	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "SingularisGeneral|引力奇点通用能力|SPI",
		meta = (DisplayName = "CanAuthorize")
	)
	bool CanAuthorize(const FSingularisGeneralAbilityContext& Context) const;

	/**
	 * 授权：执行能力核心行为逻辑。该方法仅在服务器执行。
	 *
	 * @param Context 包含 Controller、Instigator、Avatar 等运行时上下文信息
	 */
	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "SingularisGeneral|引力奇点通用能力|SPI",
		meta = (DisplayName = "Authorize")
	)
	void Authorize(const FSingularisGeneralAbilityContext& Context);

	/**
	 * 持续：持续能力的逐帧回调。仅在服务器、且能力处于授权状态时调用。
	 *
	 * @param DeltaTime 帧间隔时间（秒）
	 */
	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "SingularisGeneral|引力奇点通用能力|SPI",
		meta = (DisplayName = "Sustain")
	)
	void Sustain(float DeltaTime);

	/**
	 * 撤销钩子：撤销流程中执行清理（释放物理柄、注销委托等）。该方法仅在服务器执行。
	 * 调用时授权标记已清除，拥有标签尚未撤销。
	 *
	 * @param Reason 撤销原因
	 */
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "SingularisGeneral|引力奇点通用能力|SPI",
		meta = (DisplayName = "OnRevoked")
	)
	void OnRevoked(ESingularisGeneralAbilityEndReason Reason);

	/** 客户端授权钩子：收到授权状态复制时触发，仅客户端调用。 */
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "SingularisGeneral|引力奇点通用能力|SPI",
		meta = (DisplayName = "OnClientAuthorized")
	)
	void OnClientAuthorized();

	/** 客户端撤销钩子：收到撤销状态复制时触发，仅客户端调用。 */
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "SingularisGeneral|引力奇点通用能力|SPI",
		meta = (DisplayName = "OnClientRevoked")
	)
	void OnClientRevoked();

#pragma endregion

#pragma region API

	/**
	 * 请求撤销自身。幂等；仅服务器生效（内部权威卫语句）。
	 *
	 * @param Reason 撤销原因
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "SingularisGeneral|引力奇点通用能力|API",
		meta = (DisplayName = "Revoke")
	)
	void Revoke(ESingularisGeneralAbilityEndReason Reason);

	/** 是否处于授权状态。 */
	UFUNCTION(
		BlueprintPure,
		Category = "SingularisGeneral|引力奇点通用能力|API",
		meta = (DisplayName = "IsAuthorized")
	)
	bool IsAuthorized() const { return bIsAuthorized; }

	/** 获取本次授权的上下文（仅服务器、仅授权期间有效）。 */
	UFUNCTION(
		BlueprintPure,
		Category = "SingularisGeneral|引力奇点通用能力|API",
		meta = (DisplayName = "GetAuthorizationContext")
	)
	FSingularisGeneralAbilityContext GetAuthorizationContext() const { return AuthorizationContext; }

	/** 获取所属通用能力组件。 */
	UFUNCTION(
		BlueprintPure,
		Category = "SingularisGeneral|引力奇点通用能力|API",
		meta = (DisplayName = "GetOwningAbilityComponent")
	)
	USingularisGeneralAbilityComponent* GetOwningAbilityComponent() const;

#pragma endregion

#pragma region Callback

	UFUNCTION()
	void OnRep_IsAuthorized();

#pragma endregion
};
```

- [ ] **Step 4: 整体替换 `SingularisGeneralAbilityBase.cpp`**

```cpp
#include "Objects/SingularisGeneralAbilityBase.h"

#include <Engine/NetDriver.h>
#include <GameFramework/Actor.h>
#include <Net/UnrealNetwork.h>

#include "Components/SingularisGeneralAbilityComponent.h"

UWorld* USingularisGeneralAbility::GetWorld() const
{
	// 1) 排除 CDO：防止在编辑器启动或序列化时获取错误的上下文
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;

	// 2) 通过 Outer 链（AbilityComponent → OwnerActor）获取 WorldContext
	if (const UObject* Outer = GetOuter()) return Outer->GetWorld();

	return Super::GetWorld();
}

void USingularisGeneralAbility::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USingularisGeneralAbility, bIsAuthorized);
}

bool USingularisGeneralAbility::IsSupportedForNetworking() const
{
	return true;
}

int32 USingularisGeneralAbility::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	// 1) CDO 不支持网络调用，直接返回 Local
	if (HasAnyFlags(RF_ClassDefaultObject) || !IsSupportedForNetworking()) return FunctionCallspace::Local;

	// 2) 通过 Outer（AbilityComponent）链式委托，由 UActorComponent::GetFunctionCallspace 再委托至 Owner Actor
	return GetOuter()->GetFunctionCallspace(Function, Stack);
}

bool USingularisGeneralAbility::CallRemoteFunction(
	UFunction* Function,
	void* Parms,
	FOutParmRec* OutParms,
	FFrame* Stack
)
{
	// 1) CDO 不支持网络调用
	if (HasAnyFlags(RF_ClassDefaultObject)) return false;

	// 2) 沿 Outer 链查找 Owner Actor，通过其 NetDriver 转发 RPC
	AActor* OwnerActor = GetTypedOuter<AActor>();
	if (!IsValid(OwnerActor)) return false;

	UNetDriver* NetDriver = OwnerActor->GetNetDriver();
	if (!IsValid(NetDriver)) return false;

	// 3) 将 this（子对象）作为最后一个参数传入，使 NetDriver 正确路由子对象上的 RPC
	NetDriver->ProcessRemoteFunction(OwnerActor, Function, Parms, OutParms, Stack, this);

	return true;
}

bool USingularisGeneralAbility::CanAuthorize_Implementation(const FSingularisGeneralAbilityContext& Context) const
{
	return true;
}

void USingularisGeneralAbility::Authorize_Implementation(const FSingularisGeneralAbilityContext& Context) {}

void USingularisGeneralAbility::Sustain_Implementation(float DeltaTime) {}

void USingularisGeneralAbility::Revoke(const ESingularisGeneralAbilityEndReason Reason)
{
	// 1) 卫语句：仅服务器可请求撤销
	USingularisGeneralAbilityComponent* AbilityComponent = GetOwningAbilityComponent();
	if (!IsValid(AbilityComponent)) return;
	if (!IsValid(AbilityComponent->GetOwner()) || !AbilityComponent->GetOwner()->HasAuthority()) return;

	// 2) 委托组件统一撤销流程（幂等由流程内部保证）
	AbilityComponent->RevokeAbility(this, Reason);
}

USingularisGeneralAbilityComponent* USingularisGeneralAbility::GetOwningAbilityComponent() const
{
	return Cast<USingularisGeneralAbilityComponent>(GetOuter());
}

void USingularisGeneralAbility::OnRep_IsAuthorized()
{
	// 1) 按最新授权状态触发对应客户端钩子
	if (bIsAuthorized)
	{
		OnClientAuthorized();
	}
	else
	{
		OnClientRevoked();
	}
}
```

- [ ] **Step 5: 整体替换 `SingularisGeneralAbilityComponent.h`**

```cpp
#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Components/ActorComponent.h>

#include "Types/SingularisGeneralAbilityComponentType.h"
#include "Types/SingularisGeneralAbilityType.h"
#include "SingularisGeneralAbilityComponent.generated.h"

class AController;
class USingularisGeneralAbility;

#pragma region 委托签名

/** 状态标签变更签名。AddedTags 为新增标签，RemovedTags 为移除标签。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnStateTagsChangedSignature,
	const FGameplayTagContainer&, AddedTags,
	const FGameplayTagContainer&, RemovedTags
);

#pragma endregion

/**
 * 引力奇点通用能力组件。
 *
 * 挂载于 Actor（通常为 Pawn），承载并管理 USingularisGeneralAbility 子对象管线。
 * BeginPlay 时将全部 Instanced 能力子对象注册至复制列表；EndPlay 时先撤销全部已授权能力再注销。
 *
 * 生命周期为服务器权威：TryAuthorizeAbility 按触发标签层级匹配触发管线映射并依次执行授权例程；
 * TryRevokeTrigger 处理触发结束信号；状态容器 StateTags 承载规则判定所需的标签状态。
 */
UCLASS(
	Blueprintable,
	BlueprintType,
	ClassGroup = ("Singularis"),
	meta = (BlueprintSpawnableComponent, DisplayName = "引力奇点通用能力组件")
)
class SINGULARISGENERALABILITY_API USingularisGeneralAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region Parameter

	/**
	 * 触发管线映射。键为触发标签（支持层级匹配），值为有序能力管线。
	 * 配置于蓝图默认值，运行时不可变更。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneralAbility|通用能力组件|参数",
		meta = (
			DisplayName = "触发管线映射",
			Categories = "Singularis.General.Ability.Trigger",
			ForceSelection = "true"
		)
	)
	TMap<FGameplayTag, FSingularisGeneralAbilityPipeline> TriggerPipelineMapping{};

#pragma endregion

#pragma region Event Dispatcher

	/** 状态标签变更时广播（服务器本地变更后与客户端 OnRep 到达时均触发）。 */
	UPROPERTY(
		BlueprintAssignable,
		Category = "SingularisGeneralAbility|通用能力组件|事件分发器",
		meta = (DisplayName = "状态标签变更时")
	)
	FOnStateTagsChangedSignature OnStateTagsChangedEvent{};

#pragma endregion

private:
#pragma region State

	/** 状态容器。复制至所有客户端并触发 OnRep_StateTags。 */
	UPROPERTY(ReplicatedUsing = OnRep_StateTags)
	FGameplayTagContainer StateTags{};

#pragma endregion

public:
#pragma region Constructors

	USingularisGeneralAbilityComponent();

#pragma endregion

#pragma region ActorComponent Interface

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region API

	/**
	 * 触发授权入口。按触发标签层级匹配触发管线映射，依次执行各能力的授权例程。
	 *
	 * @param TriggerTag 用于匹配管线的触发标签
	 * @param Controller 发起请求的控制器
	 * @param InputActionValue 触发该能力的输入值
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "SingularisGeneralAbility|通用能力组件|API",
		meta = (DisplayName = "TryAuthorizeAbility")
	)
	void TryAuthorizeAbility(
		const FGameplayTag& TriggerTag,
		AController* Controller,
		const FInputActionValue& InputActionValue
	);

	/**
	 * 程序化按身份标签授权（层级匹配）。不记录触发关联，不受触发结束信号影响。
	 *
	 * @param IdentityTag 用于匹配身份标签的 GameplayTag
	 * @param Controller 发起请求的控制器
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "SingularisGeneralAbility|通用能力组件|API",
		meta = (DisplayName = "TryAuthorizeAbilitiesByTag")
	)
	void TryAuthorizeAbilitiesByTag(const FGameplayTag& IdentityTag, AController* Controller);

	/**
	 * 程序化按类授权（匹配子类）。不记录触发关联，不受触发结束信号影响。
	 *
	 * @param AbilityClass 目标能力类
	 * @param Controller 发起请求的控制器
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "SingularisGeneralAbility|通用能力组件|API",
		meta = (DisplayName = "TryAuthorizeAbilityByClass")
	)
	void TryAuthorizeAbilityByClass(TSubclassOf<USingularisGeneralAbility> AbilityClass, AController* Controller);

	/**
	 * 触发结束信号入口。撤销由该触发标签授权的全部持续能力（原因触发结束）。
	 *
	 * @param TriggerTag 触发结束信号对应的触发标签
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "SingularisGeneralAbility|通用能力组件|API",
		meta = (DisplayName = "TryRevokeTrigger")
	)
	void TryRevokeTrigger(const FGameplayTag& TriggerTag);

	/**
	 * 程序化按身份标签撤销（层级匹配）。
	 *
	 * @param IdentityTag 用于匹配身份标签的 GameplayTag
	 * @param Reason 撤销原因
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "SingularisGeneralAbility|通用能力组件|API",
		meta = (DisplayName = "TryRevokeAbilitiesByTag")
	)
	void TryRevokeAbilitiesByTag(const FGameplayTag& IdentityTag, ESingularisGeneralAbilityEndReason Reason);

	/**
	 * 新增状态标签。幂等；新增后执行状态打断扫描。
	 *
	 * @param Tag 状态标签
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "SingularisGeneralAbility|通用能力组件|API",
		meta = (DisplayName = "AddStateTag")
	)
	void AddStateTag(const FGameplayTag& Tag);

	/**
	 * 移除状态标签。幂等；不触发打断扫描。
	 *
	 * @param Tag 状态标签
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "SingularisGeneralAbility|通用能力组件|API",
		meta = (DisplayName = "RemoveStateTag")
	)
	void RemoveStateTag(const FGameplayTag& Tag);

#pragma endregion

#pragma region SPI

	/** 是否包含状态标签（层级包含）。 */
	UFUNCTION(
		BlueprintPure,
		Category = "SingularisGeneralAbility|通用能力组件|SPI",
		meta = (DisplayName = "HasStateTag")
	)
	bool HasStateTag(const FGameplayTag& Tag) const { return Tag.IsValid() && StateTags.HasTag(Tag); }

	/** 获取状态容器副本。 */
	UFUNCTION(
		BlueprintPure,
		Category = "SingularisGeneralAbility|通用能力组件|SPI",
		meta = (DisplayName = "GetStateTags")
	)
	FGameplayTagContainer GetStateTags() const { return StateTags; }

	/**
	 * 门禁判定：必需标签全部满足且禁止标签无一命中（层级包含语义）。
	 *
	 * @param InStateTags 状态容器
	 * @param RequiredTags 必需标签集
	 * @param BlockedTags 禁止标签集
	 * @return 是否通过门禁
	 */
	static bool EvaluateAuthorizationGates(
		const FGameplayTagContainer& InStateTags,
		const FGameplayTagContainer& RequiredTags,
		const FGameplayTagContainer& BlockedTags
	);

	/**
	 * 过滤匹配：标签等于某过滤器或位于其子树内。
	 *
	 * @param Tag 待判定标签
	 * @param FilterTags 过滤器集合
	 * @return 是否命中任一过滤器
	 */
	static bool MatchesAnyFilterTag(const FGameplayTag& Tag, const FGameplayTagContainer& FilterTags);

	/**
	 * 统一撤销流程（幂等）。供能力基类 Revoke 调用；仅权威端执行。
	 *
	 * @param Ability 目标能力
	 * @param Reason 撤销原因
	 */
	void RevokeAbility(USingularisGeneralAbility* Ability, ESingularisGeneralAbilityEndReason Reason);

#pragma endregion

private:
#pragma region Internal Function

	/**
	 * 统一授权例程。门禁 → CanAuthorize → 冲突打断 → 授权状态与拥有标签 → Authorize。
	 *
	 * @param Ability 目标能力
	 * @param TriggerTag 触发标签；程序化授权时传入无效标签
	 * @param Context 执行上下文
	 * @return 是否完成授权
	 */
	bool TryAuthorizeAbilityInstance(
		USingularisGeneralAbility* Ability,
		const FGameplayTag& TriggerTag,
		const FSingularisGeneralAbilityContext& Context
	);

	/** 收集触发管线映射内全部有效能力实例（按实例去重）。 */
	void CollectUniqueAbilities(TArray<USingularisGeneralAbility*>& OutAbilities) const;

	/** 逐帧回调全部已授权能力。 */
	void SustainAuthorizedAbilities(float DeltaTime);

	/** 按过滤集撤销：身份标签命中的已授权能力（原因被打断）。 */
	void RevokeAbilitiesByFilter(const FGameplayTagContainer& FilterTags);

	/** 状态标签新增后的打断扫描。 */
	void ScanStateTagAdded(const FGameplayTag& AddedTag);

	/** 内部状态标签写入：权威校验、幂等、事件广播与打断扫描。 */
	void ApplyStateTagAdded(const FGameplayTag& Tag);
	void ApplyStateTagRemoved(const FGameplayTag& Tag);

	/** 授予拥有标签（仅持续能力）。 */
	void GrantOwnedTags(USingularisGeneralAbility* Ability);

	/** 撤销拥有标签（仍有其它已授权能力拥有时保留）。 */
	void RevokeOwnedTags(const USingularisGeneralAbility* Ability);

	/** 依据是否存在已授权能力刷新组件逐帧开关。 */
	void RefreshComponentTick();

	/** 广播状态标签变更事件。 */
	void BroadcastStateTagsChanged(const FGameplayTagContainer& AddedTags, const FGameplayTagContainer& RemovedTags);

	/** 将全部有效能力实例添加至网络复制列表（按实例去重，仅服务器）。 */
	void RegisterAbilitySubObjects();

	/** 将全部有效能力实例从网络复制列表移除（按实例去重，仅服务器）。 */
	void UnregisterAbilitySubObjects();

#pragma endregion

#pragma region Callback

	/**
	 * 状态容器复制回调。计算差异并广播状态标签变更事件。
	 *
	 * @param PreviousStateTags 复制前的旧容器
	 */
	UFUNCTION()
	void OnRep_StateTags(const FGameplayTagContainer& PreviousStateTags);

#pragma endregion
};
```

- [ ] **Step 6: 整体替换 `SingularisGeneralAbilityComponent.cpp`**

```cpp
#include "Components/SingularisGeneralAbilityComponent.h"

#include <GameFramework/Controller.h>
#include <Net/UnrealNetwork.h>

#include "Objects/SingularisGeneralAbilityBase.h"
#include "SingularisGeneralAbility.h"
#include "Types/SingularisGeneralAbilityType.h"

USingularisGeneralAbilityComponent::USingularisGeneralAbilityComponent()
{
	// 1) 启用组件级复制，使用显式注册的子对象列表进行属性同步
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;

	// 2) 逐帧能力默认关闭，由生命周期编排在存在已授权能力时开启
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bAutoActivate = true;
}

void USingularisGeneralAbilityComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1) 在服务器端将全部 Instanced 能力子对象注册至复制列表
	RegisterAbilitySubObjects();
}

void USingularisGeneralAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 1) 权威端：先撤销全部已授权能力（销毁原因），保证清理钩子可见完整上下文
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TArray<USingularisGeneralAbility*> Abilities;
		CollectUniqueAbilities(Abilities);

		for (USingularisGeneralAbility* Ability : Abilities)
		{
			RevokeAbility(Ability, ESingularisGeneralAbilityEndReason::Destroyed);
		}
	}

	// 2) 将全部能力子对象从网络复制列表中移除
	UnregisterAbilitySubObjects();

	Super::EndPlay(EndPlayReason);
}

void USingularisGeneralAbilityComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 1) 卫语句：逐帧仅在权威端执行
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 逐帧回调已授权能力
	SustainAuthorizedAbilities(DeltaTime);
}

void USingularisGeneralAbilityComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USingularisGeneralAbilityComponent, StateTags);
}

void USingularisGeneralAbilityComponent::TryAuthorizeAbility(
	const FGameplayTag& TriggerTag,
	AController* Controller,
	const FInputActionValue& InputActionValue
)
{
	// 1) 卫语句：服务器权威与控制器校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Controller)) return;

	// 2) 组装执行上下文，将执行主体泛化为 Avatar
	FSingularisGeneralAbilityContext Context;
	Context.Controller = Controller;
	Context.Instigator = Controller->GetPawn();
	Context.Avatar = GetOwner();
	Context.Target = GetOwner();
	Context.AbilityComponent = this;
	Context.InputValue = InputActionValue;

	// 3) 遍历触发管线映射，使用触发标签层级匹配后按序执行授权例程
	for (const auto& [Tag, Pipeline] : TriggerPipelineMapping)
	{
		if (!Tag.MatchesTag(TriggerTag)) continue;

		for (const FSingularisGeneralAbilityEntry& Entry : Pipeline.Abilities)
		{
			TryAuthorizeAbilityInstance(Entry.Ability, TriggerTag, Context);
		}
	}
}

void USingularisGeneralAbilityComponent::TryAuthorizeAbilitiesByTag(
	const FGameplayTag& IdentityTag,
	AController* Controller
)
{
	// 1) 卫语句：服务器权威、控制器与标签校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Controller) || !IdentityTag.IsValid()) return;

	// 2) 组装执行上下文（无触发标签与输入值）
	FSingularisGeneralAbilityContext Context;
	Context.Controller = Controller;
	Context.Instigator = Controller->GetPawn();
	Context.Avatar = GetOwner();
	Context.Target = GetOwner();
	Context.AbilityComponent = this;

	// 3) 收集并授权身份标签命中的能力（不记录触发关联）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IdentityTag.IsValid()) continue;
		if (!Ability->IdentityTag.MatchesTag(IdentityTag)) continue;

		TryAuthorizeAbilityInstance(Ability, FGameplayTag(), Context);
	}
}

void USingularisGeneralAbilityComponent::TryAuthorizeAbilityByClass(
	const TSubclassOf<USingularisGeneralAbility> AbilityClass,
	AController* Controller
)
{
	// 1) 卫语句：服务器权威、控制器与类校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Controller) || !AbilityClass) return;

	// 2) 组装执行上下文（无触发标签与输入值）
	FSingularisGeneralAbilityContext Context;
	Context.Controller = Controller;
	Context.Instigator = Controller->GetPawn();
	Context.Avatar = GetOwner();
	Context.Target = GetOwner();
	Context.AbilityComponent = this;

	// 3) 收集并授权类匹配的能力（不记录触发关联）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsA(AbilityClass)) continue;

		TryAuthorizeAbilityInstance(Ability, FGameplayTag(), Context);
	}
}

void USingularisGeneralAbilityComponent::TryRevokeTrigger(const FGameplayTag& TriggerTag)
{
	// 1) 卫语句：服务器权威与标签校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!TriggerTag.IsValid()) return;

	// 2) 收集触发标签精确关联的已授权能力（先收集后撤销，避免遍历中变更）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	TArray<USingularisGeneralAbility*> Targets;
	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsAuthorized()) continue;
		if (Ability->AuthorizationTriggerTag != TriggerTag) continue;

		Targets.Add(Ability);
	}

	// 3) 统一撤销（原因：触发结束）
	for (USingularisGeneralAbility* Ability : Targets)
	{
		RevokeAbility(Ability, ESingularisGeneralAbilityEndReason::TriggerEnded);
	}
}

void USingularisGeneralAbilityComponent::TryRevokeAbilitiesByTag(
	const FGameplayTag& IdentityTag,
	const ESingularisGeneralAbilityEndReason Reason
)
{
	// 1) 卫语句：服务器权威与标签校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IdentityTag.IsValid()) return;

	// 2) 收集身份标签命中的已授权能力（先收集后撤销，避免遍历中变更）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	TArray<USingularisGeneralAbility*> Targets;
	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsAuthorized()) continue;
		if (!MatchesAnyFilterTag(Ability->IdentityTag, IdentityTag.GetSingleTagContainer())) continue;

		Targets.Add(Ability);
	}

	// 3) 统一撤销
	for (USingularisGeneralAbility* Ability : Targets)
	{
		RevokeAbility(Ability, Reason);
	}
}

void USingularisGeneralAbilityComponent::AddStateTag(const FGameplayTag& Tag)
{
	ApplyStateTagAdded(Tag);
}

void USingularisGeneralAbilityComponent::RemoveStateTag(const FGameplayTag& Tag)
{
	ApplyStateTagRemoved(Tag);
}

bool USingularisGeneralAbilityComponent::EvaluateAuthorizationGates(
	const FGameplayTagContainer& InStateTags,
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& BlockedTags
)
{
	// 1) 必需标签：须全部满足（层级包含）
	if (!RequiredTags.IsEmpty() && !InStateTags.HasAll(RequiredTags)) return false;

	// 2) 禁止标签：不得命中任一（层级包含）
	if (!BlockedTags.IsEmpty() && InStateTags.HasAny(BlockedTags)) return false;

	return true;
}

bool USingularisGeneralAbilityComponent::MatchesAnyFilterTag(
	const FGameplayTag& Tag,
	const FGameplayTagContainer& FilterTags
)
{
	// 1) 卫语句：无效标签或空过滤集不匹配
	if (!Tag.IsValid() || FilterTags.IsEmpty()) return false;

	// 2) 层级包含：标签等于某过滤器或位于其子树内
	for (const FGameplayTag& FilterTag : FilterTags.GetGameplayTagArray())
	{
		if (FilterTag.IsValid() && Tag.MatchesTag(FilterTag)) return true;
	}

	return false;
}

void USingularisGeneralAbilityComponent::RevokeAbility(
	USingularisGeneralAbility* Ability,
	const ESingularisGeneralAbilityEndReason Reason
)
{
	// 1) 卫语句：仅权威端处理；未授权则无操作（幂等）
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Ability) || !Ability->IsAuthorized()) return;

	// 2) 先行清除授权标记，保证清理期间的重入安全
	Ability->bIsAuthorized = false;

	// 3) 清理钩子（此时拥有标签尚未撤销）
	Ability->OnRevoked(Reason);

	// 4) 撤销拥有标签（多拥有者检查）
	RevokeOwnedTags(Ability);

	// 5) 清空授权上下文
	Ability->AuthorizationContext = FSingularisGeneralAbilityContext();
	Ability->AuthorizationTriggerTag = FGameplayTag();

	// 6) 依据剩余已授权能力刷新逐帧开关
	RefreshComponentTick();

	UE_LOG(
		LogSingularisGeneralAbility,
		Verbose,
		TEXT("能力 %s 已撤销（原因：%d）"),
		*Ability->GetName(),
		static_cast<int32>(Reason)
	);
}

bool USingularisGeneralAbilityComponent::TryAuthorizeAbilityInstance(
	USingularisGeneralAbility* Ability,
	const FGameplayTag& TriggerTag,
	const FSingularisGeneralAbilityContext& Context
)
{
	// 1) 卫语句：能力有效且未授权（非重入）
	if (!IsValid(Ability) || Ability->IsAuthorized()) return false;

	// 2) 门禁：必需 / 禁止标签判定
	if (!EvaluateAuthorizationGates(StateTags, Ability->RequiredTags, Ability->BlockedTags))
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Log,
			TEXT("能力 %s 被门禁拒绝（必需：%s；禁止：%s；状态：%s）"),
			*Ability->GetName(),
			*Ability->RequiredTags.ToString(),
			*Ability->BlockedTags.ToString(),
			*StateTags.ToString()
		);
		return false;
	}

	// 3) 命令式前置检查
	if (!Ability->CanAuthorize(Context)) return false;

	// 4) 冲突打断：按授权时打断标签撤销命中者
	RevokeAbilitiesByFilter(Ability->CancelAbilitiesWithTag);

	// 5) 持续能力：进入授权状态并授予拥有标签
	if (Ability->Policy == ESingularisGeneralAbilityPolicy::Sustained)
	{
		Ability->bIsAuthorized = true;
		Ability->AuthorizationTriggerTag = TriggerTag;
		Ability->AuthorizationContext = Context;
		GrantOwnedTags(Ability);
		RefreshComponentTick();
	}

	// 6) 执行授权逻辑
	Ability->Authorize(Context);

	UE_LOG(LogSingularisGeneralAbility, Verbose, TEXT("能力 %s 已授权"), *Ability->GetName());

	return true;
}

void USingularisGeneralAbilityComponent::CollectUniqueAbilities(
	TArray<USingularisGeneralAbility*>& OutAbilities
) const
{
	OutAbilities.Reset();

	// 1) 遍历触发管线映射，按实例去重后收集
	TSet<USingularisGeneralAbility*> Visited;
	for (const auto& [Tag, Pipeline] : TriggerPipelineMapping)
	{
		for (const FSingularisGeneralAbilityEntry& Entry : Pipeline.Abilities)
		{
			USingularisGeneralAbility* Ability = Entry.Ability;
			if (!IsValid(Ability) || Visited.Contains(Ability)) continue;

			Visited.Add(Ability);
			OutAbilities.Add(Ability);
		}
	}
}

void USingularisGeneralAbilityComponent::SustainAuthorizedAbilities(const float DeltaTime)
{
	// 1) 遍历去重后的能力集合，逐帧回调已授权者
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (IsValid(Ability) && Ability->IsAuthorized()) Ability->Sustain(DeltaTime);
	}
}

void USingularisGeneralAbilityComponent::RevokeAbilitiesByFilter(const FGameplayTagContainer& FilterTags)
{
	// 1) 卫语句：空过滤集无操作
	if (FilterTags.IsEmpty()) return;

	// 2) 收集身份标签命中的已授权能力（先收集后撤销，避免遍历中变更）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	TArray<USingularisGeneralAbility*> Targets;
	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsAuthorized()) continue;
		if (!MatchesAnyFilterTag(Ability->IdentityTag, FilterTags)) continue;

		Targets.Add(Ability);
	}

	// 3) 统一撤销（原因：被打断）
	for (USingularisGeneralAbility* Ability : Targets)
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Log,
			TEXT("能力 %s 被冲突打断（过滤：%s）"),
			*Ability->GetName(),
			*FilterTags.ToString()
		);
		RevokeAbility(Ability, ESingularisGeneralAbilityEndReason::Canceled);
	}
}

void USingularisGeneralAbilityComponent::ScanStateTagAdded(const FGameplayTag& AddedTag)
{
	// 1) 收集被状态打断标签命中新增标签的已授权能力（先收集后撤销，避免遍历中变更）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	TArray<USingularisGeneralAbility*> Targets;
	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsAuthorized()) continue;
		if (!MatchesAnyFilterTag(AddedTag, Ability->CanceledByTags)) continue;

		Targets.Add(Ability);
	}

	// 2) 统一撤销（原因：被打断）
	for (USingularisGeneralAbility* Ability : Targets)
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Log,
			TEXT("能力 %s 被状态打断（新增标签：%s）"),
			*Ability->GetName(),
			*AddedTag.ToString()
		);
		RevokeAbility(Ability, ESingularisGeneralAbilityEndReason::Canceled);
	}
}

void USingularisGeneralAbilityComponent::ApplyStateTagAdded(const FGameplayTag& Tag)
{
	// 1) 卫语句：权威校验与幂等（集合语义）
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!Tag.IsValid() || StateTags.HasTagExact(Tag)) return;

	// 2) 写入并广播
	StateTags.AddTag(Tag);
	BroadcastStateTagsChanged(Tag.GetSingleTagContainer(), FGameplayTagContainer());

	// 3) 状态打断扫描
	ScanStateTagAdded(Tag);
}

void USingularisGeneralAbilityComponent::ApplyStateTagRemoved(const FGameplayTag& Tag)
{
	// 1) 卫语句：权威校验与幂等
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!Tag.IsValid() || !StateTags.HasTagExact(Tag)) return;

	// 2) 移除并广播（移除不触发打断扫描）
	StateTags.RemoveTag(Tag);
	BroadcastStateTagsChanged(FGameplayTagContainer(), Tag.GetSingleTagContainer());
}

void USingularisGeneralAbilityComponent::GrantOwnedTags(USingularisGeneralAbility* Ability)
{
	// 1) 卫语句：仅持续能力持有拥有标签
	if (!IsValid(Ability) || Ability->Policy != ESingularisGeneralAbilityPolicy::Sustained) return;

	// 2) 逐个授予（内部写入含广播与打断扫描）
	for (const FGameplayTag& Tag : Ability->OwnedTags.GetGameplayTagArray())
	{
		ApplyStateTagAdded(Tag);
	}
}

void USingularisGeneralAbilityComponent::RevokeOwnedTags(const USingularisGeneralAbility* Ability)
{
	// 1) 卫语句
	if (!IsValid(Ability)) return;

	// 2) 收集全部已授权能力，供多拥有者检查
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	// 3) 逐个撤销：仍有其它已授权能力拥有该标签时保留
	for (const FGameplayTag& Tag : Ability->OwnedTags.GetGameplayTagArray())
	{
		bool bStillOwned = false;
		for (const USingularisGeneralAbility* Other : Abilities)
		{
			if (Other == Ability || !IsValid(Other) || !Other->IsAuthorized()) continue;
			if (!Other->OwnedTags.HasTagExact(Tag)) continue;

			bStillOwned = true;
			break;
		}

		if (bStillOwned) continue;

		ApplyStateTagRemoved(Tag);
	}
}

void USingularisGeneralAbilityComponent::RefreshComponentTick()
{
	// 1) 卫语句：仅权威端管理逐帧开关
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 存在已授权能力时开启，否则关闭
	bool bHasAuthorizedAbility = false;

	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (const USingularisGeneralAbility* Ability : Abilities)
	{
		if (IsValid(Ability) && Ability->IsAuthorized())
		{
			bHasAuthorizedAbility = true;
			break;
		}
	}

	SetComponentTickEnabled(bHasAuthorizedAbility);
}

void USingularisGeneralAbilityComponent::BroadcastStateTagsChanged(
	const FGameplayTagContainer& AddedTags,
	const FGameplayTagContainer& RemovedTags
)
{
	// 1) 卫语句：无差异不广播
	if (AddedTags.IsEmpty() && RemovedTags.IsEmpty()) return;

	OnStateTagsChangedEvent.Broadcast(AddedTags, RemovedTags);
}

void USingularisGeneralAbilityComponent::RegisterAbilitySubObjects()
{
	// 1) 仅服务器端执行子对象注册
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 将全部有效能力实例添加至网络复制列表（按实例去重）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
	{
		AddReplicatedSubObject(Ability);
	}
}

void USingularisGeneralAbilityComponent::UnregisterAbilitySubObjects()
{
	// 1) 仅服务器端执行子对象注销
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 将全部有效能力实例从网络复制列表中移除（按实例去重）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
	{
		RemoveReplicatedSubObject(Ability);
	}
}

void USingularisGeneralAbilityComponent::OnRep_StateTags(const FGameplayTagContainer& PreviousStateTags)
{
	// 1) 计算差异（客户端无权威变更路径，OnRep 为唯一广播入口）
	FGameplayTagContainer AddedTags;
	FGameplayTagContainer RemovedTags;

	for (const FGameplayTag& Tag : StateTags.GetGameplayTagArray())
	{
		if (!PreviousStateTags.HasTagExact(Tag)) AddedTags.AddTag(Tag);
	}

	for (const FGameplayTag& Tag : PreviousStateTags.GetGameplayTagArray())
	{
		if (!StateTags.HasTagExact(Tag)) RemovedTags.AddTag(Tag);
	}

	// 2) 广播
	BroadcastStateTagsChanged(AddedTags, RemovedTags);
}
```

- [ ] **Step 7: 更新 `SingularisGeneralAbilityComponentType.h` 管线文档注释**

将：

```cpp
/**
 * 引力奇点通用能力管线。
 * 包装一组有序的能力条目。管线内各能力按数组顺序依次执行 CanActivate / Activate。
 */
```

替换为：

```cpp
/**
 * 引力奇点通用能力管线。
 * 包装一组有序的能力条目。管线内各能力按数组顺序依次执行授权例程（CanAuthorize / Authorize）。
 */
```

- [ ] **Step 8: 更新意志输入类型 `SingularisGeneralAbilityAnimusComponentType.h`**

将整个 `FSingularisGeneralAbilityAnimusInput` 结构体（含文档注释与全部字段）替换为：

```cpp
/**
 * 引力奇点通用能力意志输入映射。
 * 将 EnhancedInput 的 InputAction 与触发标签关联，用于意志组件的授权 / 触发结束信号绑定。
 */
USTRUCT(BlueprintType)
struct SINGULARISGENERALABILITY_API FSingularisGeneralAbilityAnimusInput
{
	GENERATED_BODY()

	/** EnhancedInput 输入动作资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UInputAction> InputAction = nullptr;

	/**
	 * 触发标签。Started 相位发送授权信号；限定为 "Singularis.General.Ability.Trigger" 层级。
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		meta = (
			Categories = "Singularis.General.Ability.Trigger",
			ForceSelection = "true"
		)
	)
	FGameplayTag TriggerTag{};

	/**
	 * 按住型输入声明。为 true 时 Completed 与 Canceled 相位发送触发结束信号；
	 * 为 false 时仅 Started 相位发送授权信号。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "按住型"))
	bool bWhileHeld = false;
};
```

- [ ] **Step 9: 整体替换 `SingularisGeneralAbilityAnimusComponent.h`**

```cpp
#pragma once

#include <CoreMinimal.h>
#include <Components/ActorComponent.h>

#include "Types/SingularisGeneralAbilityAnimusComponentType.h"
#include "SingularisGeneralAbilityAnimusComponent.generated.h"

struct FInputActionValue;
class USingularisGeneralAbilityComponent;
class UInputMappingContext;

/**
 * 引力奇点通用能力意志组件。
 *
 * 挂载于 APlayerController，负责将 EnhancedInput 输入事件翻译为触发信号，
 * 通过 ServerTryAuthorizeAbility / ServerTryRevokeTrigger (Reliable RPC) 发送至服务器。
 *
 * 输入相位语义：Started 始终发送授权信号；Completed 与 Canceled 仅在按住型输入（bWhileHeld）
 * 上发送触发结束信号。组件不涉及能力逻辑，输入与逻辑分离。
 *
 * 输入绑定仅对本地控制器生效。组件通过 OnPossessPawnChanged 回调自动缓存
 * 当前 Possess 的 Pawn 上的 USingularisGeneralAbilityComponent 引用。
 */
UCLASS(
	Blueprintable,
	BlueprintType,
	ClassGroup = ("Singularis"),
	meta = (BlueprintSpawnableComponent, DisplayName = "引力奇点通用能力意志组件")
)
class SINGULARISGENERALABILITY_API USingularisGeneralAbilityAnimusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region Parameter

	/**
	 * 是否在控制器更换 Possess 时自动刷新 CachedAbilityComponent。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneralAbility|通用能力意志组件|参数",
		meta = (DisplayName = "自动控制")
	)
	bool bAutoControl = true;

	/**
	 * 是否在 BeginPlay 后自动启用输入。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneralAbility|通用能力意志组件|参数",
		meta = (DisplayName = "启动启用")
	)
	bool bStartEnabled = true;

	/**
	 * EnhancedInput MappingContext 的添加优先级。数值越高优先级越高。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneralAbility|通用能力意志组件|输入",
		meta = (DisplayName = "输入优先级")
	)
	int32 InputPriority = 10;

	/**
	 * 使用的 EnhancedInput 映射上下文。
	 * 若未在蓝图默认值中指定，则自动加载插件内置默认配置。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneralAbility|通用能力意志组件|输入",
		meta = (DisplayName = "输入映射上下文")
	)
	TObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	/**
	 * 输入动作到触发标签的映射列表。
	 * 若未手动配置，则自动添加默认的 IA_GeneralAbility → Singularis.General.Ability.Trigger.Default 映射。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneralAbility|通用能力意志组件|输入",
		meta = (DisplayName = "意志输入集")
	)
	TArray<FSingularisGeneralAbilityAnimusInput> AbilityAnimusInputs{};

#pragma endregion

private:
#pragma region Internal Variable

	/** 缓存的 Owner PlayerController 引用 */
	TWeakObjectPtr<APlayerController> OwnerPlayerController = nullptr;

	/** 当前 Possess Pawn 上缓存的能力组件引用 */
	TWeakObjectPtr<USingularisGeneralAbilityComponent> CachedAbilityComponent = nullptr;

	/** 本地输入启用状态（不复制，仅控制本地 InputMappingContext 的添加/移除） */
	bool bIsEnabled = false;

#pragma endregion

public:
#pragma region Constructors

	USingularisGeneralAbilityAnimusComponent();

#pragma endregion

#pragma region ActorComponent Interface

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region API

	/**
	 * 启用本地输入。仅对本地控制器生效，状态不复制。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "SingularisGeneralAbility|通用能力意志组件|API",
		meta = (DisplayName = "Enabled")
	)
	void Enabled();

	/**
	 * 禁用本地输入。仅对本地控制器生效，状态不复制。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "SingularisGeneralAbility|通用能力意志组件|API",
		meta = (DisplayName = "Disabled")
	)
	void Disabled();

#pragma endregion

private:
#pragma region RPC

	/**
	 * Server RPC：将授权请求从客户端发送至服务器。
	 *
	 * @param AbilityComponent 目标能力组件
	 * @param TriggerTag 触发标签
	 * @param InputActionValue 触发输入值
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTryAuthorizeAbility(
		USingularisGeneralAbilityComponent* AbilityComponent,
		const FGameplayTag& TriggerTag,
		const FInputActionValue& InputActionValue
	);

	/**
	 * Server RPC：将触发结束信号从客户端发送至服务器。
	 *
	 * @param AbilityComponent 目标能力组件
	 * @param TriggerTag 触发结束信号对应的触发标签
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTryRevokeTrigger(
		USingularisGeneralAbilityComponent* AbilityComponent,
		const FGameplayTag& TriggerTag
	);

#pragma endregion

#pragma region Internal Function

	/**
	 * 设置本地输入启用状态。
	 * 仅对本地控制器生效，状态不复制。
	 *
	 * @param bInEnabled true 添加 InputMappingContext，false 移除
	 */
	void SetEnabled(bool bInEnabled);

	/** 将 AbilityAnimusInputs 中配置的输入动作按相位绑定至 EnhancedInputComponent */
	void BindInputAction();

	/** 根据当前启用状态添加或移除 InputMappingContext */
	void RefreshInputMappingContext() const;

	/** 从当前 Possess Pawn 上查找并缓存 USingularisGeneralAbilityComponent */
	void RefreshAbilityComponent();

#pragma endregion

#pragma region Callback

	/**
	 * 输入按下回调。通过 Server RPC 向服务器发送授权请求。
	 *
	 * @param InputActionValue 输入动作值
	 * @param TriggerTag 绑定时关联的触发标签
	 */
	void HandleAbilityAuthorize(const FInputActionValue& InputActionValue, const FGameplayTag TriggerTag);

	/**
	 * 输入释放 / 中止回调。通过 Server RPC 向服务器发送触发结束信号。
	 *
	 * @param TriggerTag 绑定时关联的触发标签
	 */
	void HandleAbilityRevoke(const FGameplayTag TriggerTag);

	/**
	 * Possess Pawn 变更回调。自动刷新 CachedAbilityComponent。
	 *
	 * @param OldPawn 先前的 Pawn（未使用）
	 * @param NewPawn 新 Possess 的 Pawn
	 */
	UFUNCTION()
	void OnPossessPawnChanged(APawn* OldPawn, APawn* NewPawn);

#pragma endregion
};
```

- [ ] **Step 10: 更新 `SingularisGeneralAbilityAnimusComponent.cpp`**

将默认输入条目（见 Task 1 Step 6 已改为 `SingularisGeneral_Ability_Trigger_Default`）补充第三个字段。找到：

```cpp
			{
				DefaultAbilityActionFinder.Object,
				SingularisGeneral_Ability_Trigger_Default
			}
```

替换为：

```cpp
			{
				DefaultAbilityActionFinder.Object,
				SingularisGeneral_Ability_Trigger_Default,
				false
			}
```

将 `ServerTryActivateAbility_Implementation` / `ServerTryActivateAbility_Validate` 整体替换为：

```cpp
void USingularisGeneralAbilityAnimusComponent::ServerTryAuthorizeAbility_Implementation(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& TriggerTag,
	const FInputActionValue& InputActionValue
)
{
	// 1) 服务器收到 RPC 后，将请求转发至能力组件的授权入口（HasAuthority 内部已保证）
	if (!IsValid(AbilityComponent)) return;
	if (!OwnerPlayerController.IsValid()) return;

	AbilityComponent->TryAuthorizeAbility(TriggerTag, OwnerPlayerController.Get(), InputActionValue);
}

bool USingularisGeneralAbilityAnimusComponent::ServerTryAuthorizeAbility_Validate(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& TriggerTag,
	const FInputActionValue& InputActionValue
)
{
	// 1) 仅校验能力组件有效性，防止客户端传入无效指针
	return IsValid(AbilityComponent);
}

void USingularisGeneralAbilityAnimusComponent::ServerTryRevokeTrigger_Implementation(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& TriggerTag
)
{
	// 1) 服务器收到 RPC 后，将触发结束信号转发至能力组件
	if (!IsValid(AbilityComponent)) return;

	AbilityComponent->TryRevokeTrigger(TriggerTag);
}

bool USingularisGeneralAbilityAnimusComponent::ServerTryRevokeTrigger_Validate(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& TriggerTag
)
{
	// 1) 校验能力组件与触发标签有效性
	return IsValid(AbilityComponent) && TriggerTag.IsValid();
}
```

将 `BindInputAction` 中的绑定循环替换为：

```cpp
	// 1) 遍历 AbilityAnimusInputs 配置，按输入相位绑定回调
	for (const auto& [InputAction, TriggerTag, bWhileHeld] : AbilityAnimusInputs)
	{
		if (!IsValid(InputAction) || !TriggerTag.IsValid()) continue;

		// 2) 按下相位：始终发送授权信号
		EnhancedInputComponent->BindAction(
			InputAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::HandleAbilityAuthorize,
			TriggerTag
		);

		// 3) 按住型输入：释放与中止相位发送触发结束信号
		if (!bWhileHeld) continue;

		EnhancedInputComponent->BindAction(
			InputAction,
			ETriggerEvent::Completed,
			this,
			&ThisClass::HandleAbilityRevoke,
			TriggerTag
		);

		EnhancedInputComponent->BindAction(
			InputAction,
			ETriggerEvent::Canceled,
			this,
			&ThisClass::HandleAbilityRevoke,
			TriggerTag
		);
	}
```

将 `HandleAbilityInput` 整体替换为：

```cpp
void USingularisGeneralAbilityAnimusComponent::HandleAbilityAuthorize(
	const FInputActionValue& InputActionValue,
	const FGameplayTag TriggerTag
)
{
	// 1) 仅本地控制器处理输入；缓存组件有效时才发送 RPC
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;
	if (!CachedAbilityComponent.IsValid()) return;

	// 2) 通过 Reliable Server RPC 向服务器发送授权请求
	ServerTryAuthorizeAbility(
		CachedAbilityComponent.Get(),
		TriggerTag,
		InputActionValue
	);
}

void USingularisGeneralAbilityAnimusComponent::HandleAbilityRevoke(const FGameplayTag TriggerTag)
{
	// 1) 仅本地控制器处理输入；缓存组件有效时才发送 RPC
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;
	if (!CachedAbilityComponent.IsValid()) return;

	// 2) 通过 Reliable Server RPC 向服务器发送触发结束信号
	ServerTryRevokeTrigger(
		CachedAbilityComponent.Get(),
		TriggerTag
	);
}
```

注：保留文件中既有的 `// ReSharper disable CppMemberFunctionMayBeConst` / `// ReSharper restore CppMemberFunctionMayBeConst` 标注，使其包裹新的两个回调。

- [ ] **Step 11: 更新宿主 C++ 能力（`InstallAbility` / `PlaceAbility`）**

`VehicleTour/Source/VehicleTour/Private/GeneralAbilities/InstallAbility.h`，将：

```cpp
	virtual void Activate_Implementation(const FSingularisGeneralAbilityContext& Context) override;
```

替换为：

```cpp
	virtual void Authorize_Implementation(const FSingularisGeneralAbilityContext& Context) override;
```

`InstallAbility.cpp`，将：

```cpp
void UInstallAbility::Activate_Implementation(const FSingularisGeneralAbilityContext& Context)
{
	Super::Activate_Implementation(Context);
```

替换为：

```cpp
void UInstallAbility::Authorize_Implementation(const FSingularisGeneralAbilityContext& Context)
{
	Super::Authorize_Implementation(Context);
```

`PlaceAbility.h`，将：

```cpp
	virtual void Activate_Implementation(const FSingularisGeneralAbilityContext& Context) override;
```

替换为：

```cpp
	virtual void Authorize_Implementation(const FSingularisGeneralAbilityContext& Context) override;
```

`PlaceAbility.cpp`，将：

```cpp
void UPlaceAbility::Activate_Implementation(const FSingularisGeneralAbilityContext& Context)
{
	Super::Activate_Implementation(Context);
```

替换为：

```cpp
void UPlaceAbility::Authorize_Implementation(const FSingularisGeneralAbilityContext& Context)
{
	Super::Authorize_Implementation(Context);
```

- [ ] **Step 12: 构建，确认编译通过**

构建 `VehicleTourEditor`。

预期：编译通过。宿主项目蓝图资产的编译错误（旧事件重写失效）属预期，Task 3 处理。

- [ ] **Step 13: 运行自动化测试**

在编辑器中打开 Session Frontend（Tools → Session Frontend → Automation），筛选 `Singularis.GeneralAbility.Rules` 并运行。

预期：全部用例通过（本次先行测试至此转绿）。

- [ ] **Step 14: 提交**

```bash
git add VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Tests/SingularisGeneralAbilityRuleTests.cpp \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Objects/SingularisGeneralAbilityBase.h \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Objects/SingularisGeneralAbilityBase.cpp \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Components/SingularisGeneralAbilityComponent.h \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Components/SingularisGeneralAbilityComponent.cpp \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Types/SingularisGeneralAbilityComponentType.h \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Types/SingularisGeneralAbilityAnimusComponentType.h \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Public/Components/SingularisGeneralAbilityAnimusComponent.h \
        VehicleTour/Plugins/SingularisGeneralAbility/Source/SingularisGeneralAbility/Private/Components/SingularisGeneralAbilityAnimusComponent.cpp \
        VehicleTour/Source/VehicleTour/Private/GeneralAbilities/InstallAbility.h \
        VehicleTour/Source/VehicleTour/Private/GeneralAbilities/InstallAbility.cpp \
        VehicleTour/Source/VehicleTour/Private/GeneralAbilities/PlaceAbility.h \
        VehicleTour/Source/VehicleTour/Private/GeneralAbilities/PlaceAbility.cpp
git commit -m "feat(SingularisGeneralAbility): implement authorize/sustain/revoke lifecycle, state tags and closed-loop rules"
```

---

### Task 3: 蓝图资产与项目配置更新

**Files:**

- Modify（编辑器内）：`Content/VehicleTour/GameplayTags/GT_VehicleTour_GeneralAbilities.uasset`
- Modify（编辑器内）：`Content/VehicleTour/GeneralAbilities/GA_Install.uasset`、`GA_Place.uasset`、`GA_Seat.uasset`、`GA_UseItem.uasset`
- Modify（编辑器内）：承载通用能力组件的 Pawn 蓝图、承载意志组件的 PlayerController 蓝图（含 `TriggerPipelineMapping` / `AbilityAnimusInputs` 配置者）

**Interfaces:**

- Consumes: Task 1 的标签根与枚举；Task 2 的全部新接口。
- Produces: 恢复工作的项目内容（无代码接口）。

- [ ] **Step 1: 整理项目标签表**

打开 `GT_VehicleTour_GeneralAbilities`：

1) 确认新增根 `Singularis.General.Ability.Trigger` / `.Identity` / `.State` 可见（刷新标签列表：编辑器重启或执行 GameplayTags 重新导入）。
2) 将本项目用于通用能力的触发标签整理至 `...Ability.Trigger` 子级（例如 `Singularis.General.Ability.Install` → `Singularis.General.Ability.Trigger.Install`）；若某标签被其它系统共用，保留其原值。
3) 为四个既有能力新增身份标签：`Singularis.General.Ability.Identity.Install`、`...Identity.Place`、`...Identity.Seat`、`...Identity.UseItem`。
4) 保存标签表。

- [ ] **Step 2: 迁移四个蓝图能力资产**

对 `GA_Install` / `GA_Place` / `GA_Seat` / `GA_UseItem` 逐个执行：

1) 打开资产，查看 Functions 面板中的 Overrides 列表：旧的 `CanActivate` / `Activate` 重写已失效（失去与父类的绑定）。
2) 删除失效事件节点，重新通过 Override 下拉添加 `CanAuthorize` / `Authorize`（如有 `Sustain` / `OnRevoked` 逻辑同理）。
3) 将原节点图中的逻辑原样迁入新的重写事件（连线与调用不变）。
4) 在 Class Defaults 中设置 `Policy`（保持 `Instant`）与 `IdentityTag`（Step 1 新增的身份标签）。
5) 编译并保存资产。

- [ ] **Step 3: 更新通用能力组件的触发映射**

打开承载通用能力组件的 Pawn 蓝图：

1) 在组件的 `TriggerPipelineMapping` 中，将所有键（触发标签）重新选择为 Step 1 整理后的 `...Ability.Trigger.*` 值（字段改名与标签迁移后旧值失效）。
2) 确认各管线条目的能力引用完好。
3) 编译并保存蓝图。

- [ ] **Step 4: 更新意志组件的输入映射**

打开承载意志组件的 PlayerController 蓝图：

1) 在 `AbilityAnimusInputs` 中，为每个条目重新选择 `TriggerTag`（`...Ability.Trigger.*`）。
2) 按设计意图勾选/取消 `bWhileHeld`（原按键点按型能力保持未勾选）。
3) 编译并保存蓝图。

- [ ] **Step 5: 全项目蓝图编译检查**

在编辑器中执行：Asset Actions → Compile All Blueprints（或在 Content Browser 中全选后右键 Compile）。

预期：无错误；无失效标签告警（WarnOnInvalidTags 打开的配置下）。

- [ ] **Step 6: PIE 冒烟（瞬时能力回归）**

进入 PIE，操作既有交互（安装 / 放置物品）走通一次。

预期：行为与升级前一致；无 Ensure / 报错。

- [ ] **Step 7: 提交**

```bash
git add VehicleTour/Content/VehicleTour/GameplayTags/GT_VehicleTour_GeneralAbilities.uasset \
        VehicleTour/Content/VehicleTour/GeneralAbilities/GA_Install.uasset \
        VehicleTour/Content/VehicleTour/GeneralAbilities/GA_Place.uasset \
        VehicleTour/Content/VehicleTour/GeneralAbilities/GA_Seat.uasset \
        VehicleTour/Content/VehicleTour/GeneralAbilities/GA_UseItem.uasset
git commit -m "chore(VehicleTour): migrate general ability assets to authorize lifecycle"
```

注：Pawn / PlayerController 蓝图（组件默认值变更）需一并提交；在编辑器中右键资产选择 Copy Reference 获取其仓库相对路径后加入 `git add`。

---

### Task 4: 端到端验证与收尾

**Files:**

- Create（编辑器内）：`Content/VehicleTour/Dev/` 下的验证探针资产（`BP_ProbeHoldA` / `BP_ProbeHoldB` / `BP_ProbePickup` / `BP_ProbeStun`，标签表新增探针标签）

**Interfaces:**

- Consumes: Task 2 的全部接口；Task 3 的标签表。
- Produces: 可重复运行的验证 fixture 与验证结论。

- [ ] **Step 1: 新增探针标签**

在 `GT_VehicleTour_GeneralAbilities` 中新增：

- 触发：`...Trigger.Probe.HoldA`、`...Trigger.Probe.HoldB`、`...Trigger.Probe.Pickup`、`...Trigger.Probe.Stun`
- 身份：`...Identity.Probe.HoldA`、`...Identity.Probe.HoldB`、`...Identity.Probe.Pickup`、`...Identity.Probe.Stun`
- 状态：`...State.Probe.Holding`、`...State.Probe.Stunned`

- [ ] **Step 2: 创建四个探针能力（`/Game/VehicleTour/Dev/`）**

1) `BP_ProbeHoldA`（父类 `USingularisGeneralAbility`）：`Policy=Sustained`；`IdentityTag=...Identity.Probe.HoldA`；`OwnedTags=[...State.Probe.Holding]`；`CanceledByTags=[...State.Probe.Stunned]`；`BlockedTags=[...State.Probe.Stunned]`；事件图：`Authorize` → PrintString "ProbeHoldA Authorize"；`OnRevoked` → PrintString "ProbeHoldA Revoke"；`Sustain` → 计数器每 60 次 PrintString "ProbeHoldA Sustain"；`OnClientAuthorized` / `OnClientRevoked` → PrintString（带 Client 前缀）。
2) `BP_ProbeHoldB`：与 A 相同，但 `IdentityTag=...Identity.Probe.HoldB`，`OwnedTags` 同样为 `...State.Probe.Holding`。
3) `BP_ProbePickup`：`Policy=Instant`；`IdentityTag=...Identity.Probe.Pickup`；`BlockedTags=[...State.Probe.Holding]`；`Authorize` → PrintString "ProbePickup Authorize"。
4) `BP_ProbeStun`：`Policy=Instant`；`IdentityTag=...Identity.Probe.Stun`；`Authorize` → 经 `GetOwningAbilityComponent` 分支：`HasStateTag(...State.Probe.Stunned)` 为真则 `RemoveStateTag`，否则 `AddStateTag`。

- [ ] **Step 3: 配置映射与输入**

1) Pawn 的通用能力组件 `TriggerPipelineMapping`：新增四条映射，各含单一探针能力。
2) PlayerController 的意志组件 `AbilityAnimusInputs`：HoldA / HoldB 条目勾选 `bWhileHeld`；Pickup / Stun 条目不勾选；四个条目绑定到四个不同按键。
3) 关卡蓝图：获取 Pawn 的通用能力组件，绑定 `OnStateTagsChangedEvent` → PrintString（Added / Removed）。
4) 关卡蓝图：新增一个按键（如 T）→ 调用组件的 `TryAuthorizeAbilitiesByTag`（`...Identity.Probe.HoldA`，`GetPlayerController`），用于清单项 10。

- [ ] **Step 4: 运行验证清单**

PIE 开始后在控制台执行 `log LogSingularisGeneralAbility Verbose`，逐项验证：

1) 探针生命周期：按住 HoldA 键 → "ProbeHoldA Authorize"；持续每秒出现 "ProbeHoldA Sustain"；松开 → "ProbeHoldA Revoke"，Verbose 日志出现撤销（原因触发结束）。
2) 状态拦截：按住 HoldA 期间按 Pickup 键 → 不出现 "ProbePickup Authorize"；日志出现"被门禁拒绝"。
3) 状态打断：按住 HoldA 期间按 Stun 键 → "ProbeHoldA Revoke"，关卡打印 `...State.Probe.Holding` 的 Removed；再按 Stun 键移除眩晕后可再次按住 HoldA。
4) 多拥有者撤销（Review Focus 2）：同时按住 HoldA 与 HoldB → 松开其一 → 不得出现 `...State.Probe.Holding` 的 Removed；两者都松开后 → 出现 Removed。
5) 幂等与非重入（Review Focus 3）：快速连按 HoldA 键 → 不出现重复 "ProbeHoldA Authorize"；对未授权的能力按 Stun / Pickup 无异常；重复 AddStateTag 同一标签无第二次广播。
6) EndPlay 清理（Review Focus 4）：PIE 停止 → 无 Ensure / 报错；Verbose 日志出现撤销（原因销毁）。
7) 触发关联精确性（Review Focus 5）：按 T（程序化授权 HoldA）后，按一下 HoldA 键并立即松开 → "ProbeHoldA Authorize" 不出现（非重入），松开后 HoldA 仍处于授权（不出现 Revoke）；再按 T 无法重复授权；用 `TryRevokeAbilitiesByTag`（可在关卡蓝图临时调用）撤销后恢复。
8) 客户端钩子与复制：将 PIE 设为 Listen Server + 2 名玩家；客户端操作时观察 "Client Authorize / Client Revoke" 打印与状态事件；服务器端行为一致。

- [ ] **Step 5: 修复发现的问题**

若任一项不符合预期：在对应源码处定位根因并修复，重跑 Step 4 全清单。

预期：全清单通过。

- [ ] **Step 6: 最终提交**

```bash
git add VehicleTour/Content/VehicleTour/Dev \
        VehicleTour/Content/VehicleTour/GameplayTags/GT_VehicleTour_GeneralAbilities.uasset
git commit -m "chore(VehicleTour): add general ability lifecycle validation fixtures"
```

（若 Step 5 产生源码修复，一并纳入本次提交并在提交信息中说明。）

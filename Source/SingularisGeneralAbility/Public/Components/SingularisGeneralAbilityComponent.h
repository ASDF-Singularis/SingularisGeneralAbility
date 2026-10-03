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
	const FGameplayTagContainer&,
	AddedTags,
	const FGameplayTagContainer&,
	RemovedTags
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
		Category = "引力奇点通用能力组件",
		meta = (DisplayName = "触发管线映射", ForceSelection = "true")
	)
	TMap<FGameplayTag, FSingularisGeneralAbilityPipeline> TriggerPipelineMapping{};

#pragma endregion

#pragma region Event Dispatcher

	/** 状态标签变更时广播（服务器本地变更后与客户端 OnRep 到达时均触发）。 */
	UPROPERTY(
		BlueprintAssignable,
		Category = "引力奇点通用能力组件|事件分发器",
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

	/** 是否包含状态标签（层级包含）。 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点通用能力组件|API",
		meta = (DisplayName = "HasStateTag")
	)
	bool HasStateTag(
		UPARAM(meta = (Categories = "Singularis.General.Ability.State")) const FGameplayTag& Tag
	) const { return Tag.IsValid() && StateTags.HasTag(Tag); }

	/** 获取状态容器副本。 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点通用能力组件|API",
		meta = (DisplayName = "GetStateTags")
	)
	FGameplayTagContainer GetStateTags() const { return StateTags; }

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
		Category = "引力奇点通用能力组件|API",
		meta = (DisplayName = "TryAuthorizeAbility")
	)
	void TryAuthorizeAbility(
		UPARAM(meta = (Categories = "Singularis.General.Ability.Trigger")) const FGameplayTag& TriggerTag,
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
		Category = "引力奇点通用能力组件|API",
		meta = (DisplayName = "TryAuthorizeAbilitiesByTag")
	)
	void TryAuthorizeAbilitiesByTag(
		UPARAM(meta = (Categories = "Singularis.General.Ability.Identity")) const FGameplayTag& IdentityTag,
		AController* Controller
	);

	/**
	 * 程序化按类授权（匹配子类）。不记录触发关联，不受触发结束信号影响。
	 *
	 * @param AbilityClass 目标能力类
	 * @param Controller 发起请求的控制器
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "引力奇点通用能力组件|API",
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
		Category = "引力奇点通用能力组件|API",
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
		Category = "引力奇点通用能力组件|API",
		meta = (DisplayName = "TryRevokeAbilitiesByTag")
	)
	void TryRevokeAbilitiesByTag(
		UPARAM(meta = (Categories = "Singularis.General.Ability.Identity")) const FGameplayTag& IdentityTag,
		ESingularisGeneralAbilityEndReason Reason
	);

	/**
	 * 新增状态标签。幂等；新增后执行状态打断扫描。
	 *
	 * @param Tag 状态标签
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "引力奇点通用能力组件|API",
		meta = (DisplayName = "AddStateTag")
	)
	void AddStateTag(
		UPARAM(meta = (Categories = "Singularis.General.Ability.State")) const FGameplayTag& Tag
	);

	/**
	 * 移除状态标签。幂等；不触发打断扫描。
	 *
	 * @param Tag 状态标签
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "引力奇点通用能力组件|API",
		meta = (DisplayName = "RemoveStateTag")
	)
	void RemoveStateTag(
		UPARAM(meta = (Categories = "Singularis.General.Ability.State")) const FGameplayTag& Tag
	);

#pragma endregion

private:
#pragma region Response

	/**
	 * 状态容器复制回调。计算差异并广播状态标签变更事件。
	 *
	 * @param PreviousStateTags 复制前的旧容器
	 */
	UFUNCTION()
	void OnRep_StateTags(const FGameplayTagContainer& PreviousStateTags) const;

#pragma endregion

#pragma region Internal Function

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
	void SustainAuthorizedAbilities(float DeltaTime) const;

	/** 状态标签新增后的打断扫描。 */
	void ScanStateTagAdded(const FGameplayTag& AddedTag);

	/** 内部状态标签写入：权威校验、幂等、事件广播与打断扫描。 */
	void ApplyStateTagAdded(const FGameplayTag& Tag);
	void ApplyStateTagRemoved(const FGameplayTag& Tag);

	/** 授予拥有标签（仅持续能力）。 */
	void GrantOwnedTags(const USingularisGeneralAbility* Ability);

	/** 撤销拥有标签（仍有其它已授权能力拥有时保留）。 */
	void RevokeOwnedTags(const USingularisGeneralAbility* Ability);

	/** 依据是否存在已授权能力刷新组件逐帧开关。 */
	void RefreshComponentTick();

	/** 广播状态标签变更事件。 */
	void BroadcastStateTagsChanged(
		const FGameplayTagContainer& AddedTags,
		const FGameplayTagContainer& RemovedTags
	) const;

	/** 将全部有效能力实例添加至网络复制列表（按实例去重，仅服务器）。 */
	void RegisterAbilitySubObjects();

	/** 将全部有效能力实例从网络复制列表移除（按实例去重，仅服务器）。 */
	void UnregisterAbilitySubObjects();

#pragma endregion
};

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <UObject/Object.h>

#include "Types/SingularisGeneralAbilityType.h"
#include "SingularisGeneralAbilityBase.generated.h"

class USingularisGeneralAbilityComponent;

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
 * Blueprint 子类如需客户端反馈（视觉效果、音效、UI 变化等），可重写 OnRep_IsAuthorized
 * 驱动客户端表现逻辑（服务器直接调用时不会触发，仅复制到达客户端时触发）。
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
		Category = "引力奇点通用能力",
		meta = (
			DisplayName = "身份标签",
			ForceSelection = "true",
			AbilityParameter = "true",
			EditCondition = "IsEditableInDefaults",
			EditConditionHides
		)
	)
	FGameplayTag IdentityTag{};

	/** 能力政策：瞬时在 Authorize 返回后立即结束；持续保持授权状态直至被撤销。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "引力奇点通用能力",
		meta = (
			DisplayName = "能力政策",
			AbilityParameter = "true",
			EditCondition = "IsEditableInDefaults",
			EditConditionHides
		)
	)
	ESingularisGeneralAbilityPolicy Policy = ESingularisGeneralAbilityPolicy::Instant;

	/** 授权必需：状态容器须包含全部标签（层级包含）。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "引力奇点通用能力",
		meta = (
			DisplayName = "必需标签",
			ForceSelection = "true",
			AbilityParameter = "true",
			EditCondition = "IsEditableInDefaults",
			EditConditionHides
		)
	)
	FGameplayTagContainer RequiredTags{};

	/** 授权禁止：状态容器命中任一标签即拒绝授权（层级包含）。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "引力奇点通用能力",
		meta = (
			DisplayName = "禁止标签",
			ForceSelection = "true",
			AbilityParameter = "true",
			EditCondition = "IsEditableInDefaults",
			EditConditionHides
		)
	)
	FGameplayTagContainer BlockedTags{};

	/** 授权授予：授权期间写入状态容器，撤销时清除；仅持续能力有效。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "引力奇点通用能力",
		meta = (
			DisplayName = "拥有标签",
			ForceSelection = "true",
			AbilityParameter = "true",
			EditCondition = "IsEditableInDefaults",
			EditConditionHides
		)
	)
	FGameplayTagContainer OwnedTags{};

	/** 状态打断：状态容器新增标签命中时撤销自身（原因被打断）；仅持续能力有效。 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "引力奇点通用能力",
		meta = (
			DisplayName = "打断标签",
			ForceSelection = "true",
			AbilityParameter = "true",
			EditCondition = "IsEditableInDefaults",
			EditConditionHides
		)
	)
	FGameplayTagContainer CanceledTags{};

#pragma endregion

private:
#pragma region State

	/** 授权状态标记。复制至客户端并触发 OnRep_IsAuthorized。 */
	UPROPERTY(ReplicatedUsing = OnRep_IsAuthorized)
	bool bIsAuthorized = false;

	/** 本次授权的触发标签。瞬态，仅服务器有效；程序化授权时为无效标签。 */
	UPROPERTY(Transient)
	FGameplayTag AuthorizationTriggerTag{};

	/** 本次授权的上下文。瞬态，仅服务器有效。 */
	UPROPERTY(Transient)
	FSingularisGeneralAbilityContext AuthorizationContext{};

#pragma endregion

public:
#pragma region UObject Interface

	virtual UWorld* GetWorld() const override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool IsSupportedForNetworking() const override;
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override;

#if WITH_EDITOR

	virtual bool CanEditChange(const FProperty* InProperty) const override;

#endif

#pragma endregion

#pragma region API

	/** 是否处于授权状态。 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点通用能力|API",
		meta = (DisplayName = "IsAuthorized")
	)
	bool IsAuthorized() const { return bIsAuthorized; }

	/** 获取本次授权的上下文（仅服务器、仅授权期间有效）。 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点通用能力|API",
		meta = (DisplayName = "GetAuthorizationContext")
	)
	FSingularisGeneralAbilityContext GetAuthorizationContext() const { return AuthorizationContext; }

	/** 获取本次授权的触发标签（程序化授权时为无效标签）。 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点通用能力|API",
		meta = (DisplayName = "GetAuthorizationTriggerTag")
	)
	FGameplayTag GetAuthorizationTriggerTag() const { return AuthorizationTriggerTag; }

	/** 获取所属通用能力组件。 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点通用能力|API",
		meta = (DisplayName = "GetOwningAbilityComponent")
	)
	USingularisGeneralAbilityComponent* GetOwningAbilityComponent() const;

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
		Category = "引力奇点通用能力|SPI",
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
		Category = "引力奇点通用能力|SPI",
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
		Category = "引力奇点通用能力|SPI",
		meta = (DisplayName = "Sustain")
	)
	void Sustain(float DeltaTime);

	/**
	 * 撤销：撤销流程中执行清理（释放物理柄、注销委托等）。该方法仅在服务器执行。
	 * 调用时授权标记已清除，拥有标签尚未撤销。
	 *
	 * @param Reason 撤销原因
	 */
	UFUNCTION(
		BlueprintNativeEvent,
		Category = "引力奇点通用能力|SPI",
		meta = (DisplayName = "Revoke")
	)
	void Revoke(ESingularisGeneralAbilityEndReason Reason);

	/**
	 * 进入授权：置授权标记并记录触发标签与上下文。仅由通用能力组件在授权例程中调用。
	 *
	 * @param TriggerTag 触发标签；程序化授权时传入无效标签
	 * @param Context 执行上下文
	 */
	void EnterAuthorization(const FGameplayTag& TriggerTag, const FSingularisGeneralAbilityContext& Context);

	/** 退出授权：仅清除授权标记。仅由通用能力组件在撤销流程起始调用。 */
	void LeaveAuthorization();

	/** 清空授权记录：清除触发标签与上下文。仅由通用能力组件在撤销流程末尾调用。 */
	void ClearAuthorizationRecord();

#pragma endregion

private:
#pragma region Response

	/** 授权标记复制回调。仅客户端、且复制值实际变化时触发，供子类驱动客户端表现。 */
	UFUNCTION()
	void OnRep_IsAuthorized();

#pragma endregion

#pragma region Internal Function

	/**
	 * 细节面板条件：仅当对象是能力自身的类默认对象（CDO）时，参数才显示；
	 * 作为 Instanced 子对象嵌在其它蓝图或关卡实例中时隐藏。
	 */
	UFUNCTION()
	bool IsEditableInDefaults() const { return HasAnyFlags(RF_ClassDefaultObject); }

#pragma endregion
};

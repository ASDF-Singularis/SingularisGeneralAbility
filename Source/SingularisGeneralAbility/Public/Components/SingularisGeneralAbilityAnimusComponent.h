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
		Category = "引力奇点通用能力意志组件",
		meta = (DisplayName = "自动控制")
	)
	bool bAutoControl = true;

	/**
	 * 是否在 BeginPlay 后自动启用输入。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "引力奇点通用能力意志组件",
		meta = (DisplayName = "启动启用")
	)
	bool bStartEnabled = true;

	/**
	 * EnhancedInput MappingContext 的添加优先级。数值越高优先级越高。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "引力奇点通用能力意志组件",
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
		Category = "引力奇点通用能力意志组件",
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
		Category = "引力奇点通用能力意志组件",
		meta = (DisplayName = "意志输入集")
	)
	TArray<FSingularisGeneralAbilityAnimusInput> AbilityAnimusInputs{};

#pragma endregion

private:
#pragma region State

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
	 * 是否正在控制角色通用能力。
	 *
	 * @return 存在有效受控角色通用能力时返回 true。
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点通用能力意志组件|API",
		meta = (DisplayName = "Controlled")
	)
	bool Controlled() const { return CachedAbilityComponent.IsValid(); }

	/**
	 * 获取当前受控角色通用能力。
	 *
	 * @return 受控角色通用能力，未控制时返回 nullptr。
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "引力奇点通用能力意志组件|API",
		meta = (DisplayName = "GetAbilityComponent")
	)
	USingularisGeneralAbilityComponent* GetAbilityComponent() const { return CachedAbilityComponent.Get(); }

	/**
	 * 启用本地输入。仅对本地控制器生效，状态不复制。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "引力奇点通用能力意志组件|API",
		meta = (DisplayName = "Enabled")
	)
	void Enabled();

	/**
	 * 禁用本地输入。仅对本地控制器生效，状态不复制。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "引力奇点通用能力意志组件|API",
		meta = (DisplayName = "Disabled")
	)
	void Disabled();

	/**
	 * 开始控制指定角色通用能力。
	 *
	 * @param AbilityComponent 要控制的角色通用能力组件。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "引力奇点通用能力意志组件|API",
		meta = (DisplayName = "控制")
	)
	void Control(USingularisGeneralAbilityComponent* AbilityComponent);

	/**
	 * 释放当前受控角色通用能力。
	 *
	 * 本地控制器操作。移除输入映射上下文并清空受控状态，未控制时静默忽略。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "引力奇点通用能力意志组件|API",
		meta = (DisplayName = "释放")
	)
	void Release();

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

#pragma region Internal Function

	/** 将 AbilityAnimusInputs 中配置的输入动作按相位绑定至 EnhancedInputComponent */
	void BindInput();

	/** 根据当前启用状态添加或移除 InputMappingContext */
	void RefreshInput() const;

	/**
	 * 设置本地输入启用状态。
	 * 仅对本地控制器生效，状态不复制。
	 *
	 * @param IsEnabled true 添加 InputMappingContext，false 移除
	 */
	void SetEnabled(bool IsEnabled);

	/**
	 * 应用启用状态
	 *
	 * @param OldIsEnabled 启用状态
	 */
	void ApplyInEnabled(bool OldIsEnabled) const;

	/**
	 * 设置受控角色通用能力并应用副作用。
	 *
	 * @param AbilityComponent 新的受控角色通用能力，传入 nullptr 表示释放。
	 */
	void SetAbilityComponent(USingularisGeneralAbilityComponent* AbilityComponent);

	/**
	 * 应用受控角色通用能力变化到输入映射。
	 *
	 * @param OldAbilityComponent 变化前的旧受控角色通用能力，当前实现未使用。
	 */
	void ApplyAbilityComponent(const USingularisGeneralAbilityComponent* OldAbilityComponent) const;

	/** 从当前 Possess Pawn 上查找并缓存 USingularisGeneralAbilityComponent */
	void RefreshAbilityComponent();

#pragma endregion
};

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
 * 挂载于 APlayerController，负责将 EnhancedInput 输入事件转化为能力激活请求，
 * 通过 ServerTryActivateAbility (Reliable RPC) 将请求发送至服务器。
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
	 * 输入动作到能力 GameplayTag 的映射列表。
	 * 若未手动配置，则自动添加默认的 IA_GeneralAbility → Singularis.General.Ability.Default 映射。
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
#pragma region Internal Function

	/**
	 * 设置本地输入启用状态。
	 * 仅对本地控制器生效，状态不复制。
	 *
	 * @param bInEnabled true 添加 InputMappingContext，false 移除
	 */
	void SetEnabled(bool bInEnabled);

	/**
	 * Server RPC：将能力激活请求从客户端发送至服务器。
	 *
	 * @param AbilityComponent 目标能力组件
	 * @param AbilityTag 要激活的能力标签
	 * @param InputActionValue 触发输入值
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTryActivateAbility(
		USingularisGeneralAbilityComponent* AbilityComponent,
		const FGameplayTag& AbilityTag,
		const FInputActionValue& InputActionValue
	);

	/** 将 AbilityAnimusInputs 中配置的输入动作绑定至 EnhancedInputComponent */
	void BindInputAction();

	/** 根据当前启用状态添加或移除 InputMappingContext */
	void RefreshInputMappingContext() const;

	/** 从当前 Possess Pawn 上查找并缓存 USingularisGeneralAbilityComponent */
	void RefreshAbilityComponent();

#pragma endregion

#pragma region Callback

	/**
	 * 输入动作触发回调。通过 Server RPC 向服务器发送能力激活请求。
	 *
	 * @param InputActionValue 输入动作值
	 * @param GameplayTag 绑定时关联的能力标签
	 */
	void HandleAbilityInput(const FInputActionValue& InputActionValue, const FGameplayTag GameplayTag);

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

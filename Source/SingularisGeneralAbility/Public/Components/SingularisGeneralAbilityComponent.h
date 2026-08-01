#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <Components/ActorComponent.h>

#include "Types/SingularisGeneralAbilityComponentType.h"
#include "SingularisGeneralAbilityComponent.generated.h"

class AController;
struct FInputActionValue;

/**
 * 引力奇点通用能力组件。
 *
 * 挂载于 Actor（通常为 Pawn），承载并管理 USingularisGeneralAbility 子对象管线。
 * 在 BeginPlay 时将全部 Instanced 能力子对象注册至复制列表，使能力的 Replicated 属性（如 SyncValue）
 * 随组件一同同步至客户端。EndPlay 时自动注销。
 *
 * 激活入口 TryActivateAbility 由服务器权威执行（BlueprintAuthorityOnly），
 * 逐层匹配 GameplayTag 并依次调用管线内各能力的 CanActivate / Activate。
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
	 * 能力管线映射表。键为 GameplayTag（支持层级匹配），值为有序的能力管线。
	 * 配置于蓝图默认值，运行时不可变更。
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "SingularisGeneralAbility|通用能力组件|参数",
		meta = (
			DisplayName = "通用能力管线映射",
			Categories = "Singularis.General.Ability",
			ForceSelection = "true"
		)
	)
	TMap<FGameplayTag, FSingularisGeneralAbilityPipeline> AbilityPipelineMapping{};

#pragma endregion

#pragma region Constructors

	USingularisGeneralAbilityComponent();

#pragma endregion

#pragma region ActorComponent Interface

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region SPI

	/**
	 * 服务器权威的能力激活入口。
	 * 遍历 AbilityPipelineMapping，用 GameplayTag 层级匹配获取对应管线，依次执行各能力的 CanActivate / Activate。
	 *
	 * @param AbilityTag 用于匹配管线的 GameplayTag
	 * @param Controller 发起请求的控制器
	 * @param InputActionValue 触发该能力的输入值
	 */
	UFUNCTION(
		BlueprintCallable,
		BlueprintAuthorityOnly,
		Category = "SingularisGeneral|通用能力组件|SPI",
		meta = (DisplayName = "TryActivateAbility")
	)
	void TryActivateAbility(
		const FGameplayTag& AbilityTag,
		AController* Controller,
		const FInputActionValue& InputActionValue
	);

#pragma endregion

private:
#pragma region Internal Function

	/**
	 * 将 AbilityPipelineMapping 中全部有效的 Instanced 能力子对象添加至网络复制列表。
	 * 仅在服务器端执行。
	 */
	void RegisterAbilitySubObjects();

	/**
	 * 将 AbilityPipelineMapping 中全部已注册的 Instanced 能力子对象从网络复制列表中移除。
	 * 仅在服务器端执行。
	 */
	void UnregisterAbilitySubObjects();

#pragma endregion
};

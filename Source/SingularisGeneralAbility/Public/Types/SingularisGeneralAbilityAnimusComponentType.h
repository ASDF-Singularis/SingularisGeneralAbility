#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>

#include "SingularisGeneralAbilityAnimusComponentType.generated.h"

class UInputAction;

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
		meta = (ForceSelection = "true")
	)
	FGameplayTag TriggerTag{};
};

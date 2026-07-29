#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>

#include "SingularisGeneralAbilityAnimusComponentType.generated.h"

class UInputAction;

/**
 * 引力奇点通用能力意志输入映射。
 * 将 EnhancedInput 的 InputAction 与能力 GameplayTag 关联，
 * 用于 AnimusComponent 的输入绑定。
 */
USTRUCT(BlueprintType)
struct SINGULARISGENERALABILITY_API FSingularisGeneralAbilityAnimusInput
{
	GENERATED_BODY()

	/** EnhancedInput 输入动作资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UInputAction> InputAction = nullptr;

	/**
	 * 触发时发送至服务器的能力标签。
	 * 限定为 "Singularis.General.Ability" 层级下的 GameplayTag。
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		meta = (
			Categories = "Singularis.General.Ability",
			ForceSelection = "true"
		)
	)
	FGameplayTag AbilityTag{};
};

#pragma once

#include <CoreMinimal.h>
#include <GameplayTagContainer.h>
#include <UObject/ObjectPtr.h>

#include "SingularisGeneralAbilityComponentType.generated.h"

class USingularisGeneralAbility;

/**
 * 引力奇点通用能力条目。
 * 对应管线中的单个能力，包含显示名称、描述与 Instanced 能力子对象引用。
 */
USTRUCT(BlueprintType)
struct SINGULARISGENERALABILITY_API FSingularisGeneralAbilityEntry
{
	GENERATED_BODY()

	/** 能力用于 UI 显示的名称 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText AbilityName{};

	/** 能力用于 UI 显示的描述 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText AbilityDescription{};

	/**
	 * Instanced 能力子对象。运行时随组件一同创建，通过 AddReplicatedSubObject 注册后
	 * 其 Replicated 属性将同步至客户端。
	 */
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USingularisGeneralAbility> Ability = nullptr;
};

/**
 * 引力奇点通用能力管线。
 * 包装一组有序的能力条目。管线内各能力按数组顺序依次执行授权例程（CanAuthorize / Authorize）。
 */
USTRUCT(BlueprintType)
struct SINGULARISGENERALABILITY_API FSingularisGeneralAbilityPipeline
{
	GENERATED_BODY()

	/** 能力条目有序列表。TitleProperty 绑定至 AbilityName 用于编辑器内联显示 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (TitleProperty = "AbilityName"))
	TArray<FSingularisGeneralAbilityEntry> Abilities{};
};

/**
 * 引力奇点通用能力触发规则。
 * 触发标签与能力管线的配对。规则表按数组顺序遍历，多规则命中时规则间执行顺序确定。
 */
USTRUCT(BlueprintType)
struct SINGULARISGENERALABILITY_API FSingularisGeneralAbilityTriggerRule
{
	GENERATED_BODY()

	/** 触发标签（层级匹配） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag TriggerTag{};

	/** 命中触发标签后按序执行的能力管线 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FSingularisGeneralAbilityPipeline Pipeline{};
};

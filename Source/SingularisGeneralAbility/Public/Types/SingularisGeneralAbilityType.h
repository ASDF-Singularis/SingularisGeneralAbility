#pragma once

#include <CoreMinimal.h>
#include <InputActionValue.h>

#include "SingularisGeneralAbilityType.generated.h"

class AActor;
class APawn;
class AController;
class USingularisGeneralAbilityComponent;

/**
 * 引力奇点通用能力上下文。
 *
 * 在 TryActivateAbility 中由服务器组装，传递至各能力的 CanActivate / Activate 接口。
 * 其中的 Actor 指针在客户端上可能无效，该结构仅用于服务器端能力逻辑执行。
 */
USTRUCT(BlueprintType)
struct SINGULARISGENERALABILITY_API FSingularisGeneralAbilityContext
{
	GENERATED_BODY()

	/** 发起请求的控制器 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AController> Controller = nullptr;

	/** 触发该能力的 Pawn（通常为 Controller 当前 Possess 的 Pawn） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APawn> Instigator = nullptr;

	/** 承载能力组件的 Actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> Avatar = nullptr;

	/** 被能力影响的 Actor（默认与 Avatar 相同，可由具体能力逻辑重新指定） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> Target = nullptr;

	/** 执行能力激活的组件引用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USingularisGeneralAbilityComponent> AbilityComponent = nullptr;

	/** 触发该能力的输入动作值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInputActionValue InputValue{};
};

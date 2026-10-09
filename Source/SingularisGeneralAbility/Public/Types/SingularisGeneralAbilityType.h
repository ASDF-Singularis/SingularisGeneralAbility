#pragma once

#include <CoreMinimal.h>

#include "InputActionValue.h"
#include "SingularisGeneralAbilityType.generated.h"

class AActor;
class APawn;
class AController;
class USingularisGeneralAbilityComponent;

/**
 * 撤销原因。
 * 描述能力结束授权状态的来源：Completed 为内部自撤销，其余为外部因素。
 */
UENUM(BlueprintType)
enum class ESingularisGeneralAbilityEndReason : uint8
{
	/** 完成：能力自行结束（内部自撤销出口）。 */
	Completed UMETA(DisplayName = "完成"),

	/** 被打断：状态打断、冲突打断或程序化外部撤销。 */
	Canceled UMETA(DisplayName = "被打断"),

	/** 销毁：宿主组件销毁（外部因素）。 */
	Destroyed UMETA(DisplayName = "销毁")
};

/**
 * 引力奇点通用能力上下文。
 *
 * 在授权例程中由服务器组装，传递至各能力的 CanAuthorize / Authorize 接口，并在授权期间缓存于能力。
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

	/** 执行能力授权的组件引用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USingularisGeneralAbilityComponent> AbilityComponent = nullptr;

	/** 触发该能力的输入动作值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInputActionValue InputValue{};
};

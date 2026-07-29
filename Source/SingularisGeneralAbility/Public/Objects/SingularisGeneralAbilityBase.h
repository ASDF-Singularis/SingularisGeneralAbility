#pragma once

#include <CoreMinimal.h>
#include <UObject/Object.h>

#include "SingularisGeneralAbilityBase.generated.h"

struct FSingularisGeneralAbilityContext;

/**
 * 引力奇点通用能力抽象基类。
 *
 * 作为 UObject 子对象挂载于 USingularisGeneralAbilityComponent，
 * 通过 AddReplicatedSubObject 注册至组件级复制列表，实现属性复制的网络同步。
 *
 * 能力激活流程为服务器权威：客户端通过 AnimusComponent 的 Server RPC 向服务器发起请求，
 * 服务器在 TryActivateAbility 中依次执行 CanActivate（前置条件检查）与 Activate（核心逻辑）。
 * Blueprint 子类在重写 Activate 时，若需产生客户端反馈（视觉效果、音效、UI 变化等），
 * 应在 Activate 实现中自行声明并调用 Multicast RPC，或借助已复制的属性（如 SyncValue）
 * 通过 OnRep 回调驱动客户端表现逻辑。
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, CollapseCategories)
class SINGULARISGENERALABILITY_API USingularisGeneralAbility : public UObject
{
	GENERATED_BODY()

public:
#pragma region Parameter

	/**
	 * 同步值。属性变更时将复制至所有客户端并触发 OnRep_SyncValue 回调。
	 * 通过 BlueprintSetter 钩子确保每次 Blueprint 写入都会标记属性为脏并驱动 Owner Actor 的复制更新。
	 */
	UPROPERTY(
		ReplicatedUsing = OnRep_SyncValue,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "SingularisGeneral|引力奇点通用能力|参数",
		meta = (DisplayName = "SyncValue")
	)
	float SyncValue = 1.0f;

#pragma endregion

#pragma region UObject Interface

	virtual UWorld* GetWorld() const override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool IsSupportedForNetworking() const override;
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override;

#pragma endregion

#pragma region SPI

	/**
	 * 服务器权威的前置条件检查。
	 * 返回 false 将阻止 Activate 被调用。该方法仅在服务器执行。
	 *
	 * @param Context 包含 Controller、Instigator、Avatar 等运行时上下文信息
	 * @return 是否满足激活条件
	 */
	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "SingularisGeneral|引力奇点通用能力|SPI",
		meta = (DisplayName = "CanActivate")
	)
	bool CanActivate(const FSingularisGeneralAbilityContext& Context) const;

	/**
	 * 服务器权威的核心激活逻辑。该方法仅在服务器执行。
	 *
	 * Blueprint 子类重写时如需客户端表现（视觉特效、音效等），
	 * 应自行声明 Multicast RPC 在此处调用，或依赖已复制的属性通过 OnRep 回调驱动客户端逻辑。
	 *
	 * @param Context 包含 Controller、Instigator、Avatar 等运行时上下文信息
	 */
	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "SingularisGeneral|引力奇点通用能力|SPI",
		meta = (DisplayName = "Activate")
	)
	void Activate(const FSingularisGeneralAbilityContext& Context);

#pragma endregion

#pragma region Callback

	UFUNCTION()
	void OnRep_SyncValue() const;

#pragma endregion
};

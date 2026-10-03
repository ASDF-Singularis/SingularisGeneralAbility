#include "Objects/SingularisGeneralAbilityBase.h"

#include <Engine/NetDriver.h>
#include <GameFramework/Actor.h>
#include <Net/UnrealNetwork.h>

#include "SingularisGeneralAbility.h"
#include "Components/SingularisGeneralAbilityComponent.h"

UWorld* USingularisGeneralAbility::GetWorld() const
{
	// 1) 排除 CDO：防止在编辑器启动或序列化时获取错误的上下文
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;

	// 2) 通过 Outer 链（AbilityComponent → OwnerActor）获取 WorldContext
	if (const UObject* Outer = GetOuter()) return Outer->GetWorld();

	return Super::GetWorld();
}

void USingularisGeneralAbility::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USingularisGeneralAbility, bIsAuthorized);
}

bool USingularisGeneralAbility::IsSupportedForNetworking() const
{
	return true;
}

int32 USingularisGeneralAbility::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	// 1) CDO 不支持网络调用，直接返回 Local
	if (HasAnyFlags(RF_ClassDefaultObject) || !IsSupportedForNetworking()) return FunctionCallspace::Local;

	// 2) 通过 Outer（AbilityComponent）链式委托，由 UActorComponent::GetFunctionCallspace 再委托至 Owner Actor
	return GetOuter()->GetFunctionCallspace(Function, Stack);
}

bool USingularisGeneralAbility::CallRemoteFunction(
	UFunction* Function,
	void* Parms,
	FOutParmRec* OutParms,
	FFrame* Stack
)
{
	// 1) CDO 不支持网络调用
	if (HasAnyFlags(RF_ClassDefaultObject)) return false;

	// 2) 沿 Outer 链查找 Owner Actor，通过其 NetDriver 转发 RPC
	AActor* OwnerActor = GetTypedOuter<AActor>();
	if (!IsValid(OwnerActor))
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Warning,
			TEXT("[%s] CallRemoteFunction：Outer 链中未找到 Owner Actor，RPC 丢弃"),
			*GetNameSafe(this)
		);
		return false;
	}

	UNetDriver* NetDriver = OwnerActor->GetNetDriver();
	if (!IsValid(NetDriver))
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Warning,
			TEXT("[%s] CallRemoteFunction：Owner Actor %s 无 NetDriver，RPC 丢弃"),
			*GetNameSafe(this),
			*GetNameSafe(OwnerActor)
		);
		return false;
	}

	// 3) 将 this（子对象）作为最后一个参数传入，使 NetDriver 正确路由子对象上的 RPC
	NetDriver->ProcessRemoteFunction(OwnerActor, Function, Parms, OutParms, Stack, this);

	return true;
}

#if WITH_EDITOR

bool USingularisGeneralAbility::CanEditChange(const FProperty* InProperty) const
{
	// 1) 卫语句：基类判定不可编辑或属性无效时直接拒绝
	if (!Super::CanEditChange(InProperty) || InProperty == nullptr)
		return false;

	// 2) 仅允许在类默认对象上编辑，实例化副本中置灰不可改
	if (InProperty && InProperty->HasMetaData(TEXT("AbilityParameter")) && !HasAnyFlags(RF_ClassDefaultObject))
		return false;

	return true;
}

#endif

USingularisGeneralAbilityComponent* USingularisGeneralAbility::GetOwningAbilityComponent() const
{
	return Cast<USingularisGeneralAbilityComponent>(GetOuter());
}

bool USingularisGeneralAbility::CanAuthorize_Implementation(const FSingularisGeneralAbilityContext& Context) const
{
	// 基类默认允许授权；子类可覆写以否决特定上下文
	return true;
}

void USingularisGeneralAbility::Authorize_Implementation(const FSingularisGeneralAbilityContext& Context) {}

void USingularisGeneralAbility::Sustain_Implementation(float DeltaTime) {}

void USingularisGeneralAbility::Revoke_Implementation(ESingularisGeneralAbilityEndReason Reason) {}

void USingularisGeneralAbility::EnterAuthorization(
	const FGameplayTag& TriggerTag,
	const FSingularisGeneralAbilityContext& Context
)
{
	// 置授权标记并记录触发标签与上下文（后续按需复制授权标记 / 缓存瞬态记录）
	bIsAuthorized = true;
	AuthorizationTriggerTag = TriggerTag;
	AuthorizationContext = Context;
}

void USingularisGeneralAbility::LeaveAuthorization()
{
	// 仅清除授权标记；拥有标签与记录的清理由撤销流程后续步骤负责
	bIsAuthorized = false;
}

void USingularisGeneralAbility::ClearAuthorizationRecord()
{
	AuthorizationTriggerTag = FGameplayTag();
	AuthorizationContext = FSingularisGeneralAbilityContext();
}

void USingularisGeneralAbility::OnRep_IsAuthorized() {}

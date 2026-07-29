#include "Objects/SingularisGeneralAbilityBase.h"

#include <Engine/NetDriver.h>
#include <GameFramework/Actor.h>
#include <Net/UnrealNetwork.h>

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
	DOREPLIFETIME(USingularisGeneralAbility, SyncValue);
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
	if (!IsValid(OwnerActor)) return false;

	UNetDriver* NetDriver = OwnerActor->GetNetDriver();
	if (!IsValid(NetDriver)) return false;

	// 3) 将 this（子对象）作为最后一个参数传入，使 NetDriver 正确路由子对象上的 RPC
	NetDriver->ProcessRemoteFunction(OwnerActor, Function, Parms, OutParms, Stack, this);

	return true;
}

bool USingularisGeneralAbility::CanActivate_Implementation(const FSingularisGeneralAbilityContext& Context) const
{
	return true;
}

void USingularisGeneralAbility::Activate_Implementation(const FSingularisGeneralAbilityContext& Context) {}

void USingularisGeneralAbility::OnRep_SyncValue() const
{
	UE_LOG(LogTemp, Log, TEXT("SyncValue replicated: %f"), SyncValue);
}

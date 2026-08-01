#include "Components/SingularisGeneralAbilityComponent.h"

#include <GameFramework/Controller.h>

#include "Objects/SingularisGeneralAbilityBase.h"
#include "Types/SingularisGeneralAbilityType.h"

USingularisGeneralAbilityComponent::USingularisGeneralAbilityComponent()
{
	// 1) 启用组件级复制，使用显式注册的子对象列表进行属性同步
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;

	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	bAutoActivate = true;
}

void USingularisGeneralAbilityComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1) 在服务器端将全部 Instanced 能力子对象注册至复制列表
	RegisterAbilitySubObjects();
}

void USingularisGeneralAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterAbilitySubObjects();

	Super::EndPlay(EndPlayReason);
}

void USingularisGeneralAbilityComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void USingularisGeneralAbilityComponent::TryActivateAbility(
	const FGameplayTag& AbilityTag,
	AController* Controller,
	const FInputActionValue& InputActionValue
)
{
	// 1) 卫语句：服务器权威校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 卫语句：Controller 有效性校验
	if (!IsValid(Controller)) return;

	// 3) 组装执行上下文，将执行主体泛化为 Avatar
	FSingularisGeneralAbilityContext Context;
	Context.Controller = Controller;
	Context.Instigator = Controller->GetPawn();
	Context.Avatar = GetOwner();
	Context.Target = GetOwner();
	Context.AbilityComponent = this;
	Context.InputValue = InputActionValue;

	// 4) 遍历能力管线映射表，使用 GameplayTag 层级进行匹配
	for (const auto& [Tag, Pipeline] : AbilityPipelineMapping)
	{
		if (Tag.MatchesTag(AbilityTag))
		{
			// 5) 匹配成功后按顺序依次执行管线内各能力的 CanActivate / Activate
			for (const auto& Entry : Pipeline.Abilities)
			{
				if (IsValid(Entry.Ability) && Entry.Ability->CanActivate(Context))
				{
					Entry.Ability->Activate(Context);
				}
			}
		}
	}
}

void USingularisGeneralAbilityComponent::RegisterAbilitySubObjects()
{
	// 1) 仅服务器端执行子对象注册
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 遍历全部能力管线，将有效的 Instanced 能力子对象添加至网络复制列表
	for (auto& [Tag, Pipeline] : AbilityPipelineMapping)
	{
		for (auto& Entry : Pipeline.Abilities)
		{
			if (!IsValid(Entry.Ability)) continue;

			AddReplicatedSubObject(Entry.Ability);
		}
	}
}

void USingularisGeneralAbilityComponent::UnregisterAbilitySubObjects()
{
	// 1) 仅服务器端执行子对象注销
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 遍历全部能力管线，将已注册的能力子对象从网络复制列表中移除
	for (auto& [Tag, Pipeline] : AbilityPipelineMapping)
	{
		for (auto& Entry : Pipeline.Abilities)
		{
			if (!IsValid(Entry.Ability)) continue;

			RemoveReplicatedSubObject(Entry.Ability);
		}
	}
}

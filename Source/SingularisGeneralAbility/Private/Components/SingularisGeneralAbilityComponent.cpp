#include "Components/SingularisGeneralAbilityComponent.h"

#include <Containers/Set.h>
#include <GameFramework/Controller.h>
#include <Net/UnrealNetwork.h>

#include "SingularisGeneralAbility.h"
#include "Objects/SingularisGeneralAbilityBase.h"
#include "Types/SingularisGeneralAbilityType.h"

USingularisGeneralAbilityComponent::USingularisGeneralAbilityComponent()
{
	// 1) 启用组件级复制，使用显式注册的子对象列表进行属性同步
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;

	// 2) 逐帧能力默认关闭，由生命周期编排在存在已授权能力时开启
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

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
	// 1) 权威端：先撤销全部已授权能力（销毁原因），保证清理钩子可见完整上下文
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		const FDispatchScope DispatchScope(*this);

		TArray<USingularisGeneralAbility*> Abilities;
		CollectUniqueAbilities(Abilities);

		for (USingularisGeneralAbility* Ability : Abilities)
			RequestRevoke(Ability, ESingularisGeneralAbilityEndReason::Destroyed);
	}

	// 2) 将全部能力子对象从网络复制列表中移除
	UnregisterAbilitySubObjects();

	Super::EndPlay(EndPlayReason);
}

void USingularisGeneralAbilityComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 1) 卫语句：逐帧仅在权威端执行
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 撤销调度作用域：逐帧回调中的自撤销请求在归零时统一结算
	const FDispatchScope DispatchScope(*this);

	// 3) 逐帧回调已授权能力
	SustainAuthorizedAbilities(DeltaTime);
}

void USingularisGeneralAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USingularisGeneralAbilityComponent, StateTags);
}

void USingularisGeneralAbilityComponent::TryAuthorizeAbility(
	const FGameplayTag& TriggerTag,
	AController* Controller,
	const FInputActionValue& InputActionValue
)
{
	// 1) 卫语句：服务器权威、控制器与标签校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Controller) || !TriggerTag.IsValid()) return;

	// 2) 撤销调度作用域：本次触发的自撤销与连锁撤销在归零时统一结算
	const FDispatchScope DispatchScope(*this);

	// 3) 组装执行上下文，将执行主体泛化为 Avatar
	FSingularisGeneralAbilityContext Context;
	Context.Controller = Controller;
	Context.Instigator = Controller->GetPawn();
	Context.Avatar = GetOwner();
	Context.Target = GetOwner();
	Context.AbilityComponent = this;
	Context.InputValue = InputActionValue;

	// 4) 遍历触发管线映射，使用触发标签层级匹配后按序执行授权例程（按实例去重）
	TSet<USingularisGeneralAbility*> Attempted;
	for (const auto& [Tag, Pipeline] : TriggerPipelineMapping)
	{
		if (!Tag.MatchesTag(TriggerTag)) continue;

		for (const FSingularisGeneralAbilityEntry& Entry : Pipeline.Abilities)
		{
			USingularisGeneralAbility* Ability = Entry.Ability;
			if (!IsValid(Ability) || Attempted.Contains(Ability)) continue;

			Attempted.Add(Ability);
			TryAuthorizeAbilityInstance(Ability, TriggerTag, Context);
		}
	}
}

void USingularisGeneralAbilityComponent::TryAuthorizeAbilitiesByTag(
	const FGameplayTag& IdentityTag,
	AController* Controller
)
{
	// 1) 卫语句：服务器权威、控制器与标签校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Controller) || !IdentityTag.IsValid()) return;

	// 2) 撤销调度作用域
	const FDispatchScope DispatchScope(*this);

	// 3) 组装执行上下文（无触发标签与输入值）
	FSingularisGeneralAbilityContext Context;
	Context.Controller = Controller;
	Context.Instigator = Controller->GetPawn();
	Context.Avatar = GetOwner();
	Context.Target = GetOwner();
	Context.AbilityComponent = this;

	// 4) 收集并授权身份标签命中的能力（不记录触发关联）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IdentityTag.IsValid()) continue;
		if (!Ability->IdentityTag.MatchesTag(IdentityTag)) continue;

		TryAuthorizeAbilityInstance(Ability, FGameplayTag(), Context);
	}
}

void USingularisGeneralAbilityComponent::TryAuthorizeAbilityByClass(
	const TSubclassOf<USingularisGeneralAbility> AbilityClass,
	AController* Controller
)
{
	// 1) 卫语句：服务器权威、控制器与类校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Controller) || !AbilityClass) return;

	// 2) 撤销调度作用域
	const FDispatchScope DispatchScope(*this);

	// 3) 组装执行上下文（无触发标签与输入值）
	FSingularisGeneralAbilityContext Context;
	Context.Controller = Controller;
	Context.Instigator = Controller->GetPawn();
	Context.Avatar = GetOwner();
	Context.Target = GetOwner();
	Context.AbilityComponent = this;

	// 4) 收集并授权类匹配的能力（不记录触发关联）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsA(AbilityClass)) continue;

		TryAuthorizeAbilityInstance(Ability, FGameplayTag(), Context);
	}
}

void USingularisGeneralAbilityComponent::TryRevokeTrigger(const FGameplayTag& TriggerTag)
{
	// 1) 卫语句：服务器权威与标签校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!TriggerTag.IsValid()) return;

	// 2) 撤销调度作用域
	const FDispatchScope DispatchScope(*this);

	// 3) 收集触发标签精确关联的已授权能力（先收集后撤销，避免遍历中变更）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	TArray<USingularisGeneralAbility*> Targets;
	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsAuthorized()) continue;
		if (Ability->GetAuthorizationTriggerTag() != TriggerTag) continue;

		Targets.Add(Ability);
	}

	// 4) 统一撤销（外部原因：被打断）
	for (USingularisGeneralAbility* Ability : Targets)
		RequestRevoke(Ability, ESingularisGeneralAbilityEndReason::Canceled);

	UE_LOG(
		LogSingularisGeneralAbility,
		Verbose,
		TEXT("收到触发结束信号（触发标签：%s），撤销 %d 个能力"),
		*TriggerTag.ToString(),
		Targets.Num()
	);
}

void USingularisGeneralAbilityComponent::TryRevokeAbilitiesByTag(
	const FGameplayTag& IdentityTag,
	const ESingularisGeneralAbilityEndReason Reason
)
{
	// 1) 卫语句：服务器权威与标签校验
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IdentityTag.IsValid()) return;

	// 2) 撤销调度作用域
	const FDispatchScope DispatchScope(*this);

	// 3) 收集身份标签命中的已授权能力（先收集后撤销，避免遍历中变更）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	TArray<USingularisGeneralAbility*> Targets;
	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsAuthorized()) continue;
		if (!MatchesAnyFilterTag(Ability->IdentityTag, IdentityTag.GetSingleTagContainer())) continue;

		Targets.Add(Ability);
	}

	// 4) 统一撤销
	for (USingularisGeneralAbility* Ability : Targets)
		RequestRevoke(Ability, Reason);
}

void USingularisGeneralAbilityComponent::AddStateTag(const FGameplayTag& Tag)
{
	// 1) 撤销调度作用域：状态写入可能触发打断扫描
	const FDispatchScope DispatchScope(*this);

	// 2) 委托内部写入：权威校验、幂等、事件广播与打断扫描
	ApplyStateTagAdded(Tag);
}

void USingularisGeneralAbilityComponent::RemoveStateTag(const FGameplayTag& Tag)
{
	// 1) 撤销调度作用域
	const FDispatchScope DispatchScope(*this);

	// 2) 委托内部写入：权威校验、幂等、事件广播（不触发打断扫描）
	ApplyStateTagRemoved(Tag);
}

void USingularisGeneralAbilityComponent::OnRep_StateTags(const FGameplayTagContainer& PreviousStateTags) const
{
	// 1) 计算差异（客户端无权威变更路径，OnRep 为唯一广播入口）
	FGameplayTagContainer AddedTags;
	FGameplayTagContainer RemovedTags;

	for (const FGameplayTag& Tag : StateTags.GetGameplayTagArray())
		if (!PreviousStateTags.HasTagExact(Tag)) AddedTags.AddTag(Tag);

	for (const FGameplayTag& Tag : PreviousStateTags.GetGameplayTagArray())
		if (!StateTags.HasTagExact(Tag)) RemovedTags.AddTag(Tag);

	// 2) 广播
	BroadcastStateTagsChanged(AddedTags, RemovedTags);
}

bool USingularisGeneralAbilityComponent::EvaluateAuthorizationGates(
	const FGameplayTagContainer& InStateTags,
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& BlockedTags
)
{
	// 1) 必需标签：须全部满足（层级包含）
	if (!RequiredTags.IsEmpty() && !InStateTags.HasAll(RequiredTags)) return false;

	// 2) 禁止标签：不得命中任一（层级包含）
	if (!BlockedTags.IsEmpty() && InStateTags.HasAny(BlockedTags)) return false;

	return true;
}

bool USingularisGeneralAbilityComponent::MatchesAnyFilterTag(
	const FGameplayTag& Tag,
	const FGameplayTagContainer& FilterTags
)
{
	// 1) 卫语句：无效标签或空过滤集不匹配
	if (!Tag.IsValid() || FilterTags.IsEmpty()) return false;

	// 2) 层级包含：标签等于某过滤器或位于其子树内
	for (const FGameplayTag& FilterTag : FilterTags.GetGameplayTagArray())
		if (FilterTag.IsValid() && Tag.MatchesTag(FilterTag)) return true;

	return false;
}

void USingularisGeneralAbilityComponent::RevokeAbility(
	USingularisGeneralAbility* Ability,
	const ESingularisGeneralAbilityEndReason Reason
)
{
	// 1) 卫语句：仅权威端处理；未授权则无操作（幂等）
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Ability) || !Ability->IsAuthorized()) return;

	// 2) 先行清除授权标记，保证清理期间的重入安全
	Ability->LeaveAuthorization();

	// 3) 清理钩子（此时拥有标签尚未撤销）
	Ability->Revoke(Reason);

	// 4) 撤销拥有标签（多拥有者检查）
	RevokeOwnedTags(Ability);

	// 5) 清空授权记录
	Ability->ClearAuthorizationRecord();

	// 6) 依据剩余已授权能力刷新逐帧开关
	RefreshComponentTick();

	UE_LOG(
		LogSingularisGeneralAbility,
		Verbose,
		TEXT("能力 %s 已撤销（原因：%d）"),
		*Ability->GetName(),
		static_cast<int32>(Reason)
	);
}

void USingularisGeneralAbilityComponent::RequestRevoke(
	USingularisGeneralAbility* Ability,
	const ESingularisGeneralAbilityEndReason Reason
)
{
	// 1) 卫语句：仅权威端、有效能力
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!IsValid(Ability)) return;

	// 2) 调度期入队，非调度期立即执行
	if (DispatchDepth > 0)
	{
		PendingRevocations.Add({Ability, Reason});
		return;
	}

	RevokeAbility(Ability, Reason);
}

void USingularisGeneralAbilityComponent::FlushPendingRevocations()
{
	// 1) 防重入：结算期间钩子触发的结算请求留待本轮循环处理
	if (bIsFlushing) return;

	bIsFlushing = true;

	// 2) 逐批结算：执行期间新入队的请求留待下一轮，避免遍历中变更
	while (!PendingRevocations.IsEmpty())
	{
		TArray<FSingularisGeneralAbilityRevokeRequest> Batch = MoveTemp(PendingRevocations);
		PendingRevocations.Reset();

		for (const FSingularisGeneralAbilityRevokeRequest& Request : Batch)
			RevokeAbility(Request.Ability.Get(), Request.Reason);
	}

	bIsFlushing = false;
}

bool USingularisGeneralAbilityComponent::TryAuthorizeAbilityInstance(
	USingularisGeneralAbility* Ability,
	const FGameplayTag& TriggerTag,
	const FSingularisGeneralAbilityContext& Context
)
{
	// 1) 卫语句：能力有效且未授权（非重入）
	if (!IsValid(Ability) || Ability->IsAuthorized()) return false;

	// 2) 门禁：必需 / 禁止标签判定
	if (!EvaluateAuthorizationGates(StateTags, Ability->RequiredTags, Ability->BlockedTags))
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Log,
			TEXT("能力 %s 被门禁拒绝（必需：%s；禁止：%s；状态：%s）"),
			*Ability->GetName(),
			*Ability->RequiredTags.ToString(),
			*Ability->BlockedTags.ToString(),
			*StateTags.ToString()
		);
		return false;
	}

	// 3) 命令式前置检查
	if (!Ability->CanAuthorize(Context)) return false;

	// 4) 进入授权状态并授予拥有标签
	Ability->EnterAuthorization(TriggerTag, Context);
	GrantOwnedTags(Ability);
	RefreshComponentTick();

	// 5) 执行授权逻辑；其中若请求自撤销，则于调度作用域归零时结算
	UE_LOG(LogSingularisGeneralAbility, Verbose, TEXT("能力 %s 已授权"), *Ability->GetName());

	Ability->Authorize(Context);

	return true;
}

void USingularisGeneralAbilityComponent::CollectUniqueAbilities(
	TArray<USingularisGeneralAbility*>& OutAbilities
) const
{
	OutAbilities.Reset();

	// 1) 遍历触发管线映射，按实例去重后收集
	TSet<USingularisGeneralAbility*> Visited;
	for (const auto& [Tag, Pipeline] : TriggerPipelineMapping)
	{
		for (const FSingularisGeneralAbilityEntry& Entry : Pipeline.Abilities)
		{
			USingularisGeneralAbility* Ability = Entry.Ability;
			if (!IsValid(Ability) || Visited.Contains(Ability)) continue;

			Visited.Add(Ability);
			OutAbilities.Add(Ability);
		}
	}
}

void USingularisGeneralAbilityComponent::SustainAuthorizedAbilities(const float DeltaTime) const
{
	// 1) 遍历去重后的能力集合，逐帧回调已授权者
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
		if (IsValid(Ability) && Ability->IsAuthorized()) Ability->Sustain(DeltaTime);
}

void USingularisGeneralAbilityComponent::ScanStateTagAdded(const FGameplayTag& AddedTag)
{
	// 1) 收集被状态打断标签命中新增标签的已授权能力（先收集后撤销，避免遍历中变更）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	TArray<USingularisGeneralAbility*> Targets;
	for (USingularisGeneralAbility* Ability : Abilities)
	{
		if (!IsValid(Ability) || !Ability->IsAuthorized()) continue;
		if (!MatchesAnyFilterTag(AddedTag, Ability->CanceledTags)) continue;

		Targets.Add(Ability);
	}

	// 2) 统一撤销（原因：被打断）
	for (USingularisGeneralAbility* Ability : Targets)
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Log,
			TEXT("能力 %s 被状态打断（新增标签：%s）"),
			*Ability->GetName(),
			*AddedTag.ToString()
		);
		RequestRevoke(Ability, ESingularisGeneralAbilityEndReason::Canceled);
	}
}

void USingularisGeneralAbilityComponent::ApplyStateTagAdded(const FGameplayTag& Tag)
{
	// 1) 卫语句：权威校验与幂等（集合语义）
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!Tag.IsValid() || StateTags.HasTagExact(Tag)) return;

	// 2) 写入并广播
	StateTags.AddTag(Tag);
	BroadcastStateTagsChanged(Tag.GetSingleTagContainer(), FGameplayTagContainer());

	// 3) 状态打断扫描
	ScanStateTagAdded(Tag);
}

void USingularisGeneralAbilityComponent::ApplyStateTagRemoved(const FGameplayTag& Tag)
{
	// 1) 卫语句：权威校验与幂等
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!Tag.IsValid() || !StateTags.HasTagExact(Tag)) return;

	// 2) 移除并广播（移除不触发打断扫描）
	StateTags.RemoveTag(Tag);
	BroadcastStateTagsChanged(FGameplayTagContainer(), Tag.GetSingleTagContainer());
}

void USingularisGeneralAbilityComponent::GrantOwnedTags(const USingularisGeneralAbility* Ability)
{
	// 1) 卫语句：能力有效
	if (!IsValid(Ability)) return;

	// 2) 逐个授予（内部写入含广播与打断扫描）
	for (const FGameplayTag& Tag : Ability->OwnedTags.GetGameplayTagArray())
		ApplyStateTagAdded(Tag);
}

void USingularisGeneralAbilityComponent::RevokeOwnedTags(const USingularisGeneralAbility* Ability)
{
	// 1) 卫语句
	if (!IsValid(Ability)) return;

	// 2) 收集全部已授权能力，供多拥有者检查
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	// 3) 逐个撤销：仍有其它已授权能力拥有该标签时保留
	for (const FGameplayTag& Tag : Ability->OwnedTags.GetGameplayTagArray())
	{
		auto bStillOwned = false;
		for (const USingularisGeneralAbility* Other : Abilities)
		{
			if (Other == Ability || !IsValid(Other) || !Other->IsAuthorized()) continue;
			if (!Other->OwnedTags.HasTagExact(Tag)) continue;

			bStillOwned = true;
			break;
		}

		if (bStillOwned) continue;

		ApplyStateTagRemoved(Tag);
	}
}

void USingularisGeneralAbilityComponent::RefreshComponentTick()
{
	// 1) 卫语句：仅权威端管理逐帧开关
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 存在已授权能力时开启，否则关闭
	auto bHasAuthorizedAbility = false;

	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (const USingularisGeneralAbility* Ability : Abilities)
	{
		if (IsValid(Ability) && Ability->IsAuthorized())
		{
			bHasAuthorizedAbility = true;
			break;
		}
	}

	SetComponentTickEnabled(bHasAuthorizedAbility);
}

void USingularisGeneralAbilityComponent::BroadcastStateTagsChanged(
	const FGameplayTagContainer& AddedTags,
	const FGameplayTagContainer& RemovedTags
) const
{
	// 1) 卫语句：无差异不广播
	if (AddedTags.IsEmpty() && RemovedTags.IsEmpty()) return;

	OnStateTagsChangedEvent.Broadcast(AddedTags, RemovedTags);
}

void USingularisGeneralAbilityComponent::RegisterAbilitySubObjects()
{
	// 1) 仅服务器端执行子对象注册
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 将全部有效能力实例添加至网络复制列表（按实例去重）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
		AddReplicatedSubObject(Ability);
}

void USingularisGeneralAbilityComponent::UnregisterAbilitySubObjects()
{
	// 1) 仅服务器端执行子对象注销
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// 2) 将全部有效能力实例从网络复制列表中移除（按实例去重）
	TArray<USingularisGeneralAbility*> Abilities;
	CollectUniqueAbilities(Abilities);

	for (USingularisGeneralAbility* Ability : Abilities)
		RemoveReplicatedSubObject(Ability);
}

#include "Components/SingularisGeneralAbilityAnimusComponent.h"

#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <InputMappingContext.h>
#include <Engine/LocalPlayer.h>
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>
#include <UObject/ConstructorHelpers.h>

#include "SingularisGeneralAbility.h"
#include "Components/SingularisGeneralAbilityComponent.h"
#include "Types/SingularisGeneralAbilityGameplayTags.h"

USingularisGeneralAbilityAnimusComponent::USingularisGeneralAbilityAnimusComponent()
{
	SetIsReplicatedByDefault(true);

	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	bAutoActivate = true;

	// 1) 加载插件内置的默认 InputMappingContext 与 InputAction，提供开箱即用的默认配置
	static const ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultInputMappingContextFinder(
		TEXT(
			"/SingularisGeneralAbility/Inputs/IMC_Default_SingularisGeneralAbility.IMC_Default_SingularisGeneralAbility"
		)
	);
	static const ConstructorHelpers::FObjectFinder<UInputAction> DefaultAbilityActionFinder(
		TEXT("/SingularisGeneralAbility/Inputs/Actions/IA_GeneralAbility.IA_GeneralAbility")
	);

	if (DefaultInputMappingContextFinder.Succeeded())
		InputMappingContext = DefaultInputMappingContextFinder.Object;
	else
		UE_LOG(
		LogSingularisGeneralAbility,
		Error,
		TEXT("默认输入映射上下文加载失败：%s"),
		TEXT("/SingularisGeneralAbility/Inputs/IMC_Default_SingularisGeneralAbility")
	);

	if (DefaultAbilityActionFinder.Succeeded())
	{
		AbilityAnimusInputs.Add(
			{
				DefaultAbilityActionFinder.Object,
				SingularisGeneral_Ability_Trigger_Default
			}
		);
	}
	else
		UE_LOG(
		LogSingularisGeneralAbility,
		Error,
		TEXT("默认能力输入动作加载失败：%s"),
		TEXT("/SingularisGeneralAbility/Inputs/Actions/IA_GeneralAbility")
	);
}

void USingularisGeneralAbilityAnimusComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1) 确保组件的 Owner 是 PlayerController
	checkf(
		GetOwner()->IsA<APlayerController>(),
		TEXT("[%s] Owner 非 PlayerController"),
		*GetNameSafe(GetOwner())
	);

	OwnerPlayerController = Cast<APlayerController>(GetOwner());

	// 2) 输入绑定与启用仅对本地控制器执行
	if (OwnerPlayerController.IsValid() && OwnerPlayerController->IsLocalController())
	{
		BindInput();

		if (bStartEnabled)
			SetEnabled(true);

		if (bAutoControl)
		{
			// 3) 监听 PossessPawnChanged 以自动刷新缓存的 AbilityComponent
			OwnerPlayerController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessPawnChanged);

			RefreshAbilityComponent();
		}
	}

	UE_LOG(
		LogSingularisGeneralAbility,
		Display,
		TEXT("[%s] BeginPlay：意志组件初始化完成"),
		*GetNameSafe(GetOwner())
	);
}

void USingularisGeneralAbilityAnimusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 1) 注销 PossessPawnChanged 委托绑定
	if (OwnerPlayerController.IsValid())
		OwnerPlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessPawnChanged);

	// 2) 清理缓存引用
	CachedAbilityComponent = nullptr;
	OwnerPlayerController.Reset();

	Super::EndPlay(EndPlayReason);

	UE_LOG(
		LogSingularisGeneralAbility,
		Display,
		TEXT("[%s] EndPlay：意志组件已清理"),
		*GetNameSafe(GetOwner())
	);
}

void USingularisGeneralAbilityAnimusComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 基类无复制属性；本组件仅需承载 Server RPC，复制策略由 SetIsReplicatedByDefault 决定
}

void USingularisGeneralAbilityAnimusComponent::Enabled()
{
	SetEnabled(true);
}

void USingularisGeneralAbilityAnimusComponent::Disabled()
{
	SetEnabled(false);
}

void USingularisGeneralAbilityAnimusComponent::Control(USingularisGeneralAbilityComponent* AbilityComponent)
{
	// 1) 卫语句：已受控或无缓存能力组件时忽略
	if (Controlled() || !IsValid(AbilityComponent)) return;

	// 2) 写入受控能力组件并应用输入映射副作用
	SetAbilityComponent(AbilityComponent);
}

void USingularisGeneralAbilityAnimusComponent::Release()
{
	// 1) 卫语句：未受控时无操作（幂等）
	if (!Controlled()) return;

	// 2) 清空受控能力组件并应用输入映射副作用
	SetAbilityComponent(nullptr);
}

void USingularisGeneralAbilityAnimusComponent::ServerTryAuthorizeAbility_Implementation(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& TriggerTag,
	const FInputActionValue& InputActionValue
)
{
	// 1) 服务器收到 RPC 后，将请求转发至能力组件的授权入口（HasAuthority 内部已保证）
	if (!IsValid(AbilityComponent))
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Warning,
			TEXT("[%s] ServerTryAuthorizeAbility：能力组件无效，请求丢弃"),
			*GetNameSafe(GetOwner())
		);
		return;
	}
	if (!OwnerPlayerController.IsValid()) return;

	AbilityComponent->TryAuthorizeAbility(TriggerTag, OwnerPlayerController.Get(), InputActionValue);

	UE_LOG(
		LogSingularisGeneralAbility,
		Verbose,
		TEXT("[%s] ServerTryAuthorizeAbility：收到授权信号（触发标签：%s）"),
		*GetNameSafe(GetOwner()),
		*TriggerTag.ToString()
	);
}

bool USingularisGeneralAbilityAnimusComponent::ServerTryAuthorizeAbility_Validate(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& TriggerTag,
	const FInputActionValue& InputActionValue
)
{
	// 1) 仅校验能力组件有效性，防止客户端传入无效指针
	return IsValid(AbilityComponent);
}

void USingularisGeneralAbilityAnimusComponent::ServerTryRevokeTrigger_Implementation(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& TriggerTag
)
{
	// 1) 服务器收到 RPC 后，将触发结束信号转发至能力组件
	if (!IsValid(AbilityComponent)) return;

	AbilityComponent->TryRevokeTrigger(TriggerTag);

	UE_LOG(
		LogSingularisGeneralAbility,
		Verbose,
		TEXT("[%s] ServerTryRevokeTrigger：收到触发结束信号（触发标签：%s）"),
		*GetNameSafe(GetOwner()),
		*TriggerTag.ToString()
	);
}

bool USingularisGeneralAbilityAnimusComponent::ServerTryRevokeTrigger_Validate(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& TriggerTag
)
{
	// 1) 校验能力组件与触发标签有效性
	return IsValid(AbilityComponent) && TriggerTag.IsValid();
}

// ReSharper disable CppMemberFunctionMayBeConst
void USingularisGeneralAbilityAnimusComponent::HandleAbilityAuthorize(
	const FInputActionValue& InputActionValue,
	const FGameplayTag TriggerTag
)
{
	// 1) 仅本地控制器处理输入；缓存组件有效时才发送 RPC
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;
	if (!CachedAbilityComponent.IsValid()) return;

	// 2) 通过 Reliable Server RPC 向服务器发送授权请求
	ServerTryAuthorizeAbility(
		CachedAbilityComponent.Get(),
		TriggerTag,
		InputActionValue
	);
}

void USingularisGeneralAbilityAnimusComponent::HandleAbilityRevoke(const FGameplayTag TriggerTag)
{
	// 1) 仅本地控制器处理输入；缓存组件有效时才发送 RPC
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;
	if (!CachedAbilityComponent.IsValid()) return;

	// 2) 通过 Reliable Server RPC 向服务器发送触发结束信号
	ServerTryRevokeTrigger(
		CachedAbilityComponent.Get(),
		TriggerTag
	);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void USingularisGeneralAbilityAnimusComponent::OnPossessPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// 1) 仅在切换到有效 Pawn 时刷新 AbilityComponent 缓存
	if (!IsValid(NewPawn)) return;

	RefreshAbilityComponent();
}

// ReSharper restore CppMemberFunctionMayBeConst

void USingularisGeneralAbilityAnimusComponent::BindInput()
{
	// 1) 卫语句：仅本地控制器可绑定输入
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;

	// 2) 卫语句：Owner 未启用 EnhancedInput 时无法绑定
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(
		OwnerPlayerController->InputComponent
	);
	if (!IsValid(EnhancedInputComponent))
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Warning,
			TEXT("[%s] BindInput：EnhancedInputComponent 无效，输入不可用"),
			*GetNameSafe(GetOwner())
		);
		return;
	}

	// 3) 遍历 AbilityAnimusInputs 配置，按输入相位绑定回调
	for (const auto& [InputAction, TriggerTag] : AbilityAnimusInputs)
	{
		if (!IsValid(InputAction) || !TriggerTag.IsValid()) continue;

		// 4) 按下相位：始终发送授权信号
		EnhancedInputComponent->BindAction(
			InputAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::HandleAbilityAuthorize,
			TriggerTag
		);

		// 5) 完成相位：发送触发结束信号
		EnhancedInputComponent->BindAction(
			InputAction,
			ETriggerEvent::Completed,
			this,
			&ThisClass::HandleAbilityRevoke,
			TriggerTag
		);

		EnhancedInputComponent->BindAction(
			InputAction,
			ETriggerEvent::Canceled,
			this,
			&ThisClass::HandleAbilityRevoke,
			TriggerTag
		);
	}
}

void USingularisGeneralAbilityAnimusComponent::RefreshInput() const
{
	// 1) 卫语句：仅本地控制器持有 EnhancedInput 子系统
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;

	// 2) 卫语句：未配置映射上下文时无需处理
	if (!IsValid(InputMappingContext)) return;

	// 3) 获取本地玩家的 EnhancedInput 子系统
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		OwnerPlayerController->GetLocalPlayer()
	);
	if (!IsValid(Subsystem))
	{
		UE_LOG(
			LogSingularisGeneralAbility,
			Warning,
			TEXT("[%s] RefreshInput：EnhancedInput 本地玩家子系统无效"),
			*GetNameSafe(GetOwner())
		);
		return;
	}

	// 4) 根据启用状态添加或移除 InputMappingContext
	if (bIsEnabled)
		Subsystem->AddMappingContext(InputMappingContext, InputPriority);
	else
		Subsystem->RemoveMappingContext(InputMappingContext);
}

void USingularisGeneralAbilityAnimusComponent::SetEnabled(const bool IsEnabled)
{
	// 1) 本地玩家检查
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;

	// 2) 幂等性：状态未变化则直接返回
	if (bIsEnabled == IsEnabled) return;

	// 3) 捕获旧状态后写入新状态
	const bool OldIsEnabled = bIsEnabled;
	bIsEnabled = IsEnabled;

	// 4) 响应式编程：应用副作用
	ApplyInEnabled(OldIsEnabled);

	UE_LOG(
		LogSingularisGeneralAbility,
		Verbose,
		TEXT("[%s] SetEnabled：本地输入 %s"),
		*GetNameSafe(GetOwner()),
		IsEnabled ? TEXT("启用") : TEXT("禁用")
	);
}

void USingularisGeneralAbilityAnimusComponent::ApplyInEnabled(bool OldIsEnabled) const
{
	// 输入映射上下文随启用状态注册或移除；旧状态仅用于响应式对比语义，实际刷新读取当前状态
	RefreshInput();
}

void USingularisGeneralAbilityAnimusComponent::SetAbilityComponent(
	USingularisGeneralAbilityComponent* AbilityComponent
)
{
	// 1) 本地玩家检查
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;

	// 2) 幂等性：状态未变化则直接返回
	if (CachedAbilityComponent.Get() == AbilityComponent) return;

	// 3) 捕获旧状态后写入新状态
	const USingularisGeneralAbilityComponent* OldAbilityComponent = CachedAbilityComponent.Get();
	CachedAbilityComponent = AbilityComponent;

	// 4) 响应式编程：应用副作用
	ApplyAbilityComponent(OldAbilityComponent);

	UE_LOG(
		LogSingularisGeneralAbility,
		Verbose,
		TEXT("[%s] SetAbilityComponent：受控能力组件 %s → %s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(OldAbilityComponent),
		*GetNameSafe(AbilityComponent)
	);
}

void USingularisGeneralAbilityAnimusComponent::ApplyAbilityComponent(
	const USingularisGeneralAbilityComponent* OldAbilityComponent
) const
{
	// 输入映射上下文随受控状态注册或移除；旧引用仅用于响应式对比语义，实际刷新读取当前状态
	RefreshInput();
}

void USingularisGeneralAbilityAnimusComponent::RefreshAbilityComponent()
{
	// 1) 卫语句：本地控制器未就绪时无法解析 Pawn
	if (!OwnerPlayerController.IsValid()) return;

	// 1) 从当前 Possess 的 Pawn 上查找 USingularisGeneralAbilityComponent 并缓存
	const APawn* PossessedPawn = OwnerPlayerController->GetPawn();
	USingularisGeneralAbilityComponent* AbilityComponent =
		IsValid(PossessedPawn) ? PossessedPawn->FindComponentByClass<USingularisGeneralAbilityComponent>() : nullptr;

	// 2) 幂等性：状态未变化则直接返回
	if (CachedAbilityComponent.Get() == AbilityComponent) return;

	// 3) 捕获旧状态后写入新状态
	const USingularisGeneralAbilityComponent* OldAbilityComponent = CachedAbilityComponent.Get();
	CachedAbilityComponent = AbilityComponent;

	// 4) 响应式编程：应用副作用
	ApplyAbilityComponent(OldAbilityComponent);
}

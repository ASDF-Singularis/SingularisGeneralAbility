#include "Components/SingularisGeneralAbilityAnimusComponent.h"

#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include <InputMappingContext.h>
#include <Engine/LocalPlayer.h>
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>
#include <Net/UnrealNetwork.h>
#include <UObject/ConstructorHelpers.h>

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
	{
		InputMappingContext = DefaultInputMappingContextFinder.Object;
	}

	if (DefaultAbilityActionFinder.Succeeded())
	{
		AbilityAnimusInputs.Add(
			{
				DefaultAbilityActionFinder.Object,
				SingularisGeneral_Ability_Default
			}
		);
	}
}

void USingularisGeneralAbilityAnimusComponent::BeginPlay()
{
	Super::BeginPlay();

	// 1) 确保组件的 Owner 是 PlayerController
	checkf(
		GetOwner()->IsA<APlayerController>(),
		TEXT("SingularisGeneralAbilityAnimusComponent: Owner is not a PlayerController")
	);

	OwnerPlayerController = Cast<APlayerController>(GetOwner());

	// 2) 输入绑定与启用仅对本地控制器执行
	if (OwnerPlayerController.IsValid() && OwnerPlayerController->IsLocalController())
	{
		BindInputAction();

		if (bStartEnabled)
		{
			SetEnabled(true);
		}

		if (bAutoControl)
		{
			// 3) 监听 PossessPawnChanged 以自动刷新缓存的 AbilityComponent
			OwnerPlayerController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessPawnChanged);

			RefreshAbilityComponent();
		}
	}
}

void USingularisGeneralAbilityAnimusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 1) 注销 PossessPawnChanged 委托绑定
	if (OwnerPlayerController.IsValid())
	{
		OwnerPlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessPawnChanged);
	}

	// 2) 清理缓存引用
	CachedAbilityComponent = nullptr;
	OwnerPlayerController.Reset();

	Super::EndPlay(EndPlayReason);
}

void USingularisGeneralAbilityAnimusComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void USingularisGeneralAbilityAnimusComponent::Enabled()
{
	SetEnabled(true);
}

void USingularisGeneralAbilityAnimusComponent::Disabled()
{
	SetEnabled(false);
}

void USingularisGeneralAbilityAnimusComponent::SetEnabled(const bool bInEnabled)
{
	// 1) 仅本地控制器可变更输入启用状态
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;

	// 2) 幂等性：状态未变化则直接返回
	if (bIsEnabled == bInEnabled) return;
	bIsEnabled = bInEnabled;

	// 3) 响应式编程，触发`刷新 InputMappingContext 的添加/移除`副作用
	RefreshInputMappingContext();
}

void USingularisGeneralAbilityAnimusComponent::BindInputAction()
{
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(
		OwnerPlayerController->InputComponent
	);
	if (!IsValid(EnhancedInputComponent)) return;

	// 1) 遍历 AbilityAnimusInputs 配置，将每个 InputAction 绑定至 HandleAbilityInput 回调
	for (const auto& [InputAction, Tag] : AbilityAnimusInputs)
	{
		if (!IsValid(InputAction) || !Tag.IsValid()) continue;

		EnhancedInputComponent->BindAction(
			InputAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::HandleAbilityInput,
			Tag
		);
	}
}

void USingularisGeneralAbilityAnimusComponent::RefreshInputMappingContext() const
{
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;
	if (!IsValid(InputMappingContext)) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		OwnerPlayerController->GetLocalPlayer()
	);
	if (!IsValid(Subsystem)) return;

	// 1) 根据启用状态添加或移除 InputMappingContext
	if (bIsEnabled)
	{
		Subsystem->AddMappingContext(InputMappingContext, InputPriority);
	}
	else
	{
		Subsystem->RemoveMappingContext(InputMappingContext);
	}
}

void USingularisGeneralAbilityAnimusComponent::RefreshAbilityComponent()
{
	if (!OwnerPlayerController.IsValid()) return;

	// 1) 从当前 Possess 的 Pawn 上查找 USingularisGeneralAbilityComponent 并缓存
	const APawn* PossessedPawn = OwnerPlayerController->GetPawn();
	CachedAbilityComponent = IsValid(PossessedPawn)
		                         ? PossessedPawn->FindComponentByClass<USingularisGeneralAbilityComponent>()
		                         : nullptr;
}

// ReSharper disable CppMemberFunctionMayBeConst
void USingularisGeneralAbilityAnimusComponent::HandleAbilityInput(
	const FInputActionValue& InputActionValue,
	const FGameplayTag GameplayTag
)
{
	// 1) 仅本地控制器处理输入；缓存组件有效时才发送 RPC
	if (!OwnerPlayerController.IsValid() || !OwnerPlayerController->IsLocalController()) return;
	if (!CachedAbilityComponent.IsValid()) return;

	// 2) 通过 Reliable Server RPC 向服务器发送能力激活请求
	ServerTryActivateAbility(
		CachedAbilityComponent.Get(),
		GameplayTag,
		InputActionValue
	);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void USingularisGeneralAbilityAnimusComponent::OnPossessPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// 1) 仅在切换到有效 Pawn 时刷新 AbilityComponent 缓存
	if (!IsValid(NewPawn)) return;

	RefreshAbilityComponent();
}

void USingularisGeneralAbilityAnimusComponent::ServerTryActivateAbility_Implementation(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& AbilityTag,
	const FInputActionValue& InputActionValue
)
{
	// 1) 服务器收到 RPC 后，将请求转发至 AbilityComponent 的 TryActivateAbility（HasAuthority 内部已保证）
	if (!IsValid(AbilityComponent)) return;
	if (!OwnerPlayerController.IsValid()) return;

	AbilityComponent->TryActivateAbility(AbilityTag, OwnerPlayerController.Get(), InputActionValue);
}

bool USingularisGeneralAbilityAnimusComponent::ServerTryActivateAbility_Validate(
	USingularisGeneralAbilityComponent* AbilityComponent,
	const FGameplayTag& AbilityTag,
	const FInputActionValue& InputActionValue
)
{
	// 1) 仅校验 AbilityComponent 有效性，防止客户端传入无效指针
	return IsValid(AbilityComponent);
}

// ReSharper restore CppMemberFunctionMayBeConst

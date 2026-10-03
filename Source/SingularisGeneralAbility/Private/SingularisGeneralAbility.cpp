#include "SingularisGeneralAbility.h"

DEFINE_LOG_CATEGORY(LogSingularisGeneralAbility);

#define LOCTEXT_NAMESPACE "FSingularisGeneralAbilityModule"

void FSingularisGeneralAbilityModule::StartupModule()
{
	UE_LOG(LogSingularisGeneralAbility, Display, TEXT("StartupModule：通用能力模块初始化完成"));
}

void FSingularisGeneralAbilityModule::ShutdownModule()
{
	UE_LOG(LogSingularisGeneralAbility, Display, TEXT("ShutdownModule：通用能力模块卸载"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSingularisGeneralAbilityModule, SingularisGeneralAbility)

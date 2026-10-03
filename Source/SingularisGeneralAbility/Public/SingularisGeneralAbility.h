#pragma once

#include <Logging/LogMacros.h>
#include <Modules/ModuleManager.h>

DECLARE_LOG_CATEGORY_EXTERN(LogSingularisGeneralAbility, Log, All);

/**
 * 引力奇点通用能力运行时模块。
 *
 * 插件入口：托管模块生命周期，能力逻辑由能力组件与能力子对象承载。
 */
class FSingularisGeneralAbilityModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

#pragma once

#include <CoreMinimal.h>
#include <IAssetTypeActions.h>
#include <Modules/ModuleManager.h>

class IAssetTools;

DECLARE_LOG_CATEGORY_EXTERN(LogSingularisGeneralAbilityEditor, Log, All);

/**
 * 引力奇点通用能力编辑器模块。
 *
 * 向资产工具注册"Singularis"资产分类，并登记通用能力资产类型行为；卸载时反注册。
 */
class FSingularisGeneralAbilityEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 已登记的资产类型行为，供卸载时反注册。 */
	TArray<TSharedPtr<IAssetTypeActions>> CreatedAssetTypeActions{};

	/** 登记资产类型行为并留存引用。 */
	void RegisterAssetTypeAction(IAssetTools& AssetTools, const TSharedRef<IAssetTypeActions>& Action);
};

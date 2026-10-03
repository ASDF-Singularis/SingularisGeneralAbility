#include "SingularisGeneralAbilityEditor.h"

#include <AssetToolsModule.h>

#include "Factories/SingularisGeneralAbilityFactory.h"

DEFINE_LOG_CATEGORY(LogSingularisGeneralAbilityEditor);

#define LOCTEXT_NAMESPACE "FSingularisGeneralAbilityEditorModule"

void FSingularisGeneralAbilityEditorModule::StartupModule()
{
	// 1) 登记 Singularis 资产分类
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const EAssetTypeCategories::Type SingularisPluginCategory = AssetTools.RegisterAdvancedAssetCategory(
		FName("Singularis"),
		LOCTEXT("SingularisCategory", "Singularis")
	);

	// 2) 登记通用能力资产行为
	RegisterAssetTypeAction(
		AssetTools,
		MakeShareable(new FAssetTypeActions_SingularisGeneralAbility(SingularisPluginCategory))
	);

	UE_LOG(
		LogSingularisGeneralAbilityEditor,
		Display,
		TEXT("StartupModule：编辑器模块初始化完成，已登记 %d 项资产类型行为"),
		CreatedAssetTypeActions.Num()
	);
}

void FSingularisGeneralAbilityEditorModule::ShutdownModule()
{
	// 1) 编辑器关闭时 AssetTools 可能已卸载，需先检查再反注册
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for (const auto& Action : CreatedAssetTypeActions)
			AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
	}

	UE_LOG(
		LogSingularisGeneralAbilityEditor,
		Display,
		TEXT("ShutdownModule：编辑器模块卸载，已反注册 %d 项资产类型行为"),
		CreatedAssetTypeActions.Num()
	);

	CreatedAssetTypeActions.Empty();
}

void FSingularisGeneralAbilityEditorModule::RegisterAssetTypeAction(
	IAssetTools& AssetTools,
	const TSharedRef<IAssetTypeActions>& Action
)
{
	// 1) 登记资产行为并留存引用，供卸载时反注册
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSingularisGeneralAbilityEditorModule, SingularisGeneralAbilityEditor)

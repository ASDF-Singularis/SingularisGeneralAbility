#include "SingularisGeneralAbilityEditor.h"

#include <AssetToolsModule.h>

#include "Factories/SingularisGeneralAbilityFactory.h"

#define LOCTEXT_NAMESPACE "FSingularisGeneralAbilityEditorModule"

void FSingularisGeneralAbilityEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const EAssetTypeCategories::Type SingularisPluginCategory = AssetTools.RegisterAdvancedAssetCategory(
		FName("Singularis"),
		LOCTEXT("SingularisCategory", "Singularis")
	);

	RegisterAssetTypeAction(
		AssetTools,
		MakeShareable(new FAssetTypeActions_SingularisGeneralAbility(SingularisPluginCategory))
	);
}

void FSingularisGeneralAbilityEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		for (const auto& Action : CreatedAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
		}
	}

	CreatedAssetTypeActions.Empty();
}

void FSingularisGeneralAbilityEditorModule::RegisterAssetTypeAction(
	IAssetTools& AssetTools,
	const TSharedRef<IAssetTypeActions>& Action
)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSingularisGeneralAbilityEditorModule, SingularisGeneralAbilityEditor)

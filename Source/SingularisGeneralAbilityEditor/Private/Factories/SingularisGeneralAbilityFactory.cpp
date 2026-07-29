#include "Factories/SingularisGeneralAbilityFactory.h"

#include <Kismet2/KismetEditorUtilities.h>
#include <Objects/SingularisGeneralAbilityBase.h>

USingularisGeneralAbilityFactory::USingularisGeneralAbilityFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = USingularisGeneralAbility::StaticClass();
}

UObject* USingularisGeneralAbilityFactory::FactoryCreateNew(
	UClass* InClass,
	UObject* InParent,
	const FName InName,
	const EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn
)
{
	// 1) 利用 KismetEditorUtilities 自动生成蓝图资产
	// 2) 强制将其基类指派为最新的通用能力基础类 USingularisGeneralAbility
	return FKismetEditorUtilities::CreateBlueprint(
		USingularisGeneralAbility::StaticClass(),
		InParent,
		InName,
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		NAME_None
	);
}

bool USingularisGeneralAbilityFactory::ShouldShowInNewMenu() const
{
	return true;
}

UClass* FAssetTypeActions_SingularisGeneralAbility::GetSupportedClass() const
{
	return USingularisGeneralAbility::StaticClass();
}

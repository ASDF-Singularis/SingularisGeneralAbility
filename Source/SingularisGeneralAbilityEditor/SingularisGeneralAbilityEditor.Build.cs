using UnrealBuildTool;

public class SingularisGeneralAbilityEditor : ModuleRules
{
	public SingularisGeneralAbilityEditor(ReadOnlyTargetRules target) : base(target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			[
				"Core",
				"CoreUObject",
				"Engine",

				"SingularisGeneralAbility",

				"UMG",
				"UMGEditor",
				"UnrealEd",
				"AssetTools",
				"ContentBrowser"
			]
		);
	}
}
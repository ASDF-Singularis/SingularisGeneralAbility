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
				"Projects",

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
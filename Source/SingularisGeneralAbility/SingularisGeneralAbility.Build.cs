using UnrealBuildTool;

public class SingularisGeneralAbility : ModuleRules
{
	public SingularisGeneralAbility(ReadOnlyTargetRules target) : base(target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(["EnhancedInput"]);

		PrivateDependencyModuleNames.AddRange(
			[
				"Core",
				"CoreUObject",
				"Engine",
				"NetCore",

				"InputCore",
				"EnhancedInput",

				"GameplayTags"
			]
		);
	}
}
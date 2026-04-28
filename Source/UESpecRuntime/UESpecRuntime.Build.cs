using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class UESpecRuntime : ModuleRules
	{
		public UESpecRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new[]
				{
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"InputCore",
					"Json",
					"UMG",
					"Slate",
					"SlateCore",
					"CommonUI",
					"ModelViewViewModel"
				});

			PrivateDependencyModuleNames.AddRange(
				new[]
				{
					"AngelscriptCode",
					"FieldNotification"
				});
		}
	}
}

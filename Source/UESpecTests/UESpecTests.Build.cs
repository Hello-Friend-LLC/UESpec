using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class UESpecTests : ModuleRules
	{
		public UESpecTests(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			// Test files each define same-named helpers in anonymous namespaces; unity blobs merge them.
			bUseUnity = false;

			PrivateDependencyModuleNames.AddRange(
				new[]
				{
					"Core",
					"CoreUObject",
					"AngelscriptCode",
					"Engine",
					"ApplicationCore",
					"BlueprintGraph",
					"InputCore",
					"Json",
					"ModelViewViewModelBlueprint",
					"Slate",
					"SlateCore",
					"UMG",
					"UMGEditor",
					"UnrealEd",
					"UESpecRuntime",
					"UESpecEditor"
				});
		}
	}
}

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class UESpecEditor : ModuleRules
	{
		public UESpecEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new[]
				{
					"Core",
					"CoreUObject",
					"AssetRegistry",
					"AssetTools",
					"BlueprintGraph",
					"Blutility",
					"DeveloperSettings",
					"EditorSubsystem",
					"Engine",
					"UnrealEd",
					"Json",
					"JsonUtilities",
					"UMG",
					"Slate",
					"SlateCore",
					"SlateNullRenderer",
					"CommonUI",
					"ModelViewViewModel",
					"ModelViewViewModelBlueprint",
					"ModelViewViewModelEditor",
					"UMGEditor",
					"UESpecRuntime"
				});
		}
	}
}

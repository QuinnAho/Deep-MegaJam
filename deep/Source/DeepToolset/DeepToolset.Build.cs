using UnrealBuildTool;

public class DeepToolset : ModuleRules
{
	public DeepToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Deep" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "Json", "ToolsetRegistry" });
	}
}

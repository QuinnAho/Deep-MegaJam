// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Deep : ModuleRules
{
	public Deep(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets code include by folder from the module root, e.g. "Sim/DeepRules.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings", "EnhancedInput", "AIModule" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "AudioExtensions", "ProceduralMeshComponent" });
	}
}

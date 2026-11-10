using UnrealBuildTool;

public class OrbitBreakout : ModuleRules
{
    public OrbitBreakout(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule",
            "NavigationSystem", "GameplayTasks", "UMG", "Slate", "SlateCore", "AnimGraphRuntime"
        });
        if (Target.bBuildEditor)
        {
            // The editor-only integration test owns its PIE lifecycle.
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}

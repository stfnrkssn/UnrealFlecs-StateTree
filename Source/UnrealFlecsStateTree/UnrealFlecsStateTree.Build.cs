using UnrealBuildTool;

public class UnrealFlecsStateTree : ModuleRules
{
    public UnrealFlecsStateTree(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealFlecs",
            "FlecsLibrary",
            "StateTreeModule"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "FlecsLibrary",
            "GameplayStateTreeModule"
        });
    }
}

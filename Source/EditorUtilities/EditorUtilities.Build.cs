using UnrealBuildTool;

public class MyPluginEditor : ModuleRules
{
    public MyPluginEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 
        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",        // For editor functionalities
                "Slate",
                "SlateCore",
                "ZoneGraph"
            });
        }
    }
}

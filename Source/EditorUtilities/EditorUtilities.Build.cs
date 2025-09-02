using UnrealBuildTool;

public class EditorUtilities : ModuleRules
{
    public EditorUtilities(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd", 
            "PropertyEditor",
            "Slate",
            "SlateCore",
            "InputCore",
            "PropertyEditor",
            "ZoneGraph",
            "ZoneGraphEditor"
        });
    }
}

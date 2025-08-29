using UnrealBuildTool;

public class EditorUtilities : ModuleRules
{
    public EditorUtilities(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "Slate",
            "SlateCore",
            "PropertyEditor",
            "ZoneGraph"
        });
    }
}

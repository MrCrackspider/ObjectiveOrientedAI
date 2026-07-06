using UnrealBuildTool;

public class ObjectiveOrientedAIEditor : ModuleRules
{
    public ObjectiveOrientedAIEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "ObjectiveOrientedAI", "KismetCompiler", "UnrealEd" });
        PublicDependencyModuleNames.AddRange(new string[] { "BlueprintGraph" });
    }
}
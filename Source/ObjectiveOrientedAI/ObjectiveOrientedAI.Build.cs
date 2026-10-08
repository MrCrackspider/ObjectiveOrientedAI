using UnrealBuildTool;

public class ObjectiveOrientedAI : ModuleRules
{
    public ObjectiveOrientedAI(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
        PublicDependencyModuleNames.AddRange(new string[] { "GameplayTags", "GameplayDebugger", "AIModule", "GameplayTasks", "NavigationSystem", "DeveloperSettings" });
    }
}
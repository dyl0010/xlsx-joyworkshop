using UnrealBuildTool;

public class GangstarLocalDebug : ModuleRules
{
    public GangstarLocalDebug(ReadOnlyTargetRules Target) : base(Target)
    {
        DefaultBuildSettings = BuildSettingsVersion.Latest;

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        bool bCanUseLocalImGui = Target.Configuration != UnrealTargetConfiguration.Shipping
            && Target.Configuration != UnrealTargetConfiguration.Test
            && Target.Type != TargetRules.TargetType.Server;

        if (bCanUseLocalImGui)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "CoreUObject",
                    "Engine",
                    "Gangstar",
                    "GameplayAbilities",
                    "GameplayTags",
                    "GSTMFramework",
                    "ImGui",
                    "InputCore",
                    "OnlineCommons",
                    "Slate",
                    "SlateCore",
                }
            );

            PublicDefinitions.Add("WITH_GANGSTAR_LOCAL_IMGUI=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_GANGSTAR_LOCAL_IMGUI=0");
        }
    }
}

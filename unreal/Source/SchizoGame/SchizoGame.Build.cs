// SchizoGame.Build.cs — the game module: thin by design. The world lives in
// the SimRuntime plugin (kernel C API); this module is the game's own glue.
using UnrealBuildTool;

public class SchizoGame : ModuleRules
{
	public SchizoGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"SimRuntime",
			"ProceduralMeshComponent",
			"Slate",
			"SlateCore",
			"InputCore"
		});
	}
}

// SchizoGameGame.Target.cs — the game (shipping-path) target.
using UnrealBuildTool;
using System.Collections.Generic;

public class SchizoGameGameTarget : TargetRules
{
	public SchizoGameGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		BuildEnvironment = TargetBuildEnvironment.Shared;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SchizoGame");
	}
}

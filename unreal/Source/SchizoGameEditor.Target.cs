// SchizoGameEditor.Target.cs — the editor target (Phase 3 bring-up).
using UnrealBuildTool;
using System.Collections.Generic;

public class SchizoGameEditorTarget : TargetRules
{
	public SchizoGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		BuildEnvironment = TargetBuildEnvironment.Shared;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SchizoGame");
	}
}

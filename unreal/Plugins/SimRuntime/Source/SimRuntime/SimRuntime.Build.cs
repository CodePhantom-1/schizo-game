// SimRuntime.Build.cs — PLACEHOLDER (authored pre-editor; bring-up fixes expected).
// Consumes the kernel built by CMake: kernel/build/libsim_core.a + kernel/include.
// The kernel is the product's deterministic core (plan v2 §5); the engine renders it.
using UnrealBuildTool;

public class SimRuntime : ModuleRules
{
	public SimRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add("../../kernel/include");

		// The CMake-built kernel static library (Release). Built by:
		//   cmake -S kernel -B kernel/build -DCMAKE_BUILD_TYPE=Release && cmake --build kernel/build
		// TODO(Phase 3 bring-up): verify the artifact name/path per configuration,
		// and decide static-link vs shared once the editor's module lifetime is known.
		AdditionalLibraries.Add("../../../kernel/build/libsim_core.a");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine"
		});
	}
}
